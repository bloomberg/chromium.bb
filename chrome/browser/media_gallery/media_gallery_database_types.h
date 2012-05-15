// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERY_DATABASE_TYPES_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERY_DATABASE_TYPES_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/time.h"

namespace chrome {

typedef int CollectionId;

struct CollectionRow {
 public:
  CollectionRow();
  CollectionRow(const FilePath& path,
                base::Time last_modified_time,
                int entry_count,
                bool all_parsed);
  ~CollectionRow();
  bool operator==(const CollectionRow& row2) const;

  CollectionId id;
  FilePath path;
  base::Time last_modified_time;
  int entry_count;
  bool all_parsed;

 private:
  DISALLOW_COPY_AND_ASSIGN(CollectionRow);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERY_DATABASE_TYPES_H_
