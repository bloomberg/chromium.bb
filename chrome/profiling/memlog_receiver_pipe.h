// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_H_
#define CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_H_

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

namespace base {
class TaskRunner;
}

namespace profiling {

class MemlogStreamReceiver;

// Base class for the platform-specific receiver pipes. Since there is only
// ever one actual implementation of this in the system, those implementations
// are called "MemlogReceiverPipe" and the common functions are not
// virtual. This class is just for the shared implementation.
class MemlogReceiverPipeBase
    : public base::RefCountedThreadSafe<MemlogReceiverPipeBase> {
 public:
  void SetReceiver(scoped_refptr<base::TaskRunner> task_runner,
                   scoped_refptr<MemlogStreamReceiver> receiver);

 protected:
  friend class base::RefCountedThreadSafe<MemlogReceiverPipeBase>;

  explicit MemlogReceiverPipeBase(mojo::edk::ScopedPlatformHandle handle);
  virtual ~MemlogReceiverPipeBase();

  // Callback that indicates an error has occurred and the connection should
  // be closed. May be called more than once in an error condition.
  void ReportError();

  // Called on the receiver task runner's thread to call the OnStreamData
  // function and post the error back to the pipe on the correct thread if one
  // occurs.
  void OnStreamDataThunk(scoped_refptr<base::TaskRunner> pipe_task_runner,
                         std::unique_ptr<char[]> data,
                         size_t size);

  scoped_refptr<base::TaskRunner> receiver_task_runner_;
  scoped_refptr<MemlogStreamReceiver> receiver_;

  mojo::edk::ScopedPlatformHandle handle_;
};

}  // namespace profiling

// Define the platform-specific specialization.
#if defined(OS_WIN)
#include "chrome/profiling/memlog_receiver_pipe_win.h"
#else
#include "chrome/profiling/memlog_receiver_pipe_posix.h"
#endif

#endif  // CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_H_
