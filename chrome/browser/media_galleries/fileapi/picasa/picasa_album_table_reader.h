// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PICASA_ALBUM_TABLE_READER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PICASA_ALBUM_TABLE_READER_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/time.h"

namespace picasa {

const base::Time::Exploded kPicasaVariantTimeEpoch = {
  1899, 12, 7, 30,  // Dec 30, 1899 (Saturday)
  0, 0, 0, 0        // 00:00:00.000
};

const char kPicasaAlbumTableName[] = "albumdata";

const uint32 kAlbumCategoryAlbum     = 0;
const uint32 kAlbumCategoryFolder    = 2;
const uint32 kAlbumCategoryInvalid   = 0xffff;  // Sentinel value.

const char kAlbumTokenPrefix[] = "]album:";

struct AlbumInfo {
  AlbumInfo(const std::string& name, const base::Time& timestamp,
            const std::string& uid, const base::FilePath& path);

  ~AlbumInfo();

  std::string name;
  base::Time timestamp;
  std::string uid;
  base::FilePath path;
};

class PicasaAlbumTableReader {
 public:
  // |directory_path| is Picasa's db3 directory where the PMP table is stored.
  explicit PicasaAlbumTableReader(const base::FilePath& directory_path);
  virtual ~PicasaAlbumTableReader();

  bool Init();

  const std::vector<AlbumInfo>& albums() const;
  const std::vector<AlbumInfo>& folders() const;

  static base::FilePath PicasaDB3Dir();

 private:
  const base::FilePath directory_path_;

  bool initialized_;

  std::vector<AlbumInfo> albums_;
  std::vector<AlbumInfo> folders_;

  DISALLOW_COPY_AND_ASSIGN(PicasaAlbumTableReader);
};

}  // namespace picasa

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PICASA_ALBUM_TABLE_READER_H_
