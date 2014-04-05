// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_MEDIA_GALLERIES_PICASA_ALBUM_TABLE_READER_H_
#define CHROME_UTILITY_MEDIA_GALLERIES_PICASA_ALBUM_TABLE_READER_H_

#include <vector>

#include "base/basictypes.h"
#include "chrome/common/media_galleries/picasa_types.h"

namespace picasa {

class PicasaAlbumTableReader {
 public:
  // This class takes ownership of |table_files| and will close them.
  explicit PicasaAlbumTableReader(AlbumTableFiles table_files);
  ~PicasaAlbumTableReader();

  bool Init();

  const std::vector<AlbumInfo>& albums() const;
  const std::vector<AlbumInfo>& folders() const;

 private:
  AlbumTableFiles table_files_;

  bool initialized_;

  std::vector<AlbumInfo> albums_;
  std::vector<AlbumInfo> folders_;

  DISALLOW_COPY_AND_ASSIGN(PicasaAlbumTableReader);
};

}  // namespace picasa

#endif  // CHROME_UTILITY_MEDIA_GALLERIES_PICASA_ALBUM_TABLE_READER_H_
