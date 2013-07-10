// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_galleries/picasa_types.h"

#include "base/logging.h"
#include "chrome/common/media_galleries/pmp_constants.h"

namespace picasa {

namespace {

base::PlatformFile OpenPlatformFile(const base::FilePath& directory_path,
                                    const std::string& suffix) {
  base::FilePath path = directory_path.Append(base::FilePath::FromUTF8Unsafe(
      std::string(kPicasaAlbumTableName) + "_" + suffix));
  int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ;
  return base::CreatePlatformFile(path, flags, NULL, NULL);
}

base::PlatformFile OpenColumnPlatformFile(const base::FilePath& directory_path,
                                          const std::string& column_name) {
  return OpenPlatformFile(directory_path, column_name + "." + kPmpExtension);
}

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

AlbumTableFiles::AlbumTableFiles(const base::FilePath& directory_path)
    : indicator_file(OpenPlatformFile(directory_path, "0")),
      category_file(OpenColumnPlatformFile(directory_path, "category")),
      date_file(OpenColumnPlatformFile(directory_path, "date")),
      filename_file(OpenColumnPlatformFile(directory_path, "filename")),
      name_file(OpenColumnPlatformFile(directory_path, "name")),
      token_file(OpenColumnPlatformFile(directory_path, "token")),
      uid_file(OpenColumnPlatformFile(directory_path, "uid")) {
}

AlbumTableFilesForTransit::AlbumTableFilesForTransit()
    : indicator_file(IPC::InvalidPlatformFileForTransit()),
      category_file(IPC::InvalidPlatformFileForTransit()),
      date_file(IPC::InvalidPlatformFileForTransit()),
      filename_file(IPC::InvalidPlatformFileForTransit()),
      name_file(IPC::InvalidPlatformFileForTransit()),
      token_file(IPC::InvalidPlatformFileForTransit()),
      uid_file(IPC::InvalidPlatformFileForTransit()) {
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
