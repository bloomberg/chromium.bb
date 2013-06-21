// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/itunes_data_provider.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/media_galleries/fileapi/itunes_library_parser.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_mount_point_provider.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "content/public/browser/browser_thread.h"

using chrome::MediaFileSystemMountPointProvider;

namespace itunes {

namespace {

// A "reasonable" artificial limit.
// TODO(vandebo): Add a UMA to figure out what common values are.
const int64 kMaxLibraryFileSize = 150 * 1024 * 1024;

typedef base::Callback<void(scoped_ptr<base::FilePathWatcher> watcher)>
    FileWatchStartedCallback;

std::string ReadFile(const base::FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();

  base::PlatformFile file = base::CreatePlatformFile(
      path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ, NULL, NULL);
  if (file == base::kInvalidPlatformFileValue)
    return std::string();

  base::PlatformFileInfo file_info;
  if (!base::GetPlatformFileInfo(file, &file_info) ||
      file_info.size > kMaxLibraryFileSize) {
    base::ClosePlatformFile(file);
    return std::string();
  }

  std::string result(file_info.size, 0);
  if (base::ReadPlatformFile(file, 0, string_as_array(&result),
        file_info.size) != file_info.size) {
    result.clear();
  }

  base::ClosePlatformFile(file);
  return result;
}

ITunesDataProvider::Album MakeUniqueTrackNames(
    const ITunesLibraryParser::Album& album) {
  // TODO(vandebo): It would be nice to ensure that names returned from here
  // are stable, but aside from persisting every name returned, it's not
  // obvious how to do that (without including the track id in every name).
  typedef std::set<const ITunesLibraryParser::Track*> TrackRefs;
  typedef std::map<ITunesDataProvider::TrackName, TrackRefs> AlbumInfo;

  ITunesDataProvider::Album result;
  AlbumInfo duped_tracks;

  ITunesLibraryParser::Album::const_iterator album_it;
  for (album_it = album.begin(); album_it != album.end(); ++album_it) {
    const ITunesLibraryParser::Track& track = *album_it;
    std::string name = track.location.BaseName().AsUTF8Unsafe();
    duped_tracks[name].insert(&track);
  }

  for (AlbumInfo::const_iterator name_it = duped_tracks.begin();
       name_it != duped_tracks.end();
       ++name_it) {
    const TrackRefs& track_refs = name_it->second;
    if (track_refs.size() == 1) {
      result[name_it->first] = (*track_refs.begin())->location;
    } else {
      for (TrackRefs::const_iterator track_it = track_refs.begin();
           track_it != track_refs.end();
           ++track_it) {
        std::string id =
            base::StringPrintf(" (%" PRId64 ")", (*track_it)->id);
        base::FilePath unique_name =
            (*track_it)->location.BaseName().InsertBeforeExtensionASCII(id);
        result[unique_name.AsUTF8Unsafe()] = (*track_it)->location;
      }
    }
  }

  return result;
}

// Bounces |path| and |error| to |callback| from the FILE thread to the media
// task runner.
void OnLibraryChanged(const base::FilePathWatcher::Callback& callback,
                      const base::FilePath& path,
                      bool error) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  MediaFileSystemMountPointProvider::MediaTaskRunner()->PostTask(
      FROM_HERE, base::Bind(callback, path, error));
}

// The watch has to be started on the FILE thread, and the callback called by
// the FilePathWatcher also needs to run on the FILE thread.
void StartLibraryWatchOnFileThread(
    const base::FilePath& library_path,
    const FileWatchStartedCallback& watch_started_callback,
    const base::FilePathWatcher::Callback& library_changed_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  scoped_ptr<base::FilePathWatcher> watcher(new base::FilePathWatcher);
  bool success = watcher->Watch(
      library_path, false /*recursive*/,
      base::Bind(&OnLibraryChanged, library_changed_callback));
  if (!success)
    LOG(ERROR) << "Adding watch for " << library_path.value() << " failed";
  MediaFileSystemMountPointProvider::MediaTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(watch_started_callback, base::Passed(&watcher)));
}

}  // namespace

ITunesDataProvider::ITunesDataProvider(const base::FilePath& library_path)
    : library_path_(library_path),
      needs_refresh_(true),
      is_valid_(false) {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(!library_path_.empty());

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(StartLibraryWatchOnFileThread,
                 library_path_,
                 base::Bind(&ITunesDataProvider::OnLibraryWatchStartedCallback),
                 base::Bind(&ITunesDataProvider::OnLibraryChangedCallback)));
}

