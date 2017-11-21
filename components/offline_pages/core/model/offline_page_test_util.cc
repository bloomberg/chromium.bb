// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_store_utils.h"

#include "base/files/file_enumerator.h"

namespace offline_pages {

namespace test_util {

size_t GetFileCountInDirectory(const base::FilePath& directory) {
  base::FileEnumerator file_enumerator(directory, false,
                                       base::FileEnumerator::FILES);
  size_t count = 0;
  for (base::FilePath path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    count++;
  }
  return count;
}

}  // namespace test_util

}  // namespace offline_pages
