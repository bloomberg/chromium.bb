// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_READER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_READER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/handle.h"
#include "net/base/completion_callback.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "url/gurl.h"

namespace net {
class FileStream;
class IOBuffer;
}

namespace arc {

// FileStreamReader implementation for ARC content file system.
class ArcContentFileSystemFileStreamReader : public storage::FileStreamReader {
 public:
  ArcContentFileSystemFileStreamReader(const GURL& arc_url, int64_t offset);
  ~ArcContentFileSystemFileStreamReader() override;

  // storage::FileStreamReader override:
  int Read(net::IOBuffer* buffer,
           int buffer_length,
           const net::CompletionCallback& callback) override;
  int64_t GetLength(const net::Int64CompletionCallback& callback) override;

 private:
  void OnOpenFile(scoped_refptr<net::IOBuffer> buf,
                  int buffer_length,
                  const net::CompletionCallback& callback,
                  mojo::ScopedHandle handle);

  GURL arc_url_;
  int64_t offset_;

  std::unique_ptr<net::FileStream> file_stream_;

  base::WeakPtrFactory<ArcContentFileSystemFileStreamReader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcContentFileSystemFileStreamReader);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_READER_H_
