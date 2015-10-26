// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This file also tests child_process.*.

#include "mojo/runner/child_process_host.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/runner/context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace runner {
namespace {

// Subclass just so we can observe |DidStart()|.
class TestChildProcessHost : public ChildProcessHost {
 public:
  explicit TestChildProcessHost(Context* context)
      : ChildProcessHost(context, false, base::FilePath()) {}
  ~TestChildProcessHost() override {}

  void DidStart(bool success) override {
    EXPECT_TRUE(success);
    ChildProcessHost::DidStart(success);
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestChildProcessHost);
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
  Context context(shell_dir, nullptr);
  base::MessageLoop message_loop(
      scoped_ptr<base::MessagePump>(new common::MessagePumpMojo()));
  context.Init();
  TestChildProcessHost child_process_host(&context);
  child_process_host.Start();
  message_loop.Run();
  child_process_host.ExitNow(123);
  int exit_code = child_process_host.Join();
  VLOG(2) << "Joined child: exit_code = " << exit_code;
  EXPECT_EQ(123, exit_code);

  context.Shutdown();
}

}  // namespace
}  // namespace runner
}  // namespace mojo
