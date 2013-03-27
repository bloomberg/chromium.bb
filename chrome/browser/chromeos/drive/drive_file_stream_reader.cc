// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_stream_reader.h"

#include "base/logging.h"

namespace drive {

DriveFileStreamReader::DriveFileStreamReader() {
}

DriveFileStreamReader::~DriveFileStreamReader() {
}

int DriveFileStreamReader::Read(net::IOBuffer* buf, int buf_len,
                                const net::CompletionCallback& callback) {
  // TODO(hidehiko): Implement this.
  NOTIMPLEMENTED();
  return 0;
}

int64 DriveFileStreamReader::GetLength(
    const net::Int64CompletionCallback& callback) {
  // TODO(hidehiko): Implement this.
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace drive
