// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_POSIX_H_
#define CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_POSIX_H_

#include <string>

#include "base/files/platform_file.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "chrome/profiling/memlog_receiver_pipe.h"

namespace profiling {

class MemlogReceiverPipe : public MemlogReceiverPipeBase,
                           public base::MessageLoopForIO::Watcher {
 public:
  explicit MemlogReceiverPipe(mojo::edk::ScopedPlatformHandle handle);

  // Must be called on the IO thread.
  void StartReadingOnIOThread();

 private:
  ~MemlogReceiverPipe() override;

  // MessageLoopForIO::Watcher implementation.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  base::MessageLoopForIO::FileDescriptorWatcher controller_;
  std::unique_ptr<char[]> read_buffer_;

  DISALLOW_COPY_AND_ASSIGN(MemlogReceiverPipe);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_POSIX_H_
