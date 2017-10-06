// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_WIN_H_
#define CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_WIN_H_

#include <windows.h>

#include <string>

#include "base/files/platform_file.h"
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
    : public base::RefCountedThreadSafe<MemlogReceiverPipe>,
      public base::MessagePumpForIO::IOHandler {
 public:
  explicit MemlogReceiverPipe(base::ScopedPlatformFile handle);

  // Must be called on the IO thread.
  void StartReadingOnIOThread();

  void SetReceiver(scoped_refptr<base::TaskRunner> task_runner,
                   scoped_refptr<MemlogStreamReceiver> receiver);

  // Callback that indicates an error has occurred and the connection should
  // be closed. May be called more than once in an error condition.
  void ReportError();

 private:
  friend class base::RefCountedThreadSafe<MemlogReceiverPipe>;
  ~MemlogReceiverPipe() override;

  void ReadUntilBlocking();
  void ZeroOverlapped();

  // IOHandler implementation.
  void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                     DWORD bytes_transfered,
                     DWORD error) override;

  scoped_refptr<base::TaskRunner> receiver_task_runner_;
  scoped_refptr<MemlogStreamReceiver> receiver_;

  base::win::ScopedHandle handle_;
  base::MessagePumpForIO::IOContext context_;

  bool read_outstanding_ = false;

  std::unique_ptr<char[]> read_buffer_;

  DISALLOW_COPY_AND_ASSIGN(MemlogReceiverPipe);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_WIN_H_
