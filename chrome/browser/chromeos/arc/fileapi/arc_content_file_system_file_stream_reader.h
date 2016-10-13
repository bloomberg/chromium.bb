// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_READER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_READER_H_

#include "storage/browser/fileapi/file_stream_reader.h"

namespace arc {

class ArcContentFileSystemFileStreamReader : public storage::FileStreamReader {
 public:
  ArcContentFileSystemFileStreamReader(const storage::FileSystemURL& url,
                                       int64_t offset,
                                       int64_t max_bytes_to_read);
  ~ArcContentFileSystemFileStreamReader() override;

  // storage::FileStreamReader override:
  int Read(net::IOBuffer* buffer,
           int buffer_length,
           const net::CompletionCallback& callback) override;
  int64_t GetLength(const net::Int64CompletionCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcContentFileSystemFileStreamReader);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_READER_H_
