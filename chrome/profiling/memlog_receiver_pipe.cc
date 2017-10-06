// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_receiver_pipe.h"

#include "base/bind.h"
#include "base/task_runner.h"
#include "chrome/profiling/memlog_stream_receiver.h"

namespace profiling {

void ReceiverPipeStreamDataThunk(
    scoped_refptr<base::TaskRunner> pipe_task_runner,
    scoped_refptr<MemlogReceiverPipe> pipe,
    scoped_refptr<MemlogStreamReceiver> receiver,
    std::unique_ptr<char[]> data,
    size_t size) {
  if (!receiver->OnStreamData(std::move(data), size)) {
    pipe_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&MemlogReceiverPipe::ReportError, pipe));
  }
}

}  // namespace profiling
