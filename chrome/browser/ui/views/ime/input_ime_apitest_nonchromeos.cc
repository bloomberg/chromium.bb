// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"

namespace extensions {

class InputImeApiTest : public ExtensionApiTest {
 public:
  InputImeApiTest() {}

  // extensions::ExtensionApiTest:
  void SetUpOnMainThread() override;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableInputImeAPI);
  }

  ui::InputMethod* input_method;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputImeApiTest);
};

void InputImeApiTest::SetUpOnMainThread() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  input_method = browser_view->GetNativeWindow()->GetHost()->GetInputMethod();
}

IN_PROC_BROWSER_TEST_F(InputImeApiTest, BasicApiTest) {
  // Manipulates the focused text input client because the follow cursor
  // window requires the text input focus.
  ExtensionTestMessageListener focus_listener("get_focus_event", false);
  ExtensionTestMessageListener blur_listener("get_blur_event", false);

  scoped_ptr<ui::DummyTextInputClient> client(
      new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
  input_method->SetFocusedTextInputClient(client.get());

  ASSERT_TRUE(RunExtensionTest("input_ime_nonchromeos")) << message_;

  // Test the input.ime.sendKeyEvents API.
  ASSERT_EQ(client->insert_char_count(), 1);
  ASSERT_EQ(client->last_insert_char(), L'a');

  // Test the input.ime.commitText API.
  ASSERT_EQ(client->insert_text_count(), 1);
  ASSERT_EQ(client->last_insert_text(), base::UTF8ToUTF16("test_commit_text"));

  // Test the input.ime.onFocus and onBlur APIs.
  ASSERT_TRUE(focus_listener.WaitUntilSatisfied()) << message_;
  // Focus to the second text input client.
  scoped_ptr<ui::DummyTextInputClient> client2(
      new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
  input_method->SetFocusedTextInputClient(client2.get());
  ASSERT_TRUE(blur_listener.WaitUntilSatisfied()) << message_;

  input_method->DetachTextInputClient(client.get());
}

IN_PROC_BROWSER_TEST_F(InputImeApiTest, CompostionTextTest) {
  ExtensionTestMessageListener compsition_listener("finish_composition_test",
                                                   false);

  // Test the input.ime.setComposition and onCompositionBoundsChanged API.
  ASSERT_TRUE(RunExtensionTest("input_ime_nonchromeos")) << message_;

  ASSERT_TRUE(compsition_listener.WaitUntilSatisfied()) << message_;
};

}  // namespace extensions
