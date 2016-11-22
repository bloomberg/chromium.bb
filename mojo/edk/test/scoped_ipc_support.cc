// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/test/scoped_ipc_support.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
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

ScopedIPCSupport::ScopedIPCSupport(
    scoped_refptr<base::TaskRunner> io_thread_task_runner)
    : shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED) {
  g_io_task_runner = io_thread_task_runner.get();
  InitIPCSupport(this, io_thread_task_runner);
}

ScopedIPCSupport::~ScopedIPCSupport() {
  // ShutdownIPCSupport always runs OnShutdownComplete on the current
  // ThreadTaskRunnerHandle if set. Otherwise it's run on the IPC thread. We
  // account for both possibilities here to avoid unnecessarily starting a new
  // MessageLoop or blocking the existing one.
  //
  // TODO(rockot): Clean this up. ShutdownIPCSupport should probably always call
  // call OnShutdownComplete from the IPC thread.
  ShutdownIPCSupport();
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::RunLoop run_loop;
    shutdown_closure_ = base::Bind(IgnoreResult(&base::TaskRunner::PostTask),
                                   base::ThreadTaskRunnerHandle::Get(),
                                   FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  } else {
    shutdown_event_.Wait();
  }
}

void ScopedIPCSupport::OnShutdownComplete() {
  if (!shutdown_closure_.is_null())
    shutdown_closure_.Run();
  else
    shutdown_event_.Signal();
}

}  // namespace test
}  // namespace edk
}  // namespace mojo
