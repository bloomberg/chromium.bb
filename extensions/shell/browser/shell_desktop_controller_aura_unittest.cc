// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_desktop_controller_aura.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/test_app_window_contents.h"
#include "extensions/common/test_util.h"
#include "extensions/shell/browser/shell_app_delegate.h"
#include "extensions/shell/test/shell_test_base_aura.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ime/input_method_minimal.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#endif

namespace extensions {

class ShellDesktopControllerAuraTest : public ShellTestBaseAura {
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

    ShellTestBaseAura::SetUp();

    // The input method will be used for the next CreateInputMethod call,
    // causing the host to take ownership.
    ui::SetUpInputMethodForTesting(new ui::InputMethodMinimal(nullptr));

    controller_.reset(new ShellDesktopControllerAura());
  }

  void TearDown() override {
    controller_.reset();
    ShellTestBaseAura::TearDown();
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
  ui::InputMethod* input_method = controller_->host()->GetInputMethod();
  ASSERT_TRUE(input_method);

  // Set up a focused text input to receive the keypress event.
  ui::DummyTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  input_method->SetFocusedTextInputClient(&client);
  EXPECT_EQ(0, client.insert_char_count());

  // Dispatch a keypress on the window tree host to verify it is processed.
  ui::KeyEvent key_press(base::char16(97), ui::VKEY_A, ui::EF_NONE);
  ui::EventDispatchDetails details =
      controller_->host()->dispatcher()->DispatchEvent(
          controller_->host()->window(), &key_press);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_TRUE(key_press.handled());
  EXPECT_EQ(1, client.insert_char_count());

  // Clean up.
  input_method->DetachTextInputClient(&client);
}

// Tests the basic window layout.
TEST_F(ShellDesktopControllerAuraTest, FillLayout) {
  controller_->host()->SetBoundsInPixels(gfx::Rect(0, 0, 500, 700));

  scoped_refptr<Extension> extension = test_util::CreateEmptyExtension();
  AppWindow* app_window =
      controller_->CreateAppWindow(browser_context(), extension.get());

  content::WebContents* web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(browser_context()));
  TestAppWindowContents* app_window_contents =
      new TestAppWindowContents(web_contents);
  DCHECK_EQ(web_contents, app_window_contents->GetWebContents());

  // Init the ShellExtensionsWebContentsObserver.
  app_window->app_delegate()->InitWebContents(web_contents);
  EXPECT_TRUE(web_contents->GetMainFrame());

  app_window->Init(GURL(std::string()), app_window_contents,
                   web_contents->GetMainFrame(), AppWindow::CreateParams());

  aura::Window* root_window = controller_->host()->window();
  EXPECT_EQ(1u, root_window->children().size());

  // Test that reshaping the host window also resizes the child window.
  controller_->host()->SetBoundsInPixels(gfx::Rect(0, 0, 400, 300));

  EXPECT_EQ(400, root_window->bounds().width());
  EXPECT_EQ(400, root_window->children()[0]->bounds().width());
}

}  // namespace extensions
