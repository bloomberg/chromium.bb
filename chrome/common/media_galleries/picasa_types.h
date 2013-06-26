// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_GALLERIES_PICASA_TYPES_H_
#define CHROME_COMMON_MEDIA_GALLERIES_PICASA_TYPES_H_

#include <string>

#include "base/files/file_path.h"
#include "base/platform_file.h"

namespace picasa {

const char kPicasaAlbumTableName[] = "albumdata";

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

struct AlbumTableFiles {
  AlbumTableFiles();

  // Special empty file used to confirm existence of table.
  base::PlatformFile indicator_file;

  base::PlatformFile category_file;
  base::PlatformFile date_file;
  base::PlatformFile filename_file;
  base::PlatformFile name_file;
  base::PlatformFile token_file;
  base::PlatformFile uid_file;
};

void CloseAlbumTableFiles(AlbumTableFiles* table_files);

}  // namespace picasa

#endif  // CHROME_COMMON_MEDIA_GALLERIES_PICASA_TYPES_H_
