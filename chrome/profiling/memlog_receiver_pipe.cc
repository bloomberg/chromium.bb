// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_receiver_pipe.h"

#include "base/bind.h"
#include "base/task_runner.h"
#include "chrome/profiling/memlog_stream_receiver.h"

namespace profiling {

MemlogReceiverPipeBase::MemlogReceiverPipeBase(
    mojo::edk::ScopedPlatformHandle handle)
    : handle_(std::move(handle)) {}

MemlogReceiverPipeBase::~MemlogReceiverPipeBase() = default;

void MemlogReceiverPipeBase::SetReceiver(
    scoped_refptr<base::TaskRunner> task_runner,
    scoped_refptr<MemlogStreamReceiver> receiver) {
  receiver_task_runner_ = std::move(task_runner);
  receiver_ = receiver;
}

void MemlogReceiverPipeBase::ReportError() {
  handle_.reset();
}

void MemlogReceiverPipeBase::OnStreamDataThunk(
    scoped_refptr<base::TaskRunner> pipe_task_runner,
    std::unique_ptr<char[]> data,
    size_t size) {
  if (!receiver_->OnStreamData(std::move(data), size)) {
    pipe_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&MemlogReceiverPipeBase::ReportError, this));
  }
}

}  // namespace profiling
