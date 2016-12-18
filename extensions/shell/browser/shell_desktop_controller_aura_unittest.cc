// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_desktop_controller_aura.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ime/input_method_minimal.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#endif

namespace extensions {

// TODO(michaelpg): Why do we use AuraTestBase when ShellDesktopControllerAura
// already creates a screen and root window host itself?
class ShellDesktopControllerAuraTest : public aura::test::AuraTestBase {
 public:
  ShellDesktopControllerAuraTest()
#if defined(OS_CHROMEOS)
      : power_manager_client_(NULL)
#endif
      {
  }
  ~ShellDesktopControllerAuraTest() override {}

  void SetUp() override {
#if defined(OS_CHROMEOS)
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    power_manager_client_ = new chromeos::FakePowerManagerClient();
    dbus_setter->SetPowerManagerClient(base::WrapUnique(power_manager_client_));
#endif
    aura::test::AuraTestBase::SetUp();

    // The input method will be used for the next CreateInputMethod call,
    // causing the host to take ownership.
    ui::SetUpInputMethodForTesting(new ui::InputMethodMinimal(nullptr));

    controller_.reset(new ShellDesktopControllerAura());
  }

  void TearDown() override {
    controller_.reset();
    aura::test::AuraTestBase::TearDown();
#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Shutdown();
#endif
  }

 protected:
  std::unique_ptr<ShellDesktopControllerAura> controller_;

#if defined(OS_CHROMEOS)
  chromeos::FakePowerManagerClient* power_manager_client_;  // Not owned.
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellDesktopControllerAuraTest);
};

#if defined(OS_CHROMEOS)
// Tests that a shutdown request is sent to the power manager when the power
// button is pressed.
TEST_F(ShellDesktopControllerAuraTest, PowerButton) {
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

// Tests that basic input events are handled and forwarded to the host.
// TODO(michaelpg): Test other types of input.
TEST_F(ShellDesktopControllerAuraTest, InputEvents) {
  ui::InputMethod* input_method = controller_->host_->GetInputMethod();
  ASSERT_TRUE(input_method);

  // Set up a focused text input to receive the keypress event.
  ui::DummyTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  input_method->SetFocusedTextInputClient(&client);
  EXPECT_EQ(0, client.insert_char_count());

  // Dispatch a keypress on the window tree host to verify it is processed.
  ui::KeyEvent key_press(base::char16(97), ui::VKEY_A, ui::EF_NONE);
  ui::EventDispatchDetails details =
      controller_->host_->dispatcher()->DispatchEvent(
          controller_->host_->window(), &key_press);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_TRUE(key_press.handled());
  EXPECT_EQ(1, client.insert_char_count());

  // Clean up.
  input_method->DetachTextInputClient(&client);
}

}  // namespace extensions
