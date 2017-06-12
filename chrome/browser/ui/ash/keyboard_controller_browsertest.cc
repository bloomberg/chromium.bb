// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/keyboard/content/keyboard_constants.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_util.h"

namespace {
const int kKeyboardHeightForTest = 100;
}  // namespace

class VirtualKeyboardWebContentTest : public InProcessBrowserTest {
 public:
  VirtualKeyboardWebContentTest() {}
  ~VirtualKeyboardWebContentTest() override {}

  void SetUp() override {
    ui::SetUpInputMethodFactoryForTesting();
    InProcessBrowserTest::SetUp();
  }

  // Ensure that the virtual keyboard is enabled.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
  }

  keyboard::KeyboardUI* ui() {
    return keyboard::KeyboardController::GetInstance()->ui();
  }

 protected:
  void FocusEditableNodeAndShowKeyboard(const gfx::Rect& init_bounds) {
    client.reset(new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
    ui::InputMethod* input_method = ui()->GetInputMethod();
    input_method->SetFocusedTextInputClient(client.get());
    input_method->ShowImeIfNeeded();
    // Mock window.resizeTo that is expected to be called after navigate to a
    // new virtual keyboard.
    ui()->GetKeyboardWindow()->SetBounds(init_bounds);
  }

  void FocusNonEditableNode() {
    client.reset(new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_NONE));
    ui::InputMethod* input_method = ui()->GetInputMethod();
    input_method->SetFocusedTextInputClient(client.get());
  }

  void MockEnableIMEInDifferentExtension(const std::string& url,
                                         const gfx::Rect& init_bounds) {
    keyboard::SetOverrideContentUrl(GURL(url));
    keyboard::KeyboardController::GetInstance()->Reload();
    // Mock window.resizeTo that is expected to be called after navigate to a
    // new virtual keyboard.
    ui()->GetKeyboardWindow()->SetBounds(init_bounds);
  }

  bool IsKeyboardVisible() const {
    return keyboard::KeyboardController::GetInstance()->keyboard_visible();
  }

 private:
  std::unique_ptr<ui::DummyTextInputClient> client;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardWebContentTest);
};

// Test for crbug.com/404340. After enabling an IME in a different extension,
// its virtual keyboard should not become visible if previous one is not.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardWebContentTest,
                       EnableIMEInDifferentExtension) {
  gfx::Rect test_bounds(0, 0, 0, kKeyboardHeightForTest);
  FocusEditableNodeAndShowKeyboard(test_bounds);
  EXPECT_TRUE(IsKeyboardVisible());
  FocusNonEditableNode();
  EXPECT_FALSE(IsKeyboardVisible());

  MockEnableIMEInDifferentExtension("chrome-extension://domain-1", test_bounds);
  // Keyboard should not become visible if previous keyboard is not.
  EXPECT_FALSE(IsKeyboardVisible());

  FocusEditableNodeAndShowKeyboard(test_bounds);
  // Keyboard should become visible after focus on an editable node.
  EXPECT_TRUE(IsKeyboardVisible());

  // Simulate hide keyboard by pressing hide key on the virtual keyboard.
  keyboard::KeyboardController::GetInstance()->HideKeyboard(
      keyboard::KeyboardController::HIDE_REASON_MANUAL);
  EXPECT_FALSE(IsKeyboardVisible());

  MockEnableIMEInDifferentExtension("chrome-extension://domain-2", test_bounds);
  // Keyboard should not become visible if previous keyboard is not, even if it
  // is currently focused on an editable node.
  EXPECT_FALSE(IsKeyboardVisible());
}

