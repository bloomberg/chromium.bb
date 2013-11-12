// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/api/image_writer_private/image_writer_utils.h"

namespace extensions {
namespace image_writer_utils
{

const int kFsyncRatio = 1024;

ImageWriter::ImageWriter() : file_(base::kInvalidPlatformFileValue),
                             writes_count_(0) {
}

ImageWriter::~ImageWriter() {
  if (file_ != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(file_);
}

bool ImageWriter::Open(const base::FilePath& path) {
  if (file_ != base::kInvalidPlatformFileValue)
    Close();
  DCHECK_EQ(base::kInvalidPlatformFileValue, file_);
  base::PlatformFileError error;
  file_ = base::CreatePlatformFile(
    path,
    base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
    NULL,
    &error);
  if (error != base::PLATFORM_FILE_OK) {
    LOG(ERROR) << "Couldn't open target path "
               << path.value() << ": " << error;
    return false;
  }
  return true;
}

bool ImageWriter::Close() {
  if (file_ != base::kInvalidPlatformFileValue &&
      base::ClosePlatformFile(file_)) {
    file_ = base::kInvalidPlatformFileValue;
    return true;
  } else {
    LOG(ERROR) << "Could not close target file";
    return false;
  }
}

int ImageWriter::Write(const char* data_block, int data_size) {
  int written = base::WritePlatformFileAtCurrentPos(file_, data_block,
                                                    data_size);
  if (written != data_size) {
    LOG(ERROR) << "Error writing to target file";
    return -1;
  }

  writes_count_++;
  if (writes_count_ == kFsyncRatio) {
    if (!base::FlushPlatformFile(file_)) {
      LOG(ERROR) << "Error syncing target file";
      return -1;
    }
    writes_count_ = 0;
  }

  return written;
}

ImageReader::ImageReader() : file_(base::kInvalidPlatformFileValue) {
}

ImageReader::~ImageReader() {
  if (file_ != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(file_);
}

bool ImageReader::Open(const base::FilePath& path) {
  if (file_ != base::kInvalidPlatformFileValue)
    Close();
  DCHECK_EQ(base::kInvalidPlatformFileValue, file_);
  base::PlatformFileError error;
  file_ = base::CreatePlatformFile(
    path,
    base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
    NULL,
    &error);
  if (error != base::PLATFORM_FILE_OK) {
    LOG(ERROR) << "Couldn't open target path "
               << path.value() << ": " << error;
    return false;
  }
  return true;
}

bool ImageReader::Close() {
  if (file_ != base::kInvalidPlatformFileValue &&
      base::ClosePlatformFile(file_)) {
    file_ = base::kInvalidPlatformFileValue;
    return true;
  } else {
    LOG(ERROR) << "Could not close target file";
    return false;
  }
  return true;
}

int ImageReader::Read(char* data_block, int data_size) {
  int read = base::ReadPlatformFileAtCurrentPos(file_, data_block, data_size);
  if (read < 0)
    LOG(ERROR) << "Error reading from source file";
  return read;
}

int64 ImageReader::GetSize() {
  base::PlatformFileInfo info;
  if (base::GetPlatformFileInfo(file_, &info)) {
    return info.size;
  } else {
    return 0;
  }
}

} // namespace image_writer_utils
} // namespace extensions
