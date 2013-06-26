// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_galleries/picasa_types.h"

#include "base/logging.h"
#include "chrome/common/media_galleries/pmp_constants.h"

namespace picasa {

namespace {

void ClosePlatformFile(base::PlatformFile* platform_file) {
  DCHECK(platform_file);
  if (base::ClosePlatformFile(*platform_file))
    *platform_file = base::kInvalidPlatformFileValue;
}

}  // namespace

AlbumInfo::AlbumInfo() {}

AlbumInfo::AlbumInfo(const std::string& name, const base::Time& timestamp,
                     const std::string& uid, const base::FilePath& path)
    : name(name),
      timestamp(timestamp),
      uid(uid),
      path(path) {
}

AlbumInfo::~AlbumInfo() {}

AlbumTableFiles::AlbumTableFiles()
    : indicator_file(base::kInvalidPlatformFileValue),
      category_file(base::kInvalidPlatformFileValue),
      date_file(base::kInvalidPlatformFileValue),
      filename_file(base::kInvalidPlatformFileValue),
      name_file(base::kInvalidPlatformFileValue),
      token_file(base::kInvalidPlatformFileValue),
      uid_file(base::kInvalidPlatformFileValue) {
}

void CloseAlbumTableFiles(AlbumTableFiles* table_files) {
  ClosePlatformFile(&(table_files->indicator_file));
  ClosePlatformFile(&(table_files->category_file));
  ClosePlatformFile(&(table_files->date_file));
  ClosePlatformFile(&(table_files->filename_file));
  ClosePlatformFile(&(table_files->name_file));
  ClosePlatformFile(&(table_files->token_file));
  ClosePlatformFile(&(table_files->uid_file));
}

}  // namespace picasa
