// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/keyboard/keyboard_constants.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_proxy.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

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
    command_line->AppendSwitch(
        keyboard::switches::kEnableInputView);
  }

  keyboard::KeyboardControllerProxy* proxy() {
    return keyboard::KeyboardController::GetInstance()->proxy();
  }

 protected:
  void SetFocus(ui::TextInputClient* client) {
    ui::InputMethod* input_method = proxy()->GetInputMethod();
    input_method->SetFocusedTextInputClient(client);
    if (client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE)
      input_method->ShowImeIfNeeded();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardWebContentTest);
};

// When focused on a password field, system virtual keyboard should be used
// instead of IME provided virtual keyboard. At non password field, IME provided
// virtual keyboard should show up if any.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardWebContentTest,
                       ShowSystemKeyboardAtPasswordField) {
  ui::DummyTextInputClient input_client_0(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient input_client_1(ui::TEXT_INPUT_TYPE_PASSWORD);
  ui::DummyTextInputClient input_client_2(ui::TEXT_INPUT_TYPE_TEXT);

  scoped_ptr<keyboard::KeyboardControllerProxy::TestApi> test_api;
  test_api.reset(new keyboard::KeyboardControllerProxy::TestApi(proxy()));

  // Mock IME sets input_view.
  keyboard::SetOverrideContentUrl(GURL(content::kAboutBlankURL));

  SetFocus(&input_client_0);
  EXPECT_EQ(test_api->keyboard_contents()->GetURL(),
            GURL(content::kAboutBlankURL));

  SetFocus(&input_client_1);
  EXPECT_EQ(test_api->keyboard_contents()->GetURL(),
            GURL(keyboard::kKeyboardURL));

  SetFocus(&input_client_2);
  EXPECT_EQ(test_api->keyboard_contents()->GetURL(),
            GURL(content::kAboutBlankURL));
}
