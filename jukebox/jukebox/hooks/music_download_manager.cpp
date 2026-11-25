#include "jukebox/hooks/music_download_manager.hpp"

#include <optional>
#include <string>

#include <Geode/Result.hpp>
#include <Geode/binding/MusicDownloadManager.hpp>
#include <Geode/binding/SongInfoObject.hpp>
#include <Geode/utils/general.hpp>
#include <asp/fs/Path.hpp>
#include <asp/fs/fs.hpp>
#include <asp/fs/std.hpp>  // IWYU pragma: keep

#include "jukebox/events/get_song_info.hpp"
#include "jukebox/managers/nong_manager.hpp"
#include "jukebox/nong/nong.hpp"

using namespace jukebox;

// MusicDownloadManager is a singleton, so a plain static replaces m_fields.
// (Geode's Fields need the typeinfo of MusicDownloadManager, which is missing on macOS.)
static bool s_overrideSongInfo = false;

std::string JBMusicDownloadManager::pathForSong(int id) {
    NongManager::get().m_currentlyPreparingNong = std::nullopt;
    std::optional<Nongs*> nongs = NongManager::get().getNongs(id);
    if (!nongs.has_value()) { return MusicDownloadManager::pathForSong(id); }
    Nongs* value = nongs.value();
    Song* active = value->active();
    if (!asp::fs::exists(active->path().value())) {
        return MusicDownloadManager::pathForSong(id);
    }
    NongManager::get().m_currentlyPreparingNong = value;

    return active->path().value().string();
}

void JBMusicDownloadManager::onGetSongInfoCompleted(std::string p1, std::string p2) {
    s_overrideSongInfo = true;
    MusicDownloadManager::onGetSongInfoCompleted(p1, p2);
    geode::Result<int> songIDRes = geode::utils::numFromString<int>(p2);
    if (songIDRes.isErr()) {
        s_overrideSongInfo = false;
        return;
    }

    int songID = songIDRes.unwrap();

    SongInfoObject* obj = this->getSongInfoObject(songID);
    s_overrideSongInfo = false;

    if (obj == nullptr) { return; }

    event::GetSongInfo(songID).send(event::GetSongInfoData {songID, obj->m_songName, obj->m_artistName});
}

SongInfoObject* JBMusicDownloadManager::getSongInfoObject(int id) {
    auto* og = MusicDownloadManager::getSongInfoObject(id);
    if (og == nullptr) { return og; }

    if (s_overrideSongInfo) { return og; }

    std::optional<Nongs*> opt = NongManager::get().getNongs(id);

    if (!opt) { return og; }

    Nongs* res = opt.value();
    Song* active = res->active();

    og->m_songName = active->metadata()->name;
    og->m_artistName = active->metadata()->artist;
    return og;
}
