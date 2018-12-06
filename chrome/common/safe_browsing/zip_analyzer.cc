// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/zip_analyzer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>

#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "third_party/zlib/google/zip_reader.h"

namespace safe_browsing {
namespace zip_analyzer {

void AnalyzeZipFile(base::File zip_file,
                    base::File temp_file,
                    ArchiveAnalyzerResults* results) {
  zip::ZipReader reader;
  if (!reader.OpenFromPlatformFile(zip_file.GetPlatformFile())) {
    DVLOG(1) << "Failed to open zip file";
    return;
  }

  bool advanced = true;
  for (; reader.HasMore(); advanced = reader.AdvanceToNextEntry()) {
    if (!advanced) {
      DVLOG(1) << "Could not advance to next entry, aborting zip scan.";
      return;
    }
    if (!reader.OpenCurrentEntryInZip()) {
      DVLOG(1) << "Failed to open current entry in zip file";
      continue;
    }

    // Clear the |temp_file| between extractions.
    temp_file.Seek(base::File::Whence::FROM_BEGIN, 0);
    temp_file.SetLength(0);
    zip::FileWriterDelegate writer(&temp_file);
    reader.ExtractCurrentEntry(&writer, std::numeric_limits<uint64_t>::max());
    UpdateArchiveAnalyzerResultsWithFile(
        reader.current_entry_info()->file_path(), &temp_file,
        reader.current_entry_info()->is_encrypted(), results);
  }

  results->success = true;
}

}  // namespace zip_analyzer
}  // namespace safe_browsing
