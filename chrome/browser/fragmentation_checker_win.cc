// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fragmentation_checker_win.h"

#include <windows.h>
#include <winioctl.h>

#include <vector>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/platform_file.h"
#include "base/path_service.h"

namespace {

size_t ComputeRetrievalPointersBufferSize(int number_of_extents) {
  RETRIEVAL_POINTERS_BUFFER buffer;
  return sizeof(buffer) + (number_of_extents - 1) * sizeof(buffer.Extents);
}

}  // namespace

namespace fragmentation_checker {

int CountFileExtents(const FilePath& file_path) {
  int file_extents_count = 0;

  base::PlatformFileError error_code = base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFile file_handle = CreatePlatformFile(
      file_path,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL,
      &error_code);
  if (error_code == base::PLATFORM_FILE_OK) {
    STARTING_VCN_INPUT_BUFFER starting_vcn_input_buffer = {0};

    // Compute an output size capable of holding 16 extents at first. This will
    // fail when the number of extents exceeds 16, in which case we make
    // a bigger buffer capable of holding up to kMaxExtentCounts.
    int extents_guess = 16;
    size_t output_size = ComputeRetrievalPointersBufferSize(extents_guess);
    std::vector<uint8> retrieval_pointers_buffer(output_size);

    DWORD bytes_returned = 0;

    bool result = false;
    do {
      result = DeviceIoControl(
          file_handle,
          FSCTL_GET_RETRIEVAL_POINTERS,
          reinterpret_cast<void*>(&starting_vcn_input_buffer),
          sizeof(starting_vcn_input_buffer),
          reinterpret_cast<void*>(&retrieval_pointers_buffer[0]),
          retrieval_pointers_buffer.size(),
          &bytes_returned,
          NULL) != FALSE;

      if (!result) {
        if (GetLastError() == ERROR_MORE_DATA) {
          // Grow the extents we can handle
          extents_guess *= 2;
          if (extents_guess > kMaxExtentCount) {
            LOG(ERROR) << "FSCTL_GET_RETRIEVAL_POINTERS output buffer exceeded "
                          "maximum size.";
            file_extents_count = kMaxExtentCount;
            break;
          }
          output_size = ComputeRetrievalPointersBufferSize(extents_guess);
          retrieval_pointers_buffer.assign(output_size, 0);
        } else {
          PLOG(ERROR) << "FSCTL_GET_RETRIEVAL_POINTERS failed.";
          break;
        }
      }
    } while (!result);

    if (result) {
      RETRIEVAL_POINTERS_BUFFER* retrieval_pointers =
          reinterpret_cast<RETRIEVAL_POINTERS_BUFFER*>(
              &retrieval_pointers_buffer[0]);
      file_extents_count = static_cast<int>(retrieval_pointers->ExtentCount);
    } else {
      LOG(ERROR) << "Failed to retrieve extents.";
    }
  } else {
    LOG(ERROR) << "Failed to open module file to check extents. Error code = "
               << error_code;
  }

  return file_extents_count;
}

void RecordFragmentationMetricForCurrentModule() {
  FilePath module_path;
  if (PathService::Get(base::FILE_MODULE, &module_path)) {
    int file_extent_count = CountFileExtents(module_path);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Fragmentation.ModuleExtents",
                                file_extent_count,
                                0,
                                kMaxExtentCount,
                                50);
  } else {
    NOTREACHED() << "Could not get path to current module.";
  }
}

}  // namespace fragmentation_checker
