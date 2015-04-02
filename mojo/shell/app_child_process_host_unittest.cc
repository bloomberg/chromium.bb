// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This file also tests app_child_process.*.

#include "mojo/shell/app_child_process_host.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/common/message_pump_mojo.h"
#include "mojo/shell/context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {

// Subclass just so we can observe |DidStart()|.
class TestAppChildProcessHost : public AppChildProcessHost {
 public:
  explicit TestAppChildProcessHost(Context* context)
      : AppChildProcessHost(context) {}
  ~TestAppChildProcessHost() override {}

  void DidStart(bool success) override {
    EXPECT_TRUE(success);
    AppChildProcessHost::DidStart(success);
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAppChildProcessHost);
};

#if defined(OS_ANDROID)
// TODO(qsr): Multiprocess shell tests are not supported on android.
#define MAYBE_StartJoin DISABLED_StartJoin
#else
#define MAYBE_StartJoin StartJoin
#endif  // defined(OS_ANDROID)
// Just tests starting the child process and joining it (without starting an
// app).
TEST(AppChildProcessHostTest, MAYBE_StartJoin) {
  Context context;
  base::MessageLoop message_loop(
      scoped_ptr<base::MessagePump>(new common::MessagePumpMojo()));
  context.Init();
  TestAppChildProcessHost app_child_process_host(&context);
  app_child_process_host.Start();
  message_loop.Run();
  app_child_process_host.ExitNow(123);
  int exit_code = app_child_process_host.Join();
  VLOG(2) << "Joined child: exit_code = " << exit_code;
  EXPECT_EQ(123, exit_code);

  context.Shutdown();
}

}  // namespace
}  // namespace shell
}  // namespace mojo
