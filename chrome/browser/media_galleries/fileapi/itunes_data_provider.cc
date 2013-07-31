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
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "chrome/common/media_galleries/itunes_library.h"
#include "content/public/browser/browser_thread.h"

using chrome::MediaFileSystemBackend;

namespace itunes {

namespace {

typedef base::Callback<void(scoped_ptr<base::FilePathWatcher> watcher)>
    FileWatchStartedCallback;

ITunesDataProvider::Album MakeUniqueTrackNames(const parser::Album& album) {
  // TODO(vandebo): It would be nice to ensure that names returned from here
  // are stable, but aside from persisting every name returned, it's not
  // obvious how to do that (without including the track id in every name).
  typedef std::set<const parser::Track*> TrackRefs;
  typedef std::map<ITunesDataProvider::TrackName, TrackRefs> AlbumInfo;

  ITunesDataProvider::Album result;
  AlbumInfo duped_tracks;

  parser::Album::const_iterator album_it;
  for (album_it = album.begin(); album_it != album.end(); ++album_it) {
    const parser::Track& track = *album_it;
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
  MediaFileSystemBackend::MediaTaskRunner()->PostTask(
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
  MediaFileSystemBackend::MediaTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(watch_started_callback, base::Passed(&watcher)));
}

}  // namespace

ITunesDataProvider::ITunesDataProvider(const base::FilePath& library_path)
    : library_path_(library_path),
      needs_refresh_(true),
      is_valid_(false) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
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

// TODO(vandebo): add a file watch that resets |needs_refresh_| when the
// file changes.
void ITunesDataProvider::RefreshData(const ReadyCallback& ready_callback) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  if (!needs_refresh_) {
    ready_callback.Run(is_valid_);
    return;
  }

  needs_refresh_ = false;
  xml_parser_ = new SafeITunesLibraryParser(
      library_path_,
      base::Bind(&ITunesDataProvider::OnLibraryParsedCallback, ready_callback));
  xml_parser_->Start();
}

const base::FilePath& ITunesDataProvider::library_path() const {
  return library_path_;
}

bool ITunesDataProvider::KnownArtist(const ArtistName& artist) const {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(is_valid_);
  return ContainsKey(library_, artist);
}

bool ITunesDataProvider::KnownAlbum(const ArtistName& artist,
                                    const AlbumName& album) const {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(is_valid_);
  Library::const_iterator library_it = library_.find(artist);
  if (library_it == library_.end())
    return false;
  return ContainsKey(library_it->second, album);
}

base::FilePath ITunesDataProvider::GetTrackLocation(
    const ArtistName& artist, const AlbumName& album,
    const TrackName& track) const {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
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
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
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
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
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
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(is_valid_);
  Album result;
  Library::const_iterator artist_lookup = library_.find(artist);
  if (artist_lookup != library_.end()) {
    Artist::const_iterator album_lookup = artist_lookup->second.find(album);
    if (album_lookup != artist_lookup->second.end())
      result = album_lookup->second;
  }
  return result;
}

// static
void ITunesDataProvider::OnLibraryWatchStartedCallback(
    scoped_ptr<base::FilePathWatcher> library_watcher) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  ITunesDataProvider* provider =
      chrome::ImportedMediaGalleryRegistry::ITunesDataProvider();
  if (provider)
    provider->OnLibraryWatchStarted(library_watcher.Pass());
}

// static
void ITunesDataProvider::OnLibraryChangedCallback(const base::FilePath& path,
                                                  bool error) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  ITunesDataProvider* provider =
      chrome::ImportedMediaGalleryRegistry::ITunesDataProvider();
  if (provider)
    provider->OnLibraryChanged(path, error);
}

// static
void ITunesDataProvider::OnLibraryParsedCallback(
    const ReadyCallback& ready_callback,
    bool result,
    const parser::Library& library) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  ITunesDataProvider* provider =
      chrome::ImportedMediaGalleryRegistry::ITunesDataProvider();
  if (!provider) {
    ready_callback.Run(false);
    return;
  }
  provider->OnLibraryParsed(ready_callback, result, library);
}

void ITunesDataProvider::OnLibraryWatchStarted(
    scoped_ptr<base::FilePathWatcher> library_watcher) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  library_watcher_.reset(library_watcher.release());
}

void ITunesDataProvider::OnLibraryChanged(const base::FilePath& path,
                                          bool error) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK_EQ(library_path_.value(), path.value());
  if (error)
    LOG(ERROR) << "Error watching " << library_path_.value();
  needs_refresh_ = true;
}

void ITunesDataProvider::OnLibraryParsed(const ReadyCallback& ready_callback,
                                         bool result,
                                         const parser::Library& library) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  is_valid_ = result;
  if (is_valid_) {
    library_.clear();
    for (parser::Library::const_iterator artist_it = library.begin();
         artist_it != library.end();
         ++artist_it) {
      for (parser::Albums::const_iterator album_it = artist_it->second.begin();
           album_it != artist_it->second.end();
           ++album_it) {
        library_[artist_it->first][album_it->first] =
            MakeUniqueTrackNames(album_it->second);
      }
    }
  }
  ready_callback.Run(is_valid_);
}

}  // namespace itunes
