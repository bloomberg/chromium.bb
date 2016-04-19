// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "components/mus/public/interfaces/window_server_test.mojom.h"
#include "services/shell/public/cpp/shell_test.h"

namespace mash {
namespace wm {

void RunCallback(bool* success, const base::Closure& callback, bool result) {
  *success = result;
  callback.Run();
}

class AppLaunchTest : public shell::test::ShellTest {
 public:
  AppLaunchTest() : ShellTest("exe:mash_unittests") {}
  ~AppLaunchTest() override {}

 private:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch("use-test-config");
    ShellTest::SetUp();
  }

  DISALLOW_COPY_AND_ASSIGN(AppLaunchTest);
};

TEST_F(AppLaunchTest, TestQuickLaunch) {
  connector()->Connect("mojo:desktop_wm");
  connector()->Connect("mojo:quick_launch");

  mus::mojom::WindowServerTestPtr test_interface;
  connector()->ConnectToInterface("mojo:mus", &test_interface);

  base::RunLoop run_loop;
  bool success = false;
  test_interface->EnsureClientHasDrawnWindow(
      "mojo:quick_launch",
      base::Bind(&RunCallback, &success, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(success);
}

}  // namespace wm
}  // namespace mash
