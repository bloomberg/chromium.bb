// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/iphoto_data_provider.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/fileapi/safe_iapps_library_parser.h"

namespace iphoto {

IPhotoDataProvider::IPhotoDataProvider(const base::FilePath& library_path)
    : iapps::IAppsDataProvider(library_path),
      weak_factory_(this) {}

IPhotoDataProvider::~IPhotoDataProvider() {}

void IPhotoDataProvider::DoParseLibrary(
    const base::FilePath& library_path,
    const ReadyCallback& ready_callback) {
  xml_parser_ = new iapps::SafeIAppsLibraryParser;
  xml_parser_->ParseIPhotoLibrary(
      library_path,
      base::Bind(&IPhotoDataProvider::OnLibraryParsed,
                 weak_factory_.GetWeakPtr(),
                 ready_callback));
}

void IPhotoDataProvider::OnLibraryParsed(const ReadyCallback& ready_callback,
                                         bool result,
                                         const parser::Library& library) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  set_valid(result);
  if (valid())
    BuildIndices(library);
  ready_callback.Run(valid());
}

void IPhotoDataProvider::BuildIndices(const parser::Library& library) {
  typedef base::hash_map<uint64, const base::FilePath*> IdIndex;

  IdIndex photo_id_index;
  IdIndex originals_id_index;
  for (std::set<parser::Photo>::const_iterator photo_it =
           library.all_photos.begin();
       photo_it != library.all_photos.end(); photo_it++) {
    photo_id_index[photo_it->id] = &(photo_it->location);
    if (!photo_it->original_location.empty())
      originals_id_index[photo_it->id] = &(photo_it->original_location);
  }

  // Build up a set of IDs which have in-album duplicates.
  // Those are the filenames we want to globally mangle.
  std::set<uint64> dupe_ids;
  for (parser::Albums::const_iterator album_it = library.albums.begin();
       album_it != library.albums.end(); album_it++) {
    const parser::Album& album = album_it->second;

    std::set<std::string> album_paths;
    for (parser::Album::const_iterator id_it = album.begin();
         id_it != album.end(); id_it++) {
      uint64 id = *id_it;
      IdIndex::const_iterator photo_it = photo_id_index.find(id);
      if (photo_it == photo_id_index.end())
        continue;

      std::string filename = photo_it->second->BaseName().value();
      if (ContainsKey(album_paths, filename))
        dupe_ids.insert(id);
      else
        album_paths.insert(filename);
    }
  }

  // Now build the directory index.
  dir_index_.clear();
  originals_index_.clear();
  for (parser::Albums::const_iterator album_it = library.albums.begin();
       album_it != library.albums.end(); album_it++) {
    std::string album_name = album_it->first;
    const parser::Album& album = album_it->second;

    for (parser::Album::const_iterator id_it = album.begin();
         id_it != album.end(); id_it++) {
      uint64 id = *id_it;
      IdIndex::const_iterator photo_it = photo_id_index.find(id);
      if (photo_it == photo_id_index.end())
        continue;
      base::FilePath path = *(photo_it->second);

      std::string filename = path.BaseName().value();
      if (ContainsKey(dupe_ids, id)) {
        filename = path.BaseName().InsertBeforeExtension(
          "(" + base::Uint64ToString(id) + ")").value();
      }

      dir_index_[album_name][filename] = path;

      IdIndex::const_iterator original_it = originals_id_index.find(id);
      if (original_it != originals_id_index.end())
        originals_index_[album_name][filename] = *(original_it->second);
    }
  }
}

std::vector<std::string> IPhotoDataProvider::GetAlbumNames() const {
  std::vector<std::string> names;

  for (DirIndex::const_iterator dir_it = dir_index_.begin();
       dir_it != dir_index_.end(); dir_it++) {
    names.push_back(dir_it->first);
  }

  return names;
}

std::map<std::string, base::FilePath> IPhotoDataProvider::GetAlbumContents(
    const std::string& album) const {
  std::map<std::string, base::FilePath> locations;
  DirIndex::const_iterator dir_it = dir_index_.find(album);
  if (dir_it == dir_index_.end())
    return locations;

  for (FileIndex::const_iterator file_it = dir_it->second.begin();
       file_it != dir_it->second.end(); file_it++) {
    locations.insert(make_pair(file_it->first, file_it->second));
  }

  return locations;
}

base::FilePath IPhotoDataProvider::GetPhotoLocationInAlbum(
    const std::string& album,
    const std::string& filename) const {
  DirIndex::const_iterator dir_it = dir_index_.find(album);
  if (dir_it == dir_index_.end())
    return base::FilePath();
  FileIndex::const_iterator file_it = dir_it->second.find(filename);
  if (file_it == dir_it->second.end())
    return base::FilePath();
  return file_it->second;
}

bool IPhotoDataProvider::HasOriginals(const std::string& album) const {
  DirIndex::const_iterator originals_it = originals_index_.find(album);
  return originals_it != originals_index_.end();
}

std::map<std::string, base::FilePath> IPhotoDataProvider::GetOriginals(
    const std::string& album) const {
  std::map<std::string, base::FilePath> locations;
  DirIndex::const_iterator originals_it = originals_index_.find(album);
  if (originals_it == originals_index_.end())
    return locations;

  for (FileIndex::const_iterator file_it = originals_it->second.begin();
       file_it != originals_it->second.end(); file_it++) {
    locations.insert(make_pair(file_it->first, file_it->second));
  }

  return locations;
}

base::FilePath IPhotoDataProvider::GetOriginalPhotoLocation(
      const std::string& album,
      const std::string& filename) const {
  DirIndex::const_iterator originals_it = originals_index_.find(album);
  if (originals_it == originals_index_.end())
    return base::FilePath();
  FileIndex::const_iterator file_it = originals_it->second.find(filename);
  if (file_it == originals_it->second.end())
    return base::FilePath();
  return file_it->second;
}

}  // namespace iphoto
