// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/test/scoped_ipc_support.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder.h"

namespace mojo {
namespace edk {
namespace test {

namespace {
base::TaskRunner* g_io_task_runner = nullptr;
}

base::TaskRunner* GetIoTaskRunner() {
  return g_io_task_runner;
}

namespace internal {

ScopedIPCSupportHelper::ScopedIPCSupportHelper() {
}

ScopedIPCSupportHelper::~ScopedIPCSupportHelper() {
  ShutdownIPCSupport();
  run_loop_.Run();
}

void ScopedIPCSupportHelper::Init(
    ProcessDelegate* process_delegate,
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  io_thread_task_runner_ = io_thread_task_runner;
  InitIPCSupport(process_delegate, io_thread_task_runner_);
}

void ScopedIPCSupportHelper::OnShutdownCompleteImpl() {
  run_loop_.Quit();
}

}  // namespace internal

ScopedIPCSupport::ScopedIPCSupport(
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  g_io_task_runner = io_thread_task_runner.get();
  helper_.Init(this, std::move(io_thread_task_runner));
}

ScopedIPCSupport::~ScopedIPCSupport() {
}

void ScopedIPCSupport::OnShutdownComplete() {
  helper_.OnShutdownCompleteImpl();
}

}  // namespace test
}  // namespace edk
}  // namespace mojo