// Test for crbug.com/489366. In FLOATING mode, switch to a new IME in a
// different extension should exit FLOATING mode and position the new IME in
// FULL_WIDTH mode.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardWebContentTest,
                       IMEInDifferentExtensionNotCentered) {
  gfx::Rect test_bounds(0, 0, 0, kKeyboardHeightForTest);
  FocusEditableNodeAndShowKeyboard(test_bounds);
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::GetInstance();
  const gfx::Rect& screen_bounds = ash::Shell::GetPrimaryRootWindow()->bounds();
  gfx::Rect keyboard_bounds = controller->GetContainerWindow()->bounds();
  EXPECT_EQ(kKeyboardHeightForTest, keyboard_bounds.height());
  EXPECT_EQ(screen_bounds.height(),
            keyboard_bounds.height() + keyboard_bounds.y());
  controller->SetKeyboardMode(keyboard::FLOATING);
  // Move keyboard to a random place.
  ui()->GetKeyboardWindow()->SetBounds(gfx::Rect(50, 50, 50, 50));
  EXPECT_EQ(gfx::Rect(50, 50, 50, 50),
            controller->GetContainerWindow()->bounds());

  MockEnableIMEInDifferentExtension("chrome-extension://domain-1", test_bounds);
  keyboard_bounds = controller->GetContainerWindow()->bounds();
  EXPECT_EQ(kKeyboardHeightForTest, keyboard_bounds.height());
  EXPECT_EQ(screen_bounds.height(),
            keyboard_bounds.height() + keyboard_bounds.y());
}

class VirtualKeyboardAppWindowTest : public extensions::PlatformAppBrowserTest {
 public:
  VirtualKeyboardAppWindowTest() {}
  ~VirtualKeyboardAppWindowTest() override {}

  // Ensure that the virtual keyboard is enabled.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardAppWindowTest);
};

// Tests that ime window won't overscroll. See crbug.com/529880.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardAppWindowTest,
                       DisableOverscrollForImeWindow) {
  scoped_refptr<extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", "test extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2)
                           .Set("background",
                                extensions::DictionaryBuilder()
                                    .Set("scripts", extensions::ListBuilder()
                                                        .Append("background.js")
                                                        .Build())
                                    .Build())
                           .Build())
          .Build();

  extension_service()->AddExtension(extension.get());
  extensions::AppWindow::CreateParams non_ime_params;
  non_ime_params.frame = extensions::AppWindow::FRAME_NONE;
  extensions::AppWindow* non_ime_app_window = CreateAppWindowFromParams(
      browser()->profile(), extension.get(), non_ime_params);
  int non_ime_window_visible_height = non_ime_app_window->web_contents()
                                          ->GetRenderWidgetHostView()
                                          ->GetVisibleViewportSize()
                                          .height();

  extensions::AppWindow::CreateParams ime_params;
  ime_params.frame = extensions::AppWindow::FRAME_NONE;
  ime_params.is_ime_window = true;
  extensions::AppWindow* ime_app_window = CreateAppWindowFromParams(
      browser()->profile(), extension.get(), ime_params);
  int ime_window_visible_height = ime_app_window->web_contents()
                                      ->GetRenderWidgetHostView()
                                      ->GetVisibleViewportSize()
                                      .height();

  ASSERT_EQ(non_ime_window_visible_height, ime_window_visible_height);
  ASSERT_TRUE(ime_window_visible_height > 0);

  int screen_height = ash::Shell::GetPrimaryRootWindow()->bounds().height();
  gfx::Rect test_bounds(0, 0, 0, screen_height - ime_window_visible_height + 1);
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::GetInstance();
  controller->ShowKeyboard(true);
  controller->ui()->GetKeyboardWindow()->SetBounds(test_bounds);
  gfx::Rect keyboard_bounds = controller->GetContainerWindow()->bounds();
  // Starts overscroll.
  controller->NotifyKeyboardBoundsChanging(keyboard_bounds);

  // Non ime window should have smaller visible view port due to overlap with
  // virtual keyboard.
  EXPECT_LT(non_ime_app_window->web_contents()
                ->GetRenderWidgetHostView()
                ->GetVisibleViewportSize()
                .height(),
            non_ime_window_visible_height);
  // Ime window should have not be affected by virtual keyboard.
  EXPECT_EQ(ime_app_window->web_contents()
                ->GetRenderWidgetHostView()
                ->GetVisibleViewportSize()
                .height(),
            ime_window_visible_height);
}
