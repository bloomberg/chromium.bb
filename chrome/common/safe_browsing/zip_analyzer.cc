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
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "third_party/zlib/google/zip_reader.h"

namespace safe_browsing {
namespace zip_analyzer {

namespace {

const int kLFHBlockSize = 8192;
const char kLocalFileHeaderSize = 4;
const char kLocalFileHeader[kLocalFileHeaderSize] = {0x50, 0x4b, 0x03, 0x04};

}  // namespace

// Returns the number of Local File Headers present in the zip file.
int CountLocalFileHeaders(base::File* zip_file) {
  int lfh_count = 0;
  char block[kLFHBlockSize + 3];  // The extra 3 bytes will be used to hold
                                  // the last bytes of the previous block
  block[0] = 0;
  block[1] = 0;
  block[2] = 0;

  zip_file->Seek(base::File::Whence::FROM_BEGIN, 0);
  int bytes_read = 0;
  while ((bytes_read = zip_file->ReadAtCurrentPos(block + 3, kLFHBlockSize)) >
         0) {
    for (int i = 0; i < bytes_read; i++) {
      if (!memcmp(block + i, kLocalFileHeader, kLocalFileHeaderSize))
        lfh_count++;
    }

    // Copy the end of this block to the new block
    block[0] = block[bytes_read];
    block[1] = block[bytes_read + 1];
    block[2] = block[bytes_read + 2];
  }

  return lfh_count;
}

void AnalyzeZipFile(base::File zip_file,
                    base::File temp_file,
                    ArchiveAnalyzerResults* results) {
  zip::ZipReader reader;
  if (!reader.OpenFromPlatformFile(zip_file.GetPlatformFile())) {
    DVLOG(1) << "Failed to open zip file";
    return;
  }

  bool too_big_to_unpack =
      base::checked_cast<uint64_t>(zip_file.GetLength()) >
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("zip");
  UMA_HISTOGRAM_BOOLEAN("SBClientDownload.ZipTooBigToUnpack",
                        too_big_to_unpack);
  if (too_big_to_unpack) {
    results->success = true;
    return;
  }

  bool contains_zip = false;
  bool advanced = true;
  int zip_entry_count = 0;
  results->file_count = 0;
  results->directory_count = 0;
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

    if (FileTypePolicies::GetFileExtension(
            reader.current_entry_info()->file_path()) ==
        FILE_PATH_LITERAL(".zip"))
      contains_zip = true;
    zip_entry_count++;

    if (reader.current_entry_info()->is_directory())
      results->directory_count++;
    else
      results->file_count++;
  }

  results->success = true;

  if (base::RandDouble() < 0.01 && !contains_zip) {
    int lfh_count = CountLocalFileHeaders(&zip_file);

    DCHECK_LE(zip_entry_count, lfh_count);

    base::UmaHistogramSparse(
        "SBClientDownload.ZipFileLocalFileHeadersSkipped",
        base::ClampToRange(lfh_count - zip_entry_count, 0, 100));
  }
}

}  // namespace zip_analyzer
}  // namespace safe_browsing
