// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/media_gallery_database_types.h"

namespace chrome {

CollectionRow::CollectionRow()
    : id(0),
      entry_count(0),
      all_parsed(false) { }

CollectionRow::CollectionRow(const FilePath& path,
              base::Time last_modified_time,
              int entry_count,
              bool all_parsed)
    : id(0),
      path(path),
      last_modified_time(last_modified_time),
      entry_count(entry_count),
      all_parsed(all_parsed) { }

CollectionRow::~CollectionRow() { }

bool CollectionRow::operator==(const CollectionRow& row2) const {
  return id == row2.id &&
      path == row2.path &&
      last_modified_time == row2.last_modified_time &&
      entry_count == row2.entry_count &&
      all_parsed == row2.all_parsed;
}

}  // namespace chrome
