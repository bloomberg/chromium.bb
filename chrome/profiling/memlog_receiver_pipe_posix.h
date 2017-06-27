// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_POSIX_H_
#define CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_POSIX_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace profiling {

class MemlogStreamReceiver;

class MemlogReceiverPipe
    : public base::RefCountedThreadSafe<MemlogReceiverPipe> {
 public:
  class CompletionThunk : public base::MessageLoopForIO::Watcher {
   public:
    using Callback = base::RepeatingCallback<void(int)>;

    CompletionThunk(int fd, Callback cb);
    ~CompletionThunk() override;

    void set_callback(Callback cb) { callback_ = cb; }

    void OnFileCanReadWithoutBlocking(int fd) override;
    void OnFileCanWriteWithoutBlocking(int fd) override;

   private:
    base::MessageLoopForIO::FileDescriptorWatcher controller_;

    int fd_;
    Callback callback_;
  };

  explicit MemlogReceiverPipe(std::unique_ptr<CompletionThunk> thunk);

  void StartReadingOnIOThread();

  int GetRemoteProcessID();
  void SetReceiver(scoped_refptr<base::TaskRunner> task_runner,
                   scoped_refptr<MemlogStreamReceiver> receiver);

 private:
  friend class base::RefCountedThreadSafe<MemlogReceiverPipe>;
  ~MemlogReceiverPipe();

  std::unique_ptr<CompletionThunk> thunk_;

  scoped_refptr<base::TaskRunner> receiver_task_runner_;
  scoped_refptr<MemlogStreamReceiver> receiver_;

  DISALLOW_COPY_AND_ASSIGN(MemlogReceiverPipe);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_POSIX_H_
