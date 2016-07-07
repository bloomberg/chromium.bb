// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "services/shell/public/cpp/service_test.h"
#include "services/ui/public/interfaces/window_server_test.mojom.h"

namespace ash {
namespace mus {

void RunCallback(bool* success, const base::Closure& callback, bool result) {
  *success = result;
  callback.Run();
}

class AppLaunchTest : public shell::test::ServiceTest {
 public:
  AppLaunchTest() : ServiceTest("exe:mash_unittests") {}
  ~AppLaunchTest() override {}

 private:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch("use-test-config");
    ServiceTest::SetUp();
  }

  DISALLOW_COPY_AND_ASSIGN(AppLaunchTest);
};

TEST_F(AppLaunchTest, TestQuickLaunch) {
  connector()->Connect("mojo:ash");
  connector()->Connect("mojo:quick_launch");

  ::ui::mojom::WindowServerTestPtr test_interface;
  connector()->ConnectToInterface("mojo:ui", &test_interface);

  base::RunLoop run_loop;
  bool success = false;
  test_interface->EnsureClientHasDrawnWindow(
      "mojo:quick_launch",
      base::Bind(&RunCallback, &success, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(success);
}

}  // namespace mus
}  // namespace ash
