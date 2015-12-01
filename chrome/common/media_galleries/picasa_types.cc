// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_galleries/picasa_types.h"

#include <utility>

#include "base/logging.h"
#include "chrome/common/media_galleries/pmp_constants.h"

namespace picasa {

namespace {

base::File OpenFile(const base::FilePath& directory_path,
                    const std::string& suffix) {
  base::FilePath path = directory_path.Append(base::FilePath::FromUTF8Unsafe(
      std::string(kPicasaAlbumTableName) + "_" + suffix));
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

base::File OpenColumnFile(const base::FilePath& directory_path,
                          const std::string& column_name) {
  return OpenFile(directory_path, column_name + "." + kPmpExtension);
}

}  // namespace

const char kPicasaDatabaseDirName[] = "db3";
const char kPicasaTempDirName[] = "tmp";

const char kPicasaAlbumTableName[] = "albumdata";
const char kAlbumTokenPrefix[] = "]album:";
const char kPicasaINIFilename[] = ".picasa.ini";

const uint32 kAlbumCategoryAlbum = 0;
const uint32 kAlbumCategoryFolder = 2;
const uint32 kAlbumCategoryInvalid = 0xffff;  // Sentinel value.

AlbumInfo::AlbumInfo() {
}

AlbumInfo::AlbumInfo(const std::string& name, const base::Time& timestamp,
                     const std::string& uid, const base::FilePath& path)
    : name(name),
      timestamp(timestamp),
      uid(uid),
      path(path) {
}

AlbumInfo::~AlbumInfo() {
}

AlbumTableFiles::AlbumTableFiles() {
}

AlbumTableFiles::AlbumTableFiles(const base::FilePath& directory_path)
    : indicator_file(OpenFile(directory_path, "0")),
      category_file(OpenColumnFile(directory_path, "category")),
      date_file(OpenColumnFile(directory_path, "date")),
      filename_file(OpenColumnFile(directory_path, "filename")),
      name_file(OpenColumnFile(directory_path, "name")),
      token_file(OpenColumnFile(directory_path, "token")),
      uid_file(OpenColumnFile(directory_path, "uid")) {
}

AlbumTableFiles::~AlbumTableFiles() {
}

AlbumTableFiles::AlbumTableFiles(AlbumTableFiles&& other)
    : indicator_file(std::move(other.indicator_file)),
      category_file(std::move(other.category_file)),
      date_file(std::move(other.date_file)),
      filename_file(std::move(other.filename_file)),
      name_file(std::move(other.name_file)),
      token_file(std::move(other.token_file)),
      uid_file(std::move(other.uid_file)) {}

AlbumTableFiles& AlbumTableFiles::operator=(AlbumTableFiles&& other) {
  indicator_file = std::move(other.indicator_file);
  category_file = std::move(other.category_file);
  date_file = std::move(other.date_file);
  filename_file = std::move(other.filename_file);
  name_file = std::move(other.name_file);
  token_file = std::move(other.token_file);
  uid_file = std::move(other.uid_file);
  return *this;
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

}  // namespace picasa
