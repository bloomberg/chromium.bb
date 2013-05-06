// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/picasa/picasa_album_table_reader.h"

#include <vector>

#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_column_reader.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_constants.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_table_reader.h"

#define FPL(x) FILE_PATH_LITERAL(x)

namespace picasa {

namespace {

// |variant_time| is specified as the number of days from Dec 30, 1899.
base::Time TimeFromMicrosoftVariantTime(double variant_time) {
  base::TimeDelta variant_delta = base::TimeDelta::FromMicroseconds(
      static_cast<int64>(variant_time * base::Time::kMicrosecondsPerDay));

  return base::Time::FromLocalExploded(kPicasaVariantTimeEpoch) + variant_delta;
}

}  // namespace

AlbumInfo::AlbumInfo(const std::string& name, const base::Time& timestamp,
                     const std::string& uid, const base::FilePath& path)
    : name(name),
      timestamp(timestamp),
      uid(uid),
      path(path) {
}

AlbumInfo::~AlbumInfo() {}

PicasaAlbumTableReader::PicasaAlbumTableReader(
    const base::FilePath& directory_path)
    : directory_path_(directory_path),
      initialized_(false) {}

PicasaAlbumTableReader::~PicasaAlbumTableReader() {}

base::FilePath PicasaAlbumTableReader::PicasaDB3Dir() {
  base::FilePath path;

#if defined(OS_WIN)
  if (!PathService::Get(base::DIR_LOCAL_APP_DATA, &path))
    return base::FilePath();
#elif defined(OS_MACOSX)
  if (!PathService::Get(base::DIR_APP_DATA, &path))
    return base::FilePath();
#else
  return base::FilePath();
#endif

  return path.Append(FPL("Google")).Append(FPL("Picasa2")).Append(FPL("db3"));
}

const std::vector<AlbumInfo>& PicasaAlbumTableReader::folders() const {
  DCHECK(initialized_);
  return folders_;
}

const std::vector<AlbumInfo>& PicasaAlbumTableReader::albums() const {
  DCHECK(initialized_);
  return albums_;
}

bool PicasaAlbumTableReader::Init() {
  if (initialized_)
    return true;

  PmpTableReader pmp_reader(kPicasaAlbumTableName, directory_path_);

  const PmpColumnReader* category_column =
      pmp_reader.AddColumn("category", PMP_TYPE_UINT32);
  const PmpColumnReader* date_column =
      pmp_reader.AddColumn("date", PMP_TYPE_DOUBLE64);
  const PmpColumnReader* filename_column =
      pmp_reader.AddColumn("filename", PMP_TYPE_STRING);
  const PmpColumnReader* name_column =
      pmp_reader.AddColumn("name", PMP_TYPE_STRING);
  const PmpColumnReader* token_column =
      pmp_reader.AddColumn("token", PMP_TYPE_STRING);
  const PmpColumnReader* uid_column =
      pmp_reader.AddColumn("uid", PMP_TYPE_STRING);

  if (pmp_reader.Columns().size() != 6)
    return false;

  for (uint32 i = 0; i < pmp_reader.RowCount(); i++) {
    uint32 category = kAlbumCategoryInvalid;
    double date = 0;
    std::string name;
    std::string uid;
    if (!category_column->ReadUInt32(i, &category) ||
        !date_column->ReadDouble64(i, &date) ||
        !name_column->ReadString(i, &name) || name.empty() ||
        !uid_column->ReadString(i, &uid) || uid.empty()) {
      continue;
    }

    base::Time timestamp = TimeFromMicrosoftVariantTime(date);

    switch (category) {
      case kAlbumCategoryAlbum: {
        std::string token;
        if (!token_column->ReadString(i, &token) || token.empty() ||
            !StartsWithASCII(token, kAlbumTokenPrefix, false)) {
          continue;
        }

        albums_.push_back(AlbumInfo(name, timestamp, uid, base::FilePath()));
        break;
      }
      case kAlbumCategoryFolder: {
        std::string filename;
        if (!filename_column->ReadString(i, &filename) || filename.empty())
          continue;

        base::FilePath path =
            base::FilePath(base::FilePath::FromUTF8Unsafe(filename));

        folders_.push_back(AlbumInfo(name, timestamp, uid, path));
        break;
      }
      default: {
        break;
      }
    }
  }

  initialized_ = true;
  return true;
}

}  // namespace picasa
