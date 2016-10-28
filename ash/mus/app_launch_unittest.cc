// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/ui/public/interfaces/window_server_test.mojom.h"

namespace ash {
namespace mus {

void RunCallback(bool* success, const base::Closure& callback, bool result) {
  *success = result;
  callback.Run();
}

class AppLaunchTest : public service_manager::test::ServiceTest {
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

// Fails when the Ash material design shelf is enabled by default
// (ash::MaterialDesignController::IsShelfMaterial()). See
// crbug.com/660194 and crbug.com/642879.
// TODO(rockot): Reenable this test.
#if defined(USE_OZONE)
#define MAYBE_TestQuickLaunch TestQuickLaunch
#else
#define MAYBE_TestQuickLaunch DISABLED_TestQuickLaunch
#endif  // defined(USE_OZONE)
TEST_F(AppLaunchTest, MAYBE_TestQuickLaunch) {
  connector()->Connect("service:ash");
  connector()->Connect("service:quick_launch");

  ui::mojom::WindowServerTestPtr test_interface;
  connector()->ConnectToInterface("service:ui", &test_interface);

  base::RunLoop run_loop;
  bool success = false;
  test_interface->EnsureClientHasDrawnWindow(
      "service:quick_launch",
      base::Bind(&RunCallback, &success, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(success);
}

}  // namespace mus
}  // namespace ash
