// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements FileStream for POSIX.

#ifndef NET_BASE_FILE_STREAM_POSIX_H_
#define NET_BASE_FILE_STREAM_POSIX_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/platform_file.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream_whence.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"

class FilePath;

namespace net {

class IOBuffer;

class NET_EXPORT FileStreamPosix {
 public:
  explicit FileStreamPosix(net::NetLog* net_log);
  FileStreamPosix(base::PlatformFile file, int flags, net::NetLog* net_log);
  ~FileStreamPosix();

  // FileStream implementations.
  void Close(const CompletionCallback& callback);
  void CloseSync();
  int Open(const FilePath& path, int open_flags,
           const CompletionCallback& callback);
  int OpenSync(const FilePath& path, int open_flags);
  bool IsOpen() const;
  int64 Seek(Whence whence, int64 offset);
  int64 Available();
  int Read(IOBuffer* buf, int buf_len, const CompletionCallback& callback);
  int ReadSync(char* buf, int buf_len);
  int ReadUntilComplete(char *buf, int buf_len);
  int Write(IOBuffer* buf, int buf_len, const CompletionCallback& callback);
  int WriteSync(const char* buf, int buf_len);
  int64 Truncate(int64 bytes);
  int Flush();
  void EnableErrorStatistics();
  void SetBoundNetLogSource(
      const net::BoundNetLog& owner_bound_net_log);
  base::PlatformFile GetPlatformFileForTesting();

 private:
  // Called when the file_ is closed asynchronously.
  void OnClosed();

  // Called when Read() or Write() is completed (used only for POSIX).
  // |result| contains the result as a network error code.
  void OnIOComplete(int* result);

  // Waits until the in-flight async open/close/read/write operation is
  // complete.
  void WaitForIOCompletion();

  base::PlatformFile file_;
  int open_flags_;
  bool auto_closed_;
  bool record_uma_;
  net::BoundNetLog bound_net_log_;
  base::WeakPtrFactory<FileStreamPosix> weak_ptr_factory_;
  CompletionCallback callback_;
  scoped_ptr<base::WaitableEvent> on_io_complete_;

  DISALLOW_COPY_AND_ASSIGN(FileStreamPosix);
};

}  // namespace net

#endif  // NET_BASE_FILE_STREAM_POSIX_H
