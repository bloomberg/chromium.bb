// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_MINIZIP_HELPERS_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_MINIZIP_HELPERS_H_

#include "base/scoped_generic.h"

#include "third_party/minizip/src/mz_zip.h"

namespace zip_archiver {
namespace internal {

struct MzZipTraits {
  static void* InvalidValue() { return nullptr; }

  static void Free(void* zip_file) {
    mz_zip_close(zip_file);
    mz_zip_delete(&zip_file);
  }
};

}  // namespace internal
}  // namespace zip_archiver

typedef base::ScopedGeneric<void*, zip_archiver::internal::MzZipTraits>
    ScopedMzZip;

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_MINIZIP_HELPERS_H_
