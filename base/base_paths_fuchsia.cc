// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include "base/files/file_path.h"

namespace base {

bool PathProviderFuchsia(int key, FilePath* result) {
  switch (key) {
    case FILE_EXE:
    case FILE_MODULE: {
      // TODO(fuchsia): There's no API to retrieve this on Fuchsia currently.
      // See https://crbug.com/726124.
      *result = FilePath("/system/chrome");
      return true;
    }
  }

  return false;
}

}  // namespace base
