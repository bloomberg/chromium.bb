// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/picasa_albums_indexer.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/ini_parser.h"

namespace picasa {

namespace {

const char kAlbumSectionHeader[] = ".album:";
const char kAlbumsKey[] = "albums";
const int kMaxDedupeNumber = 1000;  // Chosen arbitrarily.

class PicasaINIParser : public INIParser {
 public:
  PicasaINIParser(
      const base::FilePath& folder_path, AlbumImagesMap* albums_images)
      : folder_path_(folder_path),
        albums_images_(albums_images) {
  }
  ~PicasaINIParser() override {}

 private:
  void HandleTriplet(const std::string& section,
                     const std::string& key,
                     const std::string& value) override {
    if (key != kAlbumsKey)
      return;

    // [.album:*] sections ignored as we get that data from the PMP files.
    if (base::StartsWith(section, kAlbumSectionHeader,
                         base::CompareCase::SENSITIVE))
      return;

    for (const std::string& album : base::SplitString(
              value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      AlbumImagesMap::iterator album_map_it = albums_images_->find(album);

      // Ignore entry if the album uid is not listed among in |album_uids|
      // in the constructor. Happens if the PMP and INI files are inconsistent.
      if (album_map_it == albums_images_->end())
        continue;

      base::FilePath filename = base::FilePath::FromUTF8Unsafe(section);
      AlbumImages& album_images = album_map_it->second;

      // If filename is first of its name in album, simply add.
      if (album_images.insert(
              std::make_pair(section, folder_path_.Append(filename))).second) {
        continue;
      }

      // Otherwise, de-dupe by appending a number starting at (1).
      for (int i = 1; i < kMaxDedupeNumber; ++i) {
        std::string deduped_filename =
            filename.InsertBeforeExtensionASCII(base::StringPrintf(" (%d)", i))
                .AsUTF8Unsafe();

        // Attempt to add the de-duped name.
        if (album_images.insert(
                std::make_pair(deduped_filename, folder_path_.Append(filename)))
                .second) {
          break;
        }
      }
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
    const std::vector<picasa::FolderINIContents>& folders_inis) {
  // Make a copy for sorting
  std::vector<picasa::FolderINIContents> folders_inis_sorted = folders_inis;

  // Sort here so image names are deduplicated in a stable ordering.
  std::sort(folders_inis_sorted.begin(), folders_inis_sorted.end());

  for (std::vector<picasa::FolderINIContents>::const_iterator it =
           folders_inis_sorted.begin();
       it != folders_inis_sorted.end();
       ++it) {
    PicasaINIParser parser(it->folder_path, &albums_images_);
    parser.Parse(it->ini_contents);
  }
}

}  // namespace picasa
