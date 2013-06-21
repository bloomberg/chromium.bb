// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PICASA_ALBUM_TABLE_READER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PICASA_ALBUM_TABLE_READER_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/platform_file.h"
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
  AlbumInfo();
  AlbumInfo(const std::string& name, const base::Time& timestamp,
            const std::string& uid, const base::FilePath& path);

  ~AlbumInfo();

  std::string name;
  base::Time timestamp;
  std::string uid;
  base::FilePath path;
};

struct PicasaAlbumTableFiles {
  explicit PicasaAlbumTableFiles(const base::FilePath& directory_path);

  // Special empty file used to confirm existence of table.
  base::PlatformFile indicator_file;

  base::PlatformFile category_file;
  base::PlatformFile date_file;
  base::PlatformFile filename_file;
  base::PlatformFile name_file;
  base::PlatformFile token_file;
  base::PlatformFile uid_file;
};

class PicasaAlbumTableReader {
 public:
  explicit PicasaAlbumTableReader(const PicasaAlbumTableFiles& table_files);
  virtual ~PicasaAlbumTableReader();

  bool Init();

  const std::vector<AlbumInfo>& albums() const;
  const std::vector<AlbumInfo>& folders() const;

 private:
  const PicasaAlbumTableFiles table_files_;

  bool initialized_;

  std::vector<AlbumInfo> albums_;
  std::vector<AlbumInfo> folders_;

  DISALLOW_COPY_AND_ASSIGN(PicasaAlbumTableReader);
};

void ClosePicasaAlbumTableFiles(PicasaAlbumTableFiles* table_files);

}  // namespace picasa

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PICASA_ALBUM_TABLE_READER_H_
