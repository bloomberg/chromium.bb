// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_POSIX_H_
#define CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_POSIX_H_

#include <string>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace profiling {

class MemlogStreamReceiver;

class MemlogReceiverPipe
    : public base::RefCountedThreadSafe<MemlogReceiverPipe> {
 public:
  explicit MemlogReceiverPipe(base::ScopedFD fd);

  void ReadUntilBlocking();

  void SetReceiver(scoped_refptr<base::TaskRunner> task_runner,
                   scoped_refptr<MemlogStreamReceiver> receiver);

  // TODO(ajwong): Remove when file watching is moved from the PipeServer to
  // the MemlogReceiverPipe.
  base::MessageLoopForIO::FileDescriptorWatcher* controller() {
    return &controller_;
  }

 private:
  friend class base::RefCountedThreadSafe<MemlogReceiverPipe>;
  ~MemlogReceiverPipe();

  mojo::edk::ScopedPlatformHandle handle_;
  base::MessageLoopForIO::FileDescriptorWatcher controller_;
  std::unique_ptr<char[]> read_buffer_;

  scoped_refptr<base::TaskRunner> receiver_task_runner_;
  scoped_refptr<MemlogStreamReceiver> receiver_;

  // Make base::UnixDomainSocket::RecvMsg happy.
  std::vector<base::ScopedFD>* dummy_for_receive_;

  DISALLOW_COPY_AND_ASSIGN(MemlogReceiverPipe);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_POSIX_H_
