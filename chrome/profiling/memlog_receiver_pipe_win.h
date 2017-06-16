// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_WIN_H_
#define CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_WIN_H_

#include <windows.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_win.h"
#include "base/strings/string16.h"
#include "build/build_config.h"

namespace base {
class TaskRunner;
}

namespace profiling {

class MemlogStreamReceiver;

class MemlogReceiverPipe
    : public base::RefCountedThreadSafe<MemlogReceiverPipe> {
 public:
  // RegisterIOHandler can't change the callback ID of a handle once it has
  // been registered. This class allows switching the callbacks from the
  // server to the pipe once the pipe is created while keeping the IOHandler
  // attached to the handle the same.
  class CompletionThunk : public base::MessagePumpForIO::IOHandler {
   public:
    using Callback = base::RepeatingCallback<void(size_t, DWORD)>;

    // Takes ownership of HANDLE and closes it when the class goes out of scope.
    CompletionThunk(HANDLE handle, Callback cb);
    ~CompletionThunk();

    void set_callback(Callback cb) { callback_ = cb; }

    HANDLE handle() { return handle_; }
    OVERLAPPED* overlapped() { return &context_.overlapped; }

    void ZeroOverlapped();

   private:
    // IOHandler implementation.
    void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                       DWORD bytes_transfered,
                       DWORD error) override;

    base::MessagePumpForIO::IOContext context_;

    HANDLE handle_;
    Callback callback_;

    DISALLOW_COPY_AND_ASSIGN(CompletionThunk);
  };

  explicit MemlogReceiverPipe(std::unique_ptr<CompletionThunk> thunk);
  ~MemlogReceiverPipe();

  void StartReadingOnIOThread();

  int GetRemoteProcessID();
  void SetReceiver(scoped_refptr<base::TaskRunner> task_runner,
                   scoped_refptr<MemlogStreamReceiver> receiver);

 private:
  void OnIOCompleted(size_t bytes_transfered, DWORD error);

  void ReadUntilBlocking();

  std::unique_ptr<CompletionThunk> thunk_;

  scoped_refptr<base::TaskRunner> receiver_task_runner_;
  scoped_refptr<MemlogStreamReceiver> receiver_;

  bool read_outstanding_ = false;

  std::unique_ptr<char[]> read_buffer_;

  DISALLOW_COPY_AND_ASSIGN(MemlogReceiverPipe);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_WIN_H_
