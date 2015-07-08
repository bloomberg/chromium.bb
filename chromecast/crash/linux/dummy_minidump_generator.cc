// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/dummy_minidump_generator.h"

#include <stdio.h>
#include <sys/stat.h>

#include <vector>

#include "base/logging.h"

namespace chromecast {

namespace {

const int kBufferSize = 32768;

}  // namespace

DummyMinidumpGenerator::DummyMinidumpGenerator(
    const std::string& existing_minidump_path)
    : existing_minidump_path_(existing_minidump_path) {
}

bool DummyMinidumpGenerator::Generate(const std::string& minidump_path) {
  // Use stdlib calls here to avoid potential IO restrictions on this thread.

  // Return false if the file does not exist.
  struct stat st;
  if (stat(existing_minidump_path_.c_str(), &st) != 0) {
    PLOG(ERROR) << existing_minidump_path_.c_str() << " does not exist: ";
    return false;
  }

  LOG(INFO) << "Moving minidump from " << existing_minidump_path_ << " to "
            << minidump_path << " for further uploading.";

  // Attempt to rename(). If this operation fails, the files are on different
  // volumes. Fall back to a copy and delete.
  if (rename(existing_minidump_path_.c_str(), minidump_path.c_str()) < 0) {
    // Any errors will be logged within CopyAndDelete().
    return CopyAndDelete(minidump_path);
  }

  return true;
}

bool DummyMinidumpGenerator::CopyAndDelete(const std::string& dest_path) {
  FILE* src = fopen(existing_minidump_path_.c_str(), "r");
  if (!src) {
    PLOG(ERROR) << existing_minidump_path_ << " failed to open: ";
    return false;
  }

  FILE* dest = fopen(dest_path.c_str(), "w");
  if (!dest) {
    PLOG(ERROR) << dest_path << " failed to open: ";
    return false;
  }

  // Copy all bytes from |src| into |dest|.
  std::vector<char> buffer(kBufferSize);
  bool success = false;
  while (!success) {
    size_t bytes_read = fread(&buffer[0], 1, buffer.size(), src);
    if (bytes_read < buffer.size()) {
      if (feof(src)) {
        success = true;
      } else {
        // An error occurred.
        PLOG(ERROR) << "Error reading " << existing_minidump_path_ << ": ";
        break;
      }
    }

    size_t bytes_written = fwrite(&buffer[0], 1, bytes_read, dest);
    if (bytes_written < bytes_read) {
      // An error occurred.
      PLOG(ERROR) << "Error writing to " << dest_path << ": ";
      success = false;
      break;
    }
  }

  // Close both files.
  fclose(src);
  fclose(dest);

  // Attempt to delete file at |existing_minidump_path_|. We should log this
  // error, but the function should not fail if the file is not removed.
  if (remove(existing_minidump_path_.c_str()) < 0)
    PLOG(ERROR) << "Could not remove " << existing_minidump_path_ << ": ";

  return success;
}

}  // namespace chromecast
