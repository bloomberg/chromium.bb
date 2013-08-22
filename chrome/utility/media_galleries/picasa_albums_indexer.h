// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_MEDIA_GALLERIES_PICASA_ALBUMS_INDEXER_H_
#define CHROME_UTILITY_MEDIA_GALLERIES_PICASA_ALBUMS_INDEXER_H_

#include <map>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "chrome/common/media_galleries/picasa_types.h"

namespace picasa {

// Parses a series of INI files and builds up the set of files contained within
// the albums passed in through |album_uids|.
//
// Each INI file only describes the images contained within a single directory.
// To build the contents of all the albums, we read in all the INI files
// in all the Picasa folders.
//
// The INI albums also contain ".album*" sections describing the albums that
// have pictures in the same directory as the INI. However, we use the PMP
// database as the authoritative source on Album metadata, so we ignore those
// sections. The PMP derived |album_uids| are passed in by the constructor.
//
// Example INI File:
//
// [.album:e66fb059001aabcc69b262b7009fad90]
// name=CustomAlbum1
// token=e66fb059001aabcc69b262b7009fad90
// date=2013-03-15T14:53:21-07:00
// [InBoth.jpg]
// albums=e66fb059001aabcc69b262b7009fad90,18cb2df48aaa98e1c276b45cfcd81c95
// [.album:18cb2df48aaa98e1c276b45cfcd81c95]
// name=CustomAlbum1
// token=18cb2df48aaa98e1c276b45cfcd81c95
// date=2013-04-01T16:37:34-07:00
// [InFirst.jpg]
// albums=e66fb059001aabcc69b262b7009fad90
// [InSecond.jpg]
// albums=18cb2df48aaa98e1c276b45cfcd81c95
class PicasaAlbumsIndexer {
 public:
  explicit PicasaAlbumsIndexer(const AlbumUIDSet& album_uids);
  ~PicasaAlbumsIndexer();

  // This method should be called once for each Folder in the PMP database.
  void ParseFolderINI(
      const std::vector<picasa::FolderINIContents>& folders_inis);

  const AlbumImagesMap& albums_images() const { return albums_images_; }

 private:
  AlbumImagesMap albums_images_;

  DISALLOW_COPY_AND_ASSIGN(PicasaAlbumsIndexer);
};

}  // namespace picasa

#endif  // CHROME_UTILITY_MEDIA_GALLERIES_PICASA_ALBUMS_INDEXER_H_
