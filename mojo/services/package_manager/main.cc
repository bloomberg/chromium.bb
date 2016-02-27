// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/c/system/main.h"
#include "mojo/services/package_manager/package_manager.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace mojo {
const size_t kMaxBlockingPoolThreads = 3;

MojoResult Run(MojoHandle shell_handle) {
  scoped_ptr<base::AtExitManager> at_exit;
  if (!base::CommandLine::InitializedForCurrentProcess() ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch("single-process")) {
    at_exit.reset(new base::AtExitManager);
  }

  {
    scoped_ptr<base::MessageLoop> loop(
        new base::MessageLoop(common::MessagePumpMojo::Create()));
    scoped_refptr<base::SequencedWorkerPool> blocking_pool(
        new base::SequencedWorkerPool(kMaxBlockingPoolThreads,
                                      "blocking_pool"));
    scoped_ptr<mojo::ShellClient> shell_client(
        new package_manager::PackageManager(blocking_pool.get(), nullptr));
    mojo::ShellConnection connection(
        shell_client.get(), MakeRequest<mojo::shell::mojom::ShellClient>(
            MakeScopedHandle(MessagePipeHandle(shell_handle))));
    loop->Run();
    blocking_pool->Shutdown();
    loop.reset();
    shell_client.reset();
  }
  return MOJO_RESULT_OK;
}

}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  return mojo::Run(shell_handle);
}
