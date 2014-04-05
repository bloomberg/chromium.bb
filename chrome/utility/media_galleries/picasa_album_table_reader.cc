// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/picasa_album_table_reader.h"

#include <algorithm>
#include <string>

#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/media_galleries/pmp_constants.h"
#include "chrome/utility/media_galleries/pmp_column_reader.h"

namespace picasa {

namespace {

// |variant_time| is specified as the number of days from Dec 30, 1899.
base::Time TimeFromMicrosoftVariantTime(double variant_time) {
  base::TimeDelta variant_delta = base::TimeDelta::FromMicroseconds(
      static_cast<int64>(variant_time * base::Time::kMicrosecondsPerDay));

  return base::Time::FromLocalExploded(kPmpVariantTimeEpoch) + variant_delta;
}

}  // namespace

PicasaAlbumTableReader::PicasaAlbumTableReader(AlbumTableFiles table_files)
    : table_files_(table_files.Pass()),
      initialized_(false) {
}

PicasaAlbumTableReader::~PicasaAlbumTableReader() {
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

  if (!table_files_.indicator_file.IsValid())
    return false;

  PmpColumnReader category_column, date_column, filename_column, name_column,
                  token_column, uid_column;
  if (!category_column.ReadFile(&table_files_.category_file, PMP_TYPE_UINT32) ||
      !date_column.ReadFile(&table_files_.date_file, PMP_TYPE_DOUBLE64) ||
      !filename_column.ReadFile(&table_files_.filename_file, PMP_TYPE_STRING) ||
      !name_column.ReadFile(&table_files_.name_file, PMP_TYPE_STRING) ||
      !token_column.ReadFile(&table_files_.token_file, PMP_TYPE_STRING) ||
      !uid_column.ReadFile(&table_files_.uid_file, PMP_TYPE_STRING)) {
    return false;
  }

  // In the PMP format, columns can be different lengths. The number of rows
  // in the table is max of all the columns, and short columns are NULL padded.
  uint32 row_count = 0;
  row_count = std::max(row_count, category_column.rows_read());
  row_count = std::max(row_count, date_column.rows_read());
  row_count = std::max(row_count, filename_column.rows_read());
  row_count = std::max(row_count, name_column.rows_read());
  row_count = std::max(row_count, token_column.rows_read());
  row_count = std::max(row_count, uid_column.rows_read());

  for (uint32 i = 0; i < row_count; i++) {
    uint32 category = kAlbumCategoryInvalid;
    double date = 0;
    std::string name;
    std::string uid;
    // PMP tables often contain 'garbage' rows of deleted or auto-generated
    // album-like entities. We ignore those rows.
    if (!category_column.ReadUInt32(i, &category) ||
        !date_column.ReadDouble64(i, &date) ||
        !name_column.ReadString(i, &name) || name.empty() ||
        !uid_column.ReadString(i, &uid) || uid.empty()) {
      continue;
    }

    base::Time timestamp = TimeFromMicrosoftVariantTime(date);

    if (category == kAlbumCategoryAlbum) {
      std::string token;
      if (!token_column.ReadString(i, &token) || token.empty() ||
          !StartsWithASCII(token, kAlbumTokenPrefix, false)) {
        continue;
      }

      albums_.push_back(AlbumInfo(name, timestamp, uid, base::FilePath()));
    } else if (category == kAlbumCategoryFolder) {
      std::string filename;
      if (!filename_column.ReadString(i, &filename) || filename.empty())
        continue;

      base::FilePath path =
          base::FilePath(base::FilePath::FromUTF8Unsafe(filename));

      folders_.push_back(AlbumInfo(name, timestamp, uid, path));
    }
  }

  initialized_ = true;
  return true;
}

}  // namespace picasa
