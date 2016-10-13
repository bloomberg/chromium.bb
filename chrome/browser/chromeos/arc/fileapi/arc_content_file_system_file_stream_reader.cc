// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_file_stream_reader.h"

#include "net/base/net_errors.h"

namespace arc {

ArcContentFileSystemFileStreamReader::ArcContentFileSystemFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read) {}

ArcContentFileSystemFileStreamReader::~ArcContentFileSystemFileStreamReader() =
    default;

int ArcContentFileSystemFileStreamReader::Read(
    net::IOBuffer* buffer,
    int buffer_length,
    const net::CompletionCallback& callback) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int64_t ArcContentFileSystemFileStreamReader::GetLength(
    const net::Int64CompletionCallback& callback) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

}  // namespace arc
