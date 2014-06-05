// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/zip_analyzer.h"

#include "base/logging.h"
#include "chrome/common/safe_browsing/download_protection_util.h"
#include "third_party/zlib/google/zip_reader.h"

namespace safe_browsing {
namespace zip_analyzer {

void AnalyzeZipFile(base::File zip_file, Results* results) {
  zip::ZipReader reader;
  // OpenFromPlatformFile may close the handle even when it fails, but there is
  // no way to know if it did that or not. Assume it did (that's the common
  // case).
  if (!reader.OpenFromPlatformFile(zip_file.TakePlatformFile())) {
    VLOG(1) << "Failed to open zip file";
    return;
  }

  bool advanced = true;
  for (; reader.HasMore(); advanced = reader.AdvanceToNextEntry()) {
    if (!advanced) {
      VLOG(1) << "Could not advance to next entry, aborting zip scan.";
      return;
    }
    if (!reader.OpenCurrentEntryInZip()) {
      VLOG(1) << "Failed to open current entry in zip file";
      continue;
    }
    const base::FilePath& file = reader.current_entry_info()->file_path();
    if (download_protection_util::IsBinaryFile(file)) {
      // Don't consider an archived archive to be executable, but record
      // a histogram.
      if (download_protection_util::IsArchiveFile(file)) {
        results->has_archive = true;
      } else {
        VLOG(2) << "Downloaded a zipped executable: " << file.value();
        results->has_executable = true;
        break;
      }
    } else {
      VLOG(3) << "Ignoring non-binary file: " << file.value();
    }
  }
  results->success = true;
}

}  // namespace zip_analyzer
}  // namespace safe_browsing
