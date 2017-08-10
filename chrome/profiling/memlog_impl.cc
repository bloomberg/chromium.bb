// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_impl.h"

#include "chrome/profiling/memlog_receiver_pipe.h"
#include "content/public/child/child_thread.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace profiling {

MemlogImpl::MemlogImpl()
    : io_runner_(content::ChildThread::Get()->GetIOTaskRunner()),
      connection_manager_(
          new MemlogConnectionManager(io_runner_, &backtrace_storage_),
          DeleteOnRunner(FROM_HERE, io_runner_.get())) {}

MemlogImpl::~MemlogImpl() {}

void MemlogImpl::AddSender(base::ProcessId pid,
                           mojo::ScopedHandle sender_pipe) {
  base::PlatformFile platform_file;
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::UnwrapPlatformFile(std::move(sender_pipe), &platform_file));

  // MemlogConnectionManager is deleted on the IOThread thus using
  // base::Unretained() is safe here.
  io_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MemlogConnectionManager::OnNewConnection,
                                base::Unretained(connection_manager_.get()),
                                base::ScopedPlatformFile(platform_file), pid));
}

void MemlogImpl::DumpProcess(base::ProcessId pid,
                             mojo::ScopedHandle output_file) {
  base::PlatformFile platform_file;
  MojoResult result =
      UnwrapPlatformFile(std::move(output_file), &platform_file);
  if (result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Failed to unwrap output file " << result;
    return;
  }
  base::File file(platform_file);
  io_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MemlogConnectionManager::DumpProcess,
                                base::Unretained(connection_manager_.get()),
                                pid, std::move(file)));
}

}  // namespace profiling
