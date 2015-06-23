// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/dummy_minidump_generator.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"

namespace chromecast {

DummyMinidumpGenerator::DummyMinidumpGenerator(
    const std::string& existing_minidump_path)
    : existing_minidump_path_(existing_minidump_path) {
}

bool DummyMinidumpGenerator::Generate(const std::string& minidump_path) {
  base::FilePath file(existing_minidump_path_);
  if (!base::PathExists(file)) {
    LOG(ERROR) << file.value() << " is not a valid file path";
    return false;
  }

  LOG(INFO) << "Moving minidump from " << existing_minidump_path_ << " to "
            << minidump_path << " for further uploading.";
  // Use stdlib call here to avoid potential IO restrictions on this thread.
  if (rename(file.value().c_str(), minidump_path.c_str()) < 0) {
    LOG(ERROR) << "Could not move file: " << file.value() << " "
               << strerror(errno);
    return false;
  }

  return true;
}

}  // namespace chromecast
