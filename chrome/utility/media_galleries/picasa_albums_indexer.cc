// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/picasa_albums_indexer.h"

#include <vector>

#include "base/ini_parser.h"
#include "base/logging.h"
#include "base/strings/string_split.h"

namespace picasa {

namespace {

const char kAlbumSectionHeader[] = ".album:";
const char kAlbumsKey[] = "albums";

class PicasaINIParser : public base::INIParser {
 public:
  PicasaINIParser(
      const base::FilePath& folder_path, AlbumImagesMap* albums_images)
      : folder_path_(folder_path),
        albums_images_(albums_images) {
  }
  virtual ~PicasaINIParser() {}

 private:
  virtual void HandleTriplet(const std::string& section,
                             const std::string& key,
                             const std::string& value) OVERRIDE {
    if (key != kAlbumsKey)
      return;

    // [.album:*] sections ignored as we get that data from the PMP files.
    if (section.find(kAlbumSectionHeader) == 0)
      return;

    std::vector<std::string> containing_albums;
    base::SplitString(value, ',', &containing_albums);
    for (std::vector<std::string>::iterator it = containing_albums.begin();
         it != containing_albums.end(); ++it) {
      AlbumImagesMap::iterator album_map_it = albums_images_->find(*it);

      // Ignore entry if the album uid is not listed among in |album_uids|
      // in the constructor. Happens if the PMP and INI files are inconsistent.
      if (album_map_it == albums_images_->end())
        continue;

      album_map_it->second.insert(
          folder_path_.Append(base::FilePath::FromUTF8Unsafe(section)));
    }
  }

  const base::FilePath folder_path_;
  AlbumImagesMap* const albums_images_;
};

}  // namespace

PicasaAlbumsIndexer::PicasaAlbumsIndexer(const AlbumUIDSet& album_uids) {
  // Create an entry in the map for the valid album uids.
  for (AlbumUIDSet::const_iterator it = album_uids.begin();
       it != album_uids.end(); ++it) {
    albums_images_[*it] = AlbumImages();
  }
}

PicasaAlbumsIndexer::~PicasaAlbumsIndexer() {}

void PicasaAlbumsIndexer::ParseFolderINI(
    const base::FilePath& folder_path, const std::string& ini_contents) {
  PicasaINIParser parser(folder_path, &albums_images_);
  parser.Parse(ini_contents);
}

}  // namespace picasa
