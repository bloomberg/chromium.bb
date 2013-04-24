// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/linux/snapshot_file_details.h"

#include "base/safe_numerics.h"

namespace chrome {

////////////////////////////////////////////////////////////////////////////////
//                             SnapshotRequestInfo                            //
////////////////////////////////////////////////////////////////////////////////

SnapshotRequestInfo::SnapshotRequestInfo(
    const std::string& device_file_path,
    const base::FilePath& snapshot_file_path,
    const MTPDeviceAsyncDelegate::CreateSnapshotFileSuccessCallback&
        success_callback,
    const MTPDeviceAsyncDelegate::ErrorCallback& error_callback)
    : device_file_path(device_file_path),
      snapshot_file_path(snapshot_file_path),
      success_callback(success_callback),
      error_callback(error_callback) {
}

SnapshotRequestInfo::~SnapshotRequestInfo() {
}

////////////////////////////////////////////////////////////////////////////////
//                             SnapshotFileDetails                            //
////////////////////////////////////////////////////////////////////////////////

SnapshotFileDetails::SnapshotFileDetails(
    const SnapshotRequestInfo& request_info,
    const base::PlatformFileInfo& file_info)
    : request_info_(request_info),
      file_info_(file_info),
      bytes_written_(0),
      error_occurred_(false) {
}

SnapshotFileDetails::~SnapshotFileDetails() {
}

void SnapshotFileDetails::set_error_occurred(bool error) {
  error_occurred_ = error;
}

bool SnapshotFileDetails::AddBytesWritten(uint32 bytes_written) {
  if ((bytes_written == 0) ||
      (bytes_written_ > kuint32max - bytes_written) ||
      (bytes_written_ + bytes_written > file_info_.size))
    return false;

  bytes_written_ += bytes_written;
  return true;
}

bool SnapshotFileDetails::IsSnapshotFileWriteComplete() const {
  return !error_occurred_ && (bytes_written_ == file_info_.size);
}

uint32 SnapshotFileDetails::BytesToRead() const {
  // Read data in 1MB chunks.
  static const uint32 kReadChunkSize = 1024 * 1024;
  return std::min(
      kReadChunkSize,
      base::checked_numeric_cast<uint32>(file_info_.size) - bytes_written_);
}

}  // namespace chrome
