// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/zip_analyzer.h"

#include "base/logging.h"
#include "chrome/common/safe_browsing/download_protection_util.h"
#include "chrome/common/zip_reader.h"

namespace safe_browsing {
namespace zip_analyzer {

void AnalyzeZipFile(base::PlatformFile zip_file, Results* results) {
  zip::ZipReader reader;
  if (!reader.OpenFromPlatformFile(zip_file)) {
    VLOG(1) << "Failed to open zip file";
    return;
  }

  for (; reader.HasMore(); reader.AdvanceToNextEntry()) {
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
