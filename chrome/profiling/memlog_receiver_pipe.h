// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_H_
#define CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include "memlog_receiver_pipe_win.h"
#else
#include "memlog_receiver_pipe_posix.h"
#endif

namespace profiling {

// Called on the receiver task runner's thread to call the OnStreamData
// function and post the error back to the pipe if one occurs.
void ReceiverPipeStreamDataThunk(
    scoped_refptr<base::TaskRunner> pipe_task_runner,
    scoped_refptr<MemlogReceiverPipe> pipe,
    scoped_refptr<MemlogStreamReceiver> receiver,
    std::unique_ptr<char[]> data,
    size_t size);

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_H_