ITunesDataProvider::~ITunesDataProvider() {}

void ITunesDataProvider::RefreshData(
    const base::Callback<void(bool)>& ready_callback) {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  if (needs_refresh_) {
    is_valid_ = ParseLibrary();
    needs_refresh_ = false;
  }
  ready_callback.Run(is_valid_);
}

const base::FilePath& ITunesDataProvider::library_path() const {
  return library_path_;
}

bool ITunesDataProvider::KnownArtist(const ArtistName& artist) const {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(is_valid_);
  return ContainsKey(library_, artist);
}

bool ITunesDataProvider::KnownAlbum(const ArtistName& artist,
                                    const AlbumName& album) const {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(is_valid_);
  Library::const_iterator library_it = library_.find(artist);
  if (library_it == library_.end())
    return false;
  return ContainsKey(library_it->second, album);
}

base::FilePath ITunesDataProvider::GetTrackLocation(
    const ArtistName& artist, const AlbumName& album,
    const TrackName& track) const {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(is_valid_);
  Library::const_iterator library_it = library_.find(artist);
  if (library_it == library_.end())
    return base::FilePath();

  Artist::const_iterator artist_it = library_it->second.find(album);
  if (artist_it == library_it->second.end())
    return base::FilePath();

  Album::const_iterator album_it = artist_it->second.find(track);
  if (album_it == artist_it->second.end())
    return base::FilePath();
  return album_it->second;
}

std::set<ITunesDataProvider::ArtistName>
ITunesDataProvider::GetArtistNames() const {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(is_valid_);
  std::set<ArtistName> result;
  Library::const_iterator it;
  for (it = library_.begin(); it != library_.end(); ++it) {
    result.insert(it->first);
  }
  return result;
}

std::set<ITunesDataProvider::AlbumName> ITunesDataProvider::GetAlbumNames(
    const ArtistName& artist) const {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(is_valid_);
  std::set<AlbumName> result;
  Library::const_iterator artist_lookup = library_.find(artist);
  if (artist_lookup == library_.end())
    return result;

  const Artist& artist_entry = artist_lookup->second;
  Artist::const_iterator it;
  for (it = artist_entry.begin(); it != artist_entry.end(); ++it) {
    result.insert(it->first);
  }
  return result;
}

ITunesDataProvider::Album ITunesDataProvider::GetAlbum(
    const ArtistName& artist, const AlbumName& album) const {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(is_valid_);
  Album empty_result;
  Library::const_iterator artist_lookup = library_.find(artist);
  if (artist_lookup == library_.end())
    return empty_result;

  Artist::const_iterator album_lookup = artist_lookup->second.find(album);
  if (album_lookup == artist_lookup->second.end())
    return empty_result;

  return album_lookup->second;
}

// static
void ITunesDataProvider::OnLibraryWatchStartedCallback(
    scoped_ptr<base::FilePathWatcher> library_watcher) {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  ITunesDataProvider* provider =
      chrome::ImportedMediaGalleryRegistry::ITunesDataProvider();
  if (provider)
    provider->OnLibraryWatchStarted(library_watcher.Pass());
}

// static
void ITunesDataProvider::OnLibraryChangedCallback(const base::FilePath& path,
                                                  bool error) {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  ITunesDataProvider* provider =
      chrome::ImportedMediaGalleryRegistry::ITunesDataProvider();
  if (provider)
    provider->OnLibraryChanged(path, error);
}

bool ITunesDataProvider::ParseLibrary() {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  std::string xml = ReadFile(library_path_);

  library_.clear();
  ITunesLibraryParser parser;
  if (!parser.Parse(xml))
    return false;

  ITunesLibraryParser::Library::const_iterator artist_it;
  ITunesLibraryParser::Albums::const_iterator album_it;
  for (artist_it = parser.library().begin();
       artist_it != parser.library().end();
       ++artist_it) {
    for (album_it = artist_it->second.begin();
         album_it != artist_it->second.end();
         ++album_it) {
      library_[artist_it->first][album_it->first] =
          MakeUniqueTrackNames(album_it->second);
    }
  }
  return true;
}

void ITunesDataProvider::OnLibraryWatchStarted(
    scoped_ptr<base::FilePathWatcher> library_watcher) {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  library_watcher_.reset(library_watcher.release());
}

void ITunesDataProvider::OnLibraryChanged(const base::FilePath& path,
                                          bool error) {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK_EQ(library_path_.value(), path.value());
  if (error)
    LOG(ERROR) << "Error watching " << library_path_.value();
  needs_refresh_ = true;
}

}  // namespace itunes
