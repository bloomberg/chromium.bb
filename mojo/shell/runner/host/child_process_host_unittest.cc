// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This file also tests child_process.*.

#include "mojo/shell/runner/host/child_process_host.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {

void ProcessReadyCallbackAdapater(const base::Closure& callback,
                                  base::ProcessId process_id) {
  callback.Run();
}

class ProcessDelegate : public edk::ProcessDelegate {
 public:
  ProcessDelegate() {}
  ~ProcessDelegate() override {}

 private:
  void OnShutdownComplete() override {}
  DISALLOW_COPY_AND_ASSIGN(ProcessDelegate);
};

#if defined(OS_ANDROID)
// TODO(qsr): Multiprocess shell tests are not supported on android.
#define MAYBE_StartJoin DISABLED_StartJoin
#else
#define MAYBE_StartJoin StartJoin
#endif  // defined(OS_ANDROID)
// Just tests starting the child process and joining it (without starting an
// app).
TEST(ChildProcessHostTest, MAYBE_StartJoin) {
  base::FilePath shell_dir;
  PathService::Get(base::DIR_MODULE, &shell_dir);
  base::MessageLoop message_loop(
      scoped_ptr<base::MessagePump>(new common::MessagePumpMojo()));
  scoped_refptr<base::SequencedWorkerPool> blocking_pool(
      new base::SequencedWorkerPool(3, "blocking_pool"));

  base::Thread io_thread("io_thread");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

  ProcessDelegate delegate;
  edk::InitIPCSupport(&delegate, io_thread.task_runner());

  ChildProcessHost child_process_host(blocking_pool.get(), false,
                                      base::FilePath());
  base::RunLoop run_loop;
  child_process_host.Start(
      base::Bind(&ProcessReadyCallbackAdapater, run_loop.QuitClosure()));
  run_loop.Run();

  child_process_host.ExitNow(123);
  int exit_code = child_process_host.Join();
  VLOG(2) << "Joined child: exit_code = " << exit_code;
  EXPECT_EQ(123, exit_code);
  blocking_pool->Shutdown();
  edk::ShutdownIPCSupport();
}

}  // namespace
}  // namespace shell
}  // namespace mojo
