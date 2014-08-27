// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_desktop_controller.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/aura/test/aura_test_base.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#endif

namespace extensions {

class ShellDesktopControllerTest : public aura::test::AuraTestBase {
 public:
  ShellDesktopControllerTest()
#if defined(OS_CHROMEOS)
      : power_manager_client_(NULL)
#endif
      {
  }
  virtual ~ShellDesktopControllerTest() {}

  virtual void SetUp() OVERRIDE {
#if defined(OS_CHROMEOS)
    scoped_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    power_manager_client_ = new chromeos::FakePowerManagerClient();
    dbus_setter->SetPowerManagerClient(make_scoped_ptr(power_manager_client_).
        PassAs<chromeos::PowerManagerClient>());
#endif
    aura::test::AuraTestBase::SetUp();
    controller_.reset(new ShellDesktopController());
  }

  virtual void TearDown() OVERRIDE {
    controller_.reset();
    aura::test::AuraTestBase::TearDown();
#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Shutdown();
#endif
  }

 protected:
  scoped_ptr<ShellDesktopController> controller_;

#if defined(OS_CHROMEOS)
  chromeos::FakePowerManagerClient* power_manager_client_;  // Not owned.
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellDesktopControllerTest);
};

#if defined(OS_CHROMEOS)
// Tests that a shutdown request is sent to the power manager when the power
// button is pressed.
TEST_F(ShellDesktopControllerTest, PowerButton) {
  // Ignore button releases.
  power_manager_client_->SendPowerButtonEvent(false /* down */,
                                              base::TimeTicks());
  EXPECT_EQ(0, power_manager_client_->num_request_shutdown_calls());

  // A button press should trigger a shutdown request.
  power_manager_client_->SendPowerButtonEvent(true /* down */,
                                              base::TimeTicks());
  EXPECT_EQ(1, power_manager_client_->num_request_shutdown_calls());
}
#endif

}  // namespace extensions
