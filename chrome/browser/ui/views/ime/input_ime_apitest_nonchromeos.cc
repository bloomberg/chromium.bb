// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api_nonchromeos.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/controls/textfield/textfield.h"

namespace extensions {

class InputImeApiTest : public ExtensionApiTest {
 public:
  InputImeApiTest() : weak_ptr_factory_(this) {}

  void ProcessKeyEventDone(bool consumed) {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableInputImeAPI);
  }

  // Used for making callbacks.
  base::WeakPtrFactory<InputImeApiTest> weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputImeApiTest);
};

IN_PROC_BROWSER_TEST_F(InputImeApiTest, BasicApiTest) {
  // Manipulates the focused text input client because the follow cursor
  // window requires the text input focus.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ui::InputMethod* input_method =
      browser_view->GetNativeWindow()->GetHost()->GetInputMethod();
  std::unique_ptr<ui::DummyTextInputClient> client(
      new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
  input_method->SetFocusedTextInputClient(client.get());
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  InputImeActivateFunction::disable_bubble_for_testing_ = true;

  // Listens for the input.ime.onBlur event.
  ExtensionTestMessageListener blur_listener("get_blur_event", false);

  ASSERT_TRUE(RunExtensionTest("input_ime_nonchromeos")) << message_;

  // Test the input.ime.sendKeyEvents API.
  ASSERT_EQ(1, client->insert_char_count());
  EXPECT_EQ(L'a', client->last_insert_char());

  // Test the input.ime.commitText API.
  ASSERT_EQ(1, client->insert_text_count());
  EXPECT_EQ(base::UTF8ToUTF16("test_commit_text"), client->last_insert_text());

  // Test the input.ime.setComposition API.
  ui::CompositionText composition;
  composition.text = base::UTF8ToUTF16("test_set_composition");
  composition.underlines.push_back(
      ui::CompositionUnderline(0, composition.text.length(), SK_ColorBLACK,
                               false /* thick */, SK_ColorTRANSPARENT));
  composition.selection = gfx::Range(2, 2);
  ASSERT_EQ(1, client->set_composition_count());
  ASSERT_EQ(composition, client->last_composition());

  // Tests input.ime.onBlur API should get event when focusing to another
  // text input client.
  std::unique_ptr<ui::DummyTextInputClient> client2(
      new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
  input_method->SetFocusedTextInputClient(client2.get());
  ASSERT_TRUE(blur_listener.WaitUntilSatisfied()) << message_;

  input_method->DetachTextInputClient(client2.get());
}

IN_PROC_BROWSER_TEST_F(InputImeApiTest, OnKeyEvent) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ui::InputMethod* input_method =
      browser_view->GetNativeWindow()->GetHost()->GetInputMethod();
  std::unique_ptr<views::Textfield> textfield(new views::Textfield());
  input_method->SetFocusedTextInputClient(textfield.get());
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  InputImeActivateFunction::disable_bubble_for_testing_ = true;

  ASSERT_TRUE(RunExtensionTest("input_ime_nonchromeos")) << message_;

  ui::IMEEngineHandlerInterface* engine_handler =
      ui::IMEBridge::Get()->GetCurrentEngineHandler();

  // Tests for input.ime.onKeyEvent event.
  {
    SCOPED_TRACE("KeyDown, KeyA, Ctrl:No, alt:No, Shift:No, Caps:No");
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback keyevent_callback =
        base::Bind(&InputImeApiTest::ProcessKeyEventDone,
                   weak_ptr_factory_.GetWeakPtr());
    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A,
                           ui::EF_NONE);
    engine_handler->ProcessKeyEvent(key_event, keyevent_callback);
    const std::string expected_value =
        "onKeyEvent::keydown:a:KeyA:false:false:false:false";
    ExtensionTestMessageListener keyevent_listener(expected_value, false);
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
  }
  {
    SCOPED_TRACE("KeyDown, KeyB, Ctrl:No, alt:No, Shift:Yes, Caps:Yes");
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback keyevent_callback =
        base::Bind(&InputImeApiTest::ProcessKeyEventDone,
                   weak_ptr_factory_.GetWeakPtr());
    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_B, ui::DomCode::US_B,
                           ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_ON);
    engine_handler->ProcessKeyEvent(key_event, keyevent_callback);
    const std::string expected_value =
        "onKeyEvent::keydown:b:KeyB:false:false:true:true";
    ExtensionTestMessageListener keyevent_listener(expected_value, false);
    ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
  }
}

}  // namespace extensions
