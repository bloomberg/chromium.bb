// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/keyboard/keyboard_constants.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_proxy.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

namespace {
const int kKeyboardHeightForTest = 100;
}  // namespace

class VirtualKeyboardWebContentTest : public InProcessBrowserTest {
 public:
  VirtualKeyboardWebContentTest() {};
  virtual ~VirtualKeyboardWebContentTest() {};

  virtual void SetUp() OVERRIDE {
    ui::SetUpInputMethodFactoryForTesting();
    InProcessBrowserTest::SetUp();
  }

  // Ensure that the virtual keyboard is enabled.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
  }

  keyboard::KeyboardControllerProxy* proxy() {
    return keyboard::KeyboardController::GetInstance()->proxy();
  }

 protected:
  void FocusEditableNodeAndShowKeyboard() {
    client.reset(new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
    ui::InputMethod* input_method = proxy()->GetInputMethod();
    input_method->SetFocusedTextInputClient(client.get());
    input_method->ShowImeIfNeeded();
    ResizeKeyboardWindow();
  }

  void FocusNonEditableNode() {
    client.reset(new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_NONE));
    ui::InputMethod* input_method = proxy()->GetInputMethod();
    input_method->SetFocusedTextInputClient(client.get());
  }

  void MockEnableIMEInDifferentExtension(const std::string& url) {
    keyboard::SetOverrideContentUrl(GURL(url));
    keyboard::KeyboardController::GetInstance()->Reload();
    ResizeKeyboardWindow();
  }

  bool IsKeyboardVisible() const {
    return keyboard::KeyboardController::GetInstance()->keyboard_visible();
  }

 private:
  // Mock window.resizeTo that is expected to be called after navigate to a new
  // virtual keyboard.
  void ResizeKeyboardWindow() {
    gfx::Rect bounds = proxy()->GetKeyboardWindow()->bounds();
    proxy()->GetKeyboardWindow()->SetBounds(gfx::Rect(
        bounds.x(),
        bounds.bottom() - kKeyboardHeightForTest,
        bounds.width(),
        kKeyboardHeightForTest));
  }

  scoped_ptr<ui::DummyTextInputClient> client;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardWebContentTest);
};

// Test for crbug.com/404340. After enabling an IME in a different extension,
// its virtual keyboard should not become visible if previous one is not.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardWebContentTest,
                       EnableIMEInDifferentExtension) {
  FocusEditableNodeAndShowKeyboard();
  EXPECT_TRUE(IsKeyboardVisible());
  FocusNonEditableNode();
  EXPECT_FALSE(IsKeyboardVisible());

  MockEnableIMEInDifferentExtension("chrome-extension://domain-1");
  // Keyboard should not become visible if previous keyboard is not.
  EXPECT_FALSE(IsKeyboardVisible());

  FocusEditableNodeAndShowKeyboard();
  // Keyboard should become visible after focus on an editable node.
  EXPECT_TRUE(IsKeyboardVisible());

  // Simulate hide keyboard by pressing hide key on the virtual keyboard.
  keyboard::KeyboardController::GetInstance()->HideKeyboard(
      keyboard::KeyboardController::HIDE_REASON_MANUAL);
  EXPECT_FALSE(IsKeyboardVisible());

  MockEnableIMEInDifferentExtension("chrome-extension://domain-2");
  // Keyboard should not become visible if previous keyboard is not, even if it
  // is currently focused on an editable node.
  EXPECT_FALSE(IsKeyboardVisible());
}
