// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/event_rewriter.h"

#include <vector>

#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/sticky_keys/sticky_keys_overlay.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_member.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/chromeos/fake_ime_keyboard.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/test_event_processor.h"

#if defined(USE_X11)
#include <X11/keysym.h>

#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/gfx/x/x11_types.h"
#endif

namespace {

// The device id of the test touchpad device.
#if defined(USE_X11)
const int kTouchPadDeviceId = 1;
#endif
const int kKeyboardDeviceId = 2;

std::string GetExpectedResultAsString(ui::EventType ui_type,
                                      ui::KeyboardCode ui_keycode,
                                      ui::DomCode code,
                                      int ui_flags,  // ui::EventFlags
                                      ui::DomKey key) {
  return base::StringPrintf(
      "type=%d code=0x%06X flags=0x%X vk=0x%02X key=0x%08X", ui_type,
      static_cast<unsigned int>(code), ui_flags & ~ui::EF_IS_REPEAT, ui_keycode,
      static_cast<unsigned int>(key));
}

std::string GetKeyEventAsString(const ui::KeyEvent& keyevent) {
  return GetExpectedResultAsString(keyevent.type(), keyevent.key_code(),
                                   keyevent.code(), keyevent.flags(),
                                   keyevent.GetDomKey());
}

std::string GetRewrittenEventAsString(chromeos::EventRewriter* rewriter,
                                      ui::EventType ui_type,
                                      ui::KeyboardCode ui_keycode,
                                      ui::DomCode code,
                                      int ui_flags,  // ui::EventFlags
                                      ui::DomKey key) {
  const ui::KeyEvent event(ui_type, ui_keycode, code, ui_flags, key,
                           ui::EventTimeForNow());
  std::unique_ptr<ui::Event> new_event;
  rewriter->RewriteEvent(event, &new_event);
  if (new_event)
    return GetKeyEventAsString(*new_event->AsKeyEvent());
  return GetKeyEventAsString(event);
}

// Table entry for simple single key event rewriting tests.
struct KeyTestCase {
  ui::EventType type;
  struct Event {
    ui::KeyboardCode key_code;
    ui::DomCode code;
    int flags;  // ui::EventFlags
    ui::DomKey::Base key;
  } input, expected;
};

std::string GetTestCaseAsString(ui::EventType ui_type,
                                const KeyTestCase::Event& test) {
  return GetExpectedResultAsString(ui_type, test.key_code, test.code,
                                   test.flags, test.key);
}

// Tests a single stateless key rewrite operation.
void CheckKeyTestCase(chromeos::EventRewriter* rewriter,
                      const KeyTestCase& test) {
  SCOPED_TRACE("\nSource:    " + GetTestCaseAsString(test.type, test.input));
  std::string expected = GetTestCaseAsString(test.type, test.expected);
  EXPECT_EQ(expected, GetRewrittenEventAsString(
                          rewriter, test.type, test.input.key_code,
                          test.input.code, test.input.flags, test.input.key));
}

}  // namespace

namespace chromeos {

class EventRewriterTest : public ash::test::AshTestBase {
 public:
  EventRewriterTest()
      : fake_user_manager_(new user_manager::FakeUserManager),
        user_manager_enabler_(fake_user_manager_),
        input_method_manager_mock_(NULL) {}
  ~EventRewriterTest() override {}

  void SetUp() override {
    input_method_manager_mock_ =
        new chromeos::input_method::MockInputMethodManager;
    chromeos::input_method::InitializeForTesting(
        input_method_manager_mock_);  // pass ownership

    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    // Shutdown() deletes the IME mock object.
    chromeos::input_method::Shutdown();
  }

 protected:
  void TestRewriteNumPadKeys();
  void TestRewriteNumPadKeysOnAppleKeyboard();

  const ui::MouseEvent* RewriteMouseButtonEvent(
      chromeos::EventRewriter* rewriter,
      const ui::MouseEvent& event,
      std::unique_ptr<ui::Event>* new_event) {
    rewriter->RewriteMouseButtonEventForTesting(event, new_event);
    return *new_event ? new_event->get()->AsMouseEvent() : &event;
  }

  user_manager::FakeUserManager* fake_user_manager_;  // Not owned.
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  chromeos::input_method::MockInputMethodManager* input_method_manager_mock_;
};

TEST_F(EventRewriterTest, TestRewriteCommandToControl) {
  // First, test with a PC keyboard.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase pc_keyboard_tests[] = {
      // VKEY_A, Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::UNIDENTIFIED}},

      // VKEY_A, Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED}},

      // VKEY_A, Alt+Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED}},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::DomKey::META}},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::DomKey::META},
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::DomKey::META}},
  };

  for (const auto& test : pc_keyboard_tests) {
    CheckKeyTestCase(&rewriter, test);
  }

  // An Apple keyboard reusing the ID, zero.
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "Apple Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);

  KeyTestCase apple_keyboard_tests[] = {
      // VKEY_A, Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::UNIDENTIFIED}},

      // VKEY_A, Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character}},

      // VKEY_A, Alt+Win modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character}},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ui::DomKey::CONTROL}},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_RIGHT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ui::DomKey::CONTROL}},
  };

  for (const auto& test : apple_keyboard_tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

// For crbug.com/133896.
TEST_F(EventRewriterTest, TestRewriteCommandToControlWithControlRemapped) {
  // Remap Control to Alt.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapControlKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kAltKey);

  EventRewriter rewriter(NULL);
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);

  KeyTestCase pc_keyboard_tests[] = {
      // Control should be remapped to Alt.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN,
        ui::DomKey::ALT}},
  };

  for (const auto& test : pc_keyboard_tests) {
    CheckKeyTestCase(&rewriter, test);
  }

  // An Apple keyboard reusing the ID, zero.
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "Apple Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);

  KeyTestCase apple_keyboard_tests[] = {
      // VKEY_LWIN (left Command key) with  Alt modifier. The remapped Command
      // key should never be re-remapped to Alt.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ui::DomKey::CONTROL}},

      // VKEY_RWIN (right Command key) with  Alt modifier. The remapped Command
      // key should never be re-remapped to Alt.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::DomCode::META_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_RIGHT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ui::DomKey::CONTROL}},
  };

  for (const auto& test : apple_keyboard_tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

void EventRewriterTest::TestRewriteNumPadKeys() {
  syncable_prefs::TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // XK_KP_Insert (= NumPad 0 without Num Lock), no modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_INSERT, ui::DomCode::NUMPAD0, ui::EF_NONE, ui::DomKey::INSERT},
       {ui::VKEY_NUMPAD0, ui::DomCode::NUMPAD0, ui::EF_NONE,
        ui::DomKey::Constant<'0'>::Character}},

      // XK_KP_Insert (= NumPad 0 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_INSERT, ui::DomCode::NUMPAD0, ui::EF_ALT_DOWN,
        ui::DomKey::INSERT},
       {ui::VKEY_NUMPAD0, ui::DomCode::NUMPAD0, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'0'>::Character}},

      // XK_KP_Delete (= NumPad . without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DELETE, ui::DomCode::NUMPAD_DECIMAL, ui::EF_ALT_DOWN,
        ui::DomKey::DEL},
       {ui::VKEY_DECIMAL, ui::DomCode::NUMPAD_DECIMAL, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'.'>::Character}},

      // XK_KP_End (= NumPad 1 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_END, ui::DomCode::NUMPAD1, ui::EF_ALT_DOWN, ui::DomKey::END},
       {ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'1'>::Character}},

      // XK_KP_Down (= NumPad 2 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::NUMPAD2, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_DOWN},
       {ui::VKEY_NUMPAD2, ui::DomCode::NUMPAD2, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'2'>::Character}},

      // XK_KP_Next (= NumPad 3 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NEXT, ui::DomCode::NUMPAD3, ui::EF_ALT_DOWN,
        ui::DomKey::PAGE_DOWN},
       {ui::VKEY_NUMPAD3, ui::DomCode::NUMPAD3, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'3'>::Character}},

      // XK_KP_Left (= NumPad 4 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LEFT, ui::DomCode::NUMPAD4, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_LEFT},
       {ui::VKEY_NUMPAD4, ui::DomCode::NUMPAD4, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'4'>::Character}},

      // XK_KP_Begin (= NumPad 5 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CLEAR, ui::DomCode::NUMPAD5, ui::EF_ALT_DOWN,
        ui::DomKey::CLEAR},
       {ui::VKEY_NUMPAD5, ui::DomCode::NUMPAD5, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'5'>::Character}},

      // XK_KP_Right (= NumPad 6 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RIGHT, ui::DomCode::NUMPAD6, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_RIGHT},
       {ui::VKEY_NUMPAD6, ui::DomCode::NUMPAD6, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'6'>::Character}},

      // XK_KP_Home (= NumPad 7 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_HOME, ui::DomCode::NUMPAD7, ui::EF_ALT_DOWN, ui::DomKey::HOME},
       {ui::VKEY_NUMPAD7, ui::DomCode::NUMPAD7, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'7'>::Character}},

      // XK_KP_Up (= NumPad 8 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::DomCode::NUMPAD8, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_UP},
       {ui::VKEY_NUMPAD8, ui::DomCode::NUMPAD8, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'8'>::Character}},

      // XK_KP_Prior (= NumPad 9 without Num Lock), Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_PRIOR, ui::DomCode::NUMPAD9, ui::EF_ALT_DOWN,
        ui::DomKey::PAGE_UP},
       {ui::VKEY_NUMPAD9, ui::DomCode::NUMPAD9, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'9'>::Character}},

      // XK_KP_0 (= NumPad 0 with Num Lock), Num Lock modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD0, ui::DomCode::NUMPAD0, ui::EF_NONE,
        ui::DomKey::Constant<'0'>::Character},
       {ui::VKEY_NUMPAD0, ui::DomCode::NUMPAD0, ui::EF_NONE,
        ui::DomKey::Constant<'0'>::Character}},

      // XK_KP_DECIMAL (= NumPad . with Num Lock), Num Lock modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DECIMAL, ui::DomCode::NUMPAD_DECIMAL, ui::EF_NONE,
        ui::DomKey::Constant<'.'>::Character},
       {ui::VKEY_DECIMAL, ui::DomCode::NUMPAD_DECIMAL, ui::EF_NONE,
        ui::DomKey::Constant<'.'>::Character}},

      // XK_KP_1 (= NumPad 1 with Num Lock), Num Lock modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1, ui::EF_NONE,
        ui::DomKey::Constant<'1'>::Character},
       {ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1, ui::EF_NONE,
        ui::DomKey::Constant<'1'>::Character}},

      // XK_KP_2 (= NumPad 2 with Num Lock), Num Lock modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD2, ui::DomCode::NUMPAD2, ui::EF_NONE,
        ui::DomKey::Constant<'2'>::Character},
       {ui::VKEY_NUMPAD2, ui::DomCode::NUMPAD2, ui::EF_NONE,
        ui::DomKey::Constant<'2'>::Character}},

      // XK_KP_3 (= NumPad 3 with Num Lock), Num Lock modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD3, ui::DomCode::NUMPAD3, ui::EF_NONE,
        ui::DomKey::Constant<'3'>::Character},
       {ui::VKEY_NUMPAD3, ui::DomCode::NUMPAD3, ui::EF_NONE,
        ui::DomKey::Constant<'3'>::Character}},

      // XK_KP_4 (= NumPad 4 with Num Lock), Num Lock modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD4, ui::DomCode::NUMPAD4, ui::EF_NONE,
        ui::DomKey::Constant<'4'>::Character},
       {ui::VKEY_NUMPAD4, ui::DomCode::NUMPAD4, ui::EF_NONE,
        ui::DomKey::Constant<'4'>::Character}},

      // XK_KP_5 (= NumPad 5 with Num Lock), Num Lock
      // modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD5, ui::DomCode::NUMPAD5, ui::EF_NONE,
        ui::DomKey::Constant<'5'>::Character},
       {ui::VKEY_NUMPAD5, ui::DomCode::NUMPAD5, ui::EF_NONE,
        ui::DomKey::Constant<'5'>::Character}},

      // XK_KP_6 (= NumPad 6 with Num Lock), Num Lock
      // modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD6, ui::DomCode::NUMPAD6, ui::EF_NONE,
        ui::DomKey::Constant<'6'>::Character},
       {ui::VKEY_NUMPAD6, ui::DomCode::NUMPAD6, ui::EF_NONE,
        ui::DomKey::Constant<'6'>::Character}},

      // XK_KP_7 (= NumPad 7 with Num Lock), Num Lock
      // modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD7, ui::DomCode::NUMPAD7, ui::EF_NONE,
        ui::DomKey::Constant<'7'>::Character},
       {ui::VKEY_NUMPAD7, ui::DomCode::NUMPAD7, ui::EF_NONE,
        ui::DomKey::Constant<'7'>::Character}},

      // XK_KP_8 (= NumPad 8 with Num Lock), Num Lock
      // modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD8, ui::DomCode::NUMPAD8, ui::EF_NONE,
        ui::DomKey::Constant<'8'>::Character},
       {ui::VKEY_NUMPAD8, ui::DomCode::NUMPAD8, ui::EF_NONE,
        ui::DomKey::Constant<'8'>::Character}},

      // XK_KP_9 (= NumPad 9 with Num Lock), Num Lock
      // modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD9, ui::DomCode::NUMPAD9, ui::EF_NONE,
        ui::DomKey::Constant<'9'>::Character},
       {ui::VKEY_NUMPAD9, ui::DomCode::NUMPAD9, ui::EF_NONE,
        ui::DomKey::Constant<'9'>::Character}},
  };

  for (const auto& test : tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteNumPadKeys) {
  TestRewriteNumPadKeys();
}

TEST_F(EventRewriterTest, TestRewriteNumPadKeysWithDiamondKeyFlag) {
  // Make sure the num lock works correctly even when Diamond key exists.
  const base::CommandLine original_cl(*base::CommandLine::ForCurrentProcess());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kHasChromeOSDiamondKey, "");

  TestRewriteNumPadKeys();
  *base::CommandLine::ForCurrentProcess() = original_cl;
}

// Tests if the rewriter can handle a Command + Num Pad event.
void EventRewriterTest::TestRewriteNumPadKeysOnAppleKeyboard() {
  syncable_prefs::TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "Apple Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // XK_KP_End (= NumPad 1 without Num Lock), Win modifier.
      // The result should be "Num Pad 1 with Control + Num Lock modifiers".
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_END, ui::DomCode::NUMPAD1, ui::EF_COMMAND_DOWN,
        ui::DomKey::END},
       {ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'1'>::Character}},

      // XK_KP_1 (= NumPad 1 with Num Lock), Win modifier.
      // The result should also be "Num Pad 1 with Control + Num Lock
      // modifiers".
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'1'>::Character},
       {ui::VKEY_NUMPAD1, ui::DomCode::NUMPAD1, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'1'>::Character}}};

  for (const auto& test : tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteNumPadKeysOnAppleKeyboard) {
  TestRewriteNumPadKeysOnAppleKeyboard();
}

TEST_F(EventRewriterTest,
       TestRewriteNumPadKeysOnAppleKeyboardWithDiamondKeyFlag) {
  // Makes sure the num lock works correctly even when Diamond key exists.
  const base::CommandLine original_cl(*base::CommandLine::ForCurrentProcess());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kHasChromeOSDiamondKey, "");

  TestRewriteNumPadKeysOnAppleKeyboard();
  *base::CommandLine::ForCurrentProcess() = original_cl;
}

TEST_F(EventRewriterTest, TestRewriteModifiersNoRemap) {
  syncable_prefs::TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // Press Search. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_NONE, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::META}},

      // Press left Control. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      // Press right Control. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      // Press left Alt. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN,
        ui::DomKey::ALT}},

      // Press right Alt. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN,
        ui::DomKey::ALT}},

      // Test KeyRelease event, just in case.
      // Release Search. Confirm the release event is not rewritten.
      {ui::ET_KEY_RELEASED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_NONE, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_NONE, ui::DomKey::META}},
  };

  for (const auto& test : tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersNoRemapMultipleKeys) {
  syncable_prefs::TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // Press Alt with Shift. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, ui::DomKey::ALT}},

      // Press Search with Caps Lock mask. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_CAPS_LOCK_ON | ui::EF_COMMAND_DOWN, ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_CAPS_LOCK_ON | ui::EF_COMMAND_DOWN, ui::DomKey::META}},

      // Release Search with Caps Lock mask. Confirm the event is not rewritten.
      {ui::ET_KEY_RELEASED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_CAPS_LOCK_ON,
        ui::DomKey::META},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_CAPS_LOCK_ON,
        ui::DomKey::META}},

      // Press Shift+Ctrl+Alt+Search+A. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_B, ui::DomCode::US_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'B'>::Character},
       {ui::VKEY_B, ui::DomCode::US_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'B'>::Character}},
  };

  for (const auto& test : tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersDisableSome) {
  // Disable Search and Control keys.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kVoidKey);
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapControlKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kVoidKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase disabled_modifier_tests[] = {
      // Press Alt with Shift. This key press shouldn't be affected by the
      // pref. Confirm the event is not rewritten.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, ui::DomKey::ALT}},

      // Press Search. Confirm the event is now VKEY_UNKNOWN.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_NONE, ui::DomKey::META},
       {ui::VKEY_UNKNOWN, ui::DomCode::NONE, ui::EF_NONE,
        ui::DomKey::UNIDENTIFIED}},

      // Press Control. Confirm the event is now VKEY_UNKNOWN.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL},
       {ui::VKEY_UNKNOWN, ui::DomCode::NONE, ui::EF_NONE,
        ui::DomKey::UNIDENTIFIED}},

      // Press Control+Search. Confirm the event is now VKEY_UNKNOWN
      // without any modifiers.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::META},
       {ui::VKEY_UNKNOWN, ui::DomCode::NONE, ui::EF_NONE,
        ui::DomKey::UNIDENTIFIED}},

      // Press Control+Search+a. Confirm the event is now VKEY_A without any
      // modifiers.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_NONE,
        ui::DomKey::Constant<'a'>::Character}},

      // Press Control+Search+Alt+a. Confirm the event is now VKEY_A only with
      // the Alt modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'a'>::Character}},
  };

  for (const auto& test : disabled_modifier_tests) {
    CheckKeyTestCase(&rewriter, test);
  }

  // Remap Alt to Control.
  IntegerPrefMember alt;
  alt.Init(prefs::kLanguageRemapAltKeyTo, &prefs);
  alt.SetValue(chromeos::input_method::kControlKey);

  KeyTestCase tests[] = {
      // Press left Alt. Confirm the event is now VKEY_CONTROL
      // even though the Control key itself is disabled.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      // Press Alt+a. Confirm the event is now Control+a even though the Control
      // key itself is disabled.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character}},
  };

  for (const auto& test : tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToControl) {
  // Remap Search to Control.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kControlKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase s_tests[] = {
      // Press Search. Confirm the event is now VKEY_CONTROL.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},
  };

  for (const auto& test : s_tests) {
    CheckKeyTestCase(&rewriter, test);
  }

  // Remap Alt to Control too.
  IntegerPrefMember alt;
  alt.Init(prefs::kLanguageRemapAltKeyTo, &prefs);
  alt.SetValue(chromeos::input_method::kControlKey);

  KeyTestCase sa_tests[] = {
      // Press Alt. Confirm the event is now VKEY_CONTROL.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      // Press Alt+Search. Confirm the event is now VKEY_CONTROL.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      // Press Control+Alt+Search. Confirm the event is now VKEY_CONTROL.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      // Press Shift+Control+Alt+Search. Confirm the event is now Control with
      // Shift and Control modifiers.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::META},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::CONTROL}},

      // Press Shift+Control+Alt+Search+B. Confirm the event is now B with Shift
      // and Control modifiers.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_B, ui::DomCode::US_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'B'>::Character},
       {ui::VKEY_B, ui::DomCode::US_B, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'B'>::Character}},
  };

  for (const auto& test : sa_tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToEscape) {
  // Remap Search to ESC.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kEscapeKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // Press Search. Confirm the event is now VKEY_ESCAPE.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::META},
       {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE, ui::EF_NONE, ui::DomKey::ESCAPE}},
  };

  for (const auto& test : tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapMany) {
  // Remap Search to Alt.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kAltKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase s2a_tests[] = {
      // Press Search. Confirm the event is now VKEY_MENU.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::META},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN,
        ui::DomKey::ALT}},
  };

  for (const auto& test : s2a_tests) {
    CheckKeyTestCase(&rewriter, test);
  }

  // Remap Alt to Control.
  IntegerPrefMember alt;
  alt.Init(prefs::kLanguageRemapAltKeyTo, &prefs);
  alt.SetValue(chromeos::input_method::kControlKey);

  KeyTestCase a2c_tests[] = {
      // Press left Alt. Confirm the event is now VKEY_CONTROL.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN, ui::DomKey::ALT},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},
      // Press Shift+comma. Verify that only the flags are changed.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_COMMA, ui::DomCode::COMMA,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_OEM_COMMA, ui::DomCode::COMMA,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'<'>::Character}},
      // Press Shift+9. Verify that only the flags are changed.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::DomCode::DIGIT9, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::UNIDENTIFIED},
       {ui::VKEY_9, ui::DomCode::DIGIT9,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'('>::Character}},
  };

  for (const auto& test : a2c_tests) {
    CheckKeyTestCase(&rewriter, test);
  }

  // Remap Control to Search.
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapControlKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kSearchKey);

  KeyTestCase c2s_tests[] = {
      // Press left Control. Confirm the event is now VKEY_LWIN.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL},
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::META}},

      // Then, press all of the three, Control+Alt+Search.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::META},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::ALT}},

      // Press Shift+Control+Alt+Search.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::META_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::META},
       {ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::ALT}},

      // Press Shift+Control+Alt+Search+B
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_B, ui::DomCode::US_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'B'>::Character},
       {ui::VKEY_B, ui::DomCode::US_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'B'>::Character}},
  };

  for (const auto& test : c2s_tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToCapsLock) {
  // Remap Search to Caps Lock.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kCapsLockKey);

  chromeos::input_method::FakeImeKeyboard ime_keyboard;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.set_ime_keyboard_for_testing(&ime_keyboard);
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Press Search.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                ui::EF_MOD3_DOWN | ui::EF_CAPS_LOCK_ON, ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_LWIN, ui::DomCode::META_LEFT,
                                      ui::EF_COMMAND_DOWN, ui::DomKey::META));
  // Confirm that the Caps Lock status is changed.
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);

  // Release Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK, ui::EF_NONE,
                                      ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_LWIN, ui::DomCode::META_LEFT,
                                      ui::EF_NONE, ui::DomKey::META));
  // Confirm that the Caps Lock status is not changed.
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);

  // Press Search.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                ui::EF_CAPS_LOCK_ON | ui::EF_MOD3_DOWN, ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_LWIN, ui::DomCode::META_LEFT,
                                      ui::EF_COMMAND_DOWN | ui::EF_CAPS_LOCK_ON,
                                      ui::DomKey::META));
  // Confirm that the Caps Lock status is changed.
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Release Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK, ui::EF_NONE,
                                      ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_LWIN, ui::DomCode::META_LEFT,
                                      ui::EF_NONE, ui::DomKey::META));
  // Confirm that the Caps Lock status is not changed.
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Press Caps Lock (on an external keyboard).
  EXPECT_EQ(GetExpectedResultAsString(
                ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                ui::EF_CAPS_LOCK_ON | ui::EF_MOD3_DOWN, ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                                      ui::EF_CAPS_LOCK_ON | ui::EF_MOD3_DOWN,
                                      ui::DomKey::CAPS_LOCK));

#if defined(USE_X11)
  // Confirm that calling RewriteForTesting() does not change the state of
  // |ime_keyboard|. In this case, X Window system itself should change the
  // Caps Lock state, not ash::EventRewriter.
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);
#elif defined(USE_OZONE)
  // Under Ozone the rewriter is responsible for changing the caps lock
  // state when the final key is Caps Lock, regardless of whether the
  // initial key is Caps Lock.
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);
#endif

  // Release Caps Lock (on an external keyboard).
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK, ui::EF_NONE,
                                      ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                                      ui::EF_NONE, ui::DomKey::CAPS_LOCK));
#if defined(USE_X11)
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);
#elif defined(USE_OZONE)
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);
#endif
}

TEST_F(EventRewriterTest, TestRewriteCapsLock) {
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());

  chromeos::input_method::FakeImeKeyboard ime_keyboard;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.set_ime_keyboard_for_testing(&ime_keyboard);
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // On Chrome OS, CapsLock is mapped to F16 with Mod3Mask.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                ui::EF_CAPS_LOCK_ON | ui::EF_MOD3_DOWN, ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_F16, ui::DomCode::F16,
                                      ui::EF_MOD3_DOWN, ui::DomKey::F16));
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);
}

TEST_F(EventRewriterTest, TestRewriteDiamondKey) {
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());

  chromeos::input_method::FakeImeKeyboard ime_keyboard;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.set_ime_keyboard_for_testing(&ime_keyboard);

  KeyTestCase tests[] = {
      // F15 should work as Ctrl when --has-chromeos-diamond-key is not
      // specified.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F15, ui::DomCode::F15, ui::EF_NONE, ui::DomKey::F15},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL}},

      {ui::ET_KEY_RELEASED,
       {ui::VKEY_F15, ui::DomCode::F15, ui::EF_NONE, ui::DomKey::F15},
       {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_NONE,
        ui::DomKey::CONTROL}},

      // However, Mod2Mask should not be rewritten to CtrlMask when
      // --has-chromeos-diamond-key is not specified.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_NONE,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_NONE,
        ui::DomKey::Constant<'a'>::Character}},
  };

  for (const auto& test : tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteDiamondKeyWithFlag) {
  const base::CommandLine original_cl(*base::CommandLine::ForCurrentProcess());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kHasChromeOSDiamondKey, "");

  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());

  chromeos::input_method::FakeImeKeyboard ime_keyboard;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.set_ime_keyboard_for_testing(&ime_keyboard);

  // By default, F15 should work as Control.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL,
                                      ui::DomCode::CONTROL_LEFT,
                                      ui::EF_CONTROL_DOWN, ui::DomKey::CONTROL),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15));
  // Check that Control is applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
                                      ui::DomKey::Constant<'a'>::Character),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character));
  // Release F15
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL,
                                      ui::DomCode::CONTROL_LEFT, ui::EF_NONE,
                                      ui::DomKey::CONTROL),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15));
  // Check that Control is no longer applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character));

  IntegerPrefMember diamond;
  diamond.Init(prefs::kLanguageRemapDiamondKeyTo, &prefs);
  diamond.SetValue(chromeos::input_method::kVoidKey);

  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN,
                                      ui::DomCode::NONE, ui::EF_NONE,
                                      ui::DomKey::UNIDENTIFIED),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15));
  // Check that no modifier is applied to another key.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character));

  diamond.SetValue(chromeos::input_method::kControlKey);

  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL,
                                      ui::DomCode::CONTROL_LEFT,
                                      ui::EF_CONTROL_DOWN, ui::DomKey::CONTROL),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15));
  // Check that Control is applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
                                      ui::DomKey::Constant<'a'>::Character),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character));
  // Release F15
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL,
                                      ui::DomCode::CONTROL_LEFT, ui::EF_NONE,
                                      ui::DomKey::CONTROL),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15));
  // Check that Control is no longer applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character));

  diamond.SetValue(chromeos::input_method::kAltKey);

  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_MENU,
                                      ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN,
                                      ui::DomKey::ALT),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15));
  // Check that Alt is applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_ALT_DOWN,
                                      ui::DomKey::Constant<'a'>::Character),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character));
  // Release F15
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_MENU,
                                      ui::DomCode::ALT_LEFT, ui::EF_NONE,
                                      ui::DomKey::ALT),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15));
  // Check that Alt is no longer applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character));

  diamond.SetValue(chromeos::input_method::kCapsLockKey);

  EXPECT_EQ(GetExpectedResultAsString(
                ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                ui::EF_CAPS_LOCK_ON | ui::EF_MOD3_DOWN, ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15));
  // Check that Caps is applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A,
                                      ui::EF_CAPS_LOCK_ON | ui::EF_MOD3_DOWN,
                                      ui::DomKey::Constant<'A'>::Character),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character));
  // Release F15
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK, ui::EF_NONE,
                                      ui::DomKey::CAPS_LOCK),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15));
  // Check that Control is no longer applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character));

  *base::CommandLine::ForCurrentProcess() = original_cl;
}

TEST_F(EventRewriterTest, TestRewriteCapsLockToControl) {
  // Remap CapsLock to Control.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapCapsLockKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kControlKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // Press CapsLock+a. Confirm that Mod3Mask is rewritten to ControlMask.
      // On Chrome OS, CapsLock works as a Mod3 modifier.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_MOD3_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character}},

      // Press Control+CapsLock+a. Confirm that Mod3Mask is rewritten to
      // ControlMask
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN | ui::EF_MOD3_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character}},

      // Press Alt+CapsLock+a. Confirm that Mod3Mask is rewritten to
      // ControlMask.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_MOD3_DOWN,
        ui::DomKey::Constant<'a'>::Character},
       {ui::VKEY_A, ui::DomCode::US_A, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'a'>::Character}},
  };

  for (const auto& test : tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteCapsLockMod3InUse) {
  // Remap CapsLock to Control.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapCapsLockKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kControlKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);
  input_method_manager_mock_->set_mod3_used(true);

  // Press CapsLock+a. Confirm that Mod3Mask is NOT rewritten to ControlMask
  // when Mod3Mask is already in use by the current XKB layout.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::US_A, ui::EF_NONE,
                                      ui::DomKey::Constant<'a'>::Character));

  input_method_manager_mock_->set_mod3_used(false);
}

TEST_F(EventRewriterTest, TestRewriteExtendedKeys) {
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // Alt+Backspace -> Delete
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE, ui::EF_ALT_DOWN,
        ui::DomKey::BACKSPACE},
       {ui::VKEY_DELETE, ui::DomCode::DEL, ui::EF_NONE, ui::DomKey::DEL}},
      // Control+Alt+Backspace -> Control+Delete
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::BACKSPACE},
       {ui::VKEY_DELETE, ui::DomCode::DEL, ui::EF_CONTROL_DOWN,
        ui::DomKey::DEL}},
      // Search+Alt+Backspace -> Alt+Backspace
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::BACKSPACE},
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE, ui::EF_ALT_DOWN,
        ui::DomKey::BACKSPACE}},
      // Search+Control+Alt+Backspace -> Control+Alt+Backspace
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::BACKSPACE},
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::BACKSPACE}},
      // Alt+Up -> Prior
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::DomCode::ARROW_UP, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_UP},
       {ui::VKEY_PRIOR, ui::DomCode::PAGE_UP, ui::EF_NONE,
        ui::DomKey::PAGE_UP}},
      // Alt+Down -> Next
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_DOWN},
       {ui::VKEY_NEXT, ui::DomCode::PAGE_DOWN, ui::EF_NONE,
        ui::DomKey::PAGE_DOWN}},
      // Ctrl+Alt+Up -> Home
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::DomCode::ARROW_UP,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::ARROW_UP},
       {ui::VKEY_HOME, ui::DomCode::HOME, ui::EF_NONE, ui::DomKey::HOME}},
      // Ctrl+Alt+Down -> End
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::ARROW_DOWN},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_NONE, ui::DomKey::END}},

      // Search+Alt+Up -> Alt+Up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::DomCode::ARROW_UP,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::ARROW_UP},
       {ui::VKEY_UP, ui::DomCode::ARROW_UP, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_UP}},
      // Search+Alt+Down -> Alt+Down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::DomKey::ARROW_DOWN},
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN, ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_DOWN}},
      // Search+Ctrl+Alt+Up -> Search+Ctrl+Alt+Up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::DomCode::ARROW_UP,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::ARROW_UP},
       {ui::VKEY_UP, ui::DomCode::ARROW_UP,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::ARROW_UP}},
      // Search+Ctrl+Alt+Down -> Ctrl+Alt+Down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::ARROW_DOWN},
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::ARROW_DOWN}},

      // Period -> Period
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PERIOD, ui::DomCode::PERIOD, ui::EF_NONE,
        ui::DomKey::Constant<'.'>::Character},
       {ui::VKEY_OEM_PERIOD, ui::DomCode::PERIOD, ui::EF_NONE,
        ui::DomKey::Constant<'.'>::Character}},

      // Search+Backspace -> Delete
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::DomCode::BACKSPACE, ui::EF_COMMAND_DOWN,
        ui::DomKey::BACKSPACE},
       {ui::VKEY_DELETE, ui::DomCode::DEL, ui::EF_NONE, ui::DomKey::DEL}},
      // Search+Up -> Prior
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::DomCode::ARROW_UP, ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_UP},
       {ui::VKEY_PRIOR, ui::DomCode::PAGE_UP, ui::EF_NONE,
        ui::DomKey::PAGE_UP}},
      // Search+Down -> Next
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN, ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_DOWN},
       {ui::VKEY_NEXT, ui::DomCode::PAGE_DOWN, ui::EF_NONE,
        ui::DomKey::PAGE_DOWN}},
      // Search+Left -> Home
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LEFT, ui::DomCode::ARROW_LEFT, ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_LEFT},
       {ui::VKEY_HOME, ui::DomCode::HOME, ui::EF_NONE, ui::DomKey::HOME}},
      // Control+Search+Left -> Home
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_LEFT, ui::DomCode::ARROW_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::ARROW_LEFT},
       {ui::VKEY_HOME, ui::DomCode::HOME, ui::EF_CONTROL_DOWN,
        ui::DomKey::HOME}},
      // Search+Right -> End
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RIGHT, ui::DomCode::ARROW_RIGHT, ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_RIGHT},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_NONE, ui::DomKey::END}},
      // Control+Search+Right -> End
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_RIGHT, ui::DomCode::ARROW_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN, ui::DomKey::ARROW_RIGHT},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_CONTROL_DOWN, ui::DomKey::END}},
      // Search+Period -> Insert
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PERIOD, ui::DomCode::PERIOD, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'.'>::Character},
       {ui::VKEY_INSERT, ui::DomCode::INSERT, ui::EF_NONE, ui::DomKey::INSERT}},
      // Control+Search+Period -> Control+Insert
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PERIOD, ui::DomCode::PERIOD,
        ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::Constant<'.'>::Character},
       {ui::VKEY_INSERT, ui::DomCode::INSERT, ui::EF_CONTROL_DOWN,
        ui::DomKey::INSERT}}};

  for (const auto& test : tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeys) {
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // F1 -> Back
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1},
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_NONE,
        ui::DomKey::BROWSER_BACK}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_CONTROL_DOWN, ui::DomKey::F1},
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_CONTROL_DOWN,
        ui::DomKey::BROWSER_BACK}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_ALT_DOWN, ui::DomKey::F1},
       {ui::VKEY_BROWSER_BACK, ui::DomCode::BROWSER_BACK, ui::EF_ALT_DOWN,
        ui::DomKey::BROWSER_BACK}},
      // F2 -> Forward
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2},
       {ui::VKEY_BROWSER_FORWARD, ui::DomCode::BROWSER_FORWARD, ui::EF_NONE,
        ui::DomKey::BROWSER_FORWARD}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_CONTROL_DOWN, ui::DomKey::F2},
       {ui::VKEY_BROWSER_FORWARD, ui::DomCode::BROWSER_FORWARD,
        ui::EF_CONTROL_DOWN, ui::DomKey::BROWSER_FORWARD}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_ALT_DOWN, ui::DomKey::F2},
       {ui::VKEY_BROWSER_FORWARD, ui::DomCode::BROWSER_FORWARD, ui::EF_ALT_DOWN,
        ui::DomKey::BROWSER_FORWARD}},
      // F3 -> Refresh
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_NONE,
        ui::DomKey::BROWSER_REFRESH}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_CONTROL_DOWN, ui::DomKey::F3},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH,
        ui::EF_CONTROL_DOWN, ui::DomKey::BROWSER_REFRESH}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_ALT_DOWN, ui::DomKey::F3},
       {ui::VKEY_BROWSER_REFRESH, ui::DomCode::BROWSER_REFRESH, ui::EF_ALT_DOWN,
        ui::DomKey::BROWSER_REFRESH}},
      // F4 -> Launch App 2
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE, ui::EF_NONE,
        ui::DomKey::ZOOM_TOGGLE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_CONTROL_DOWN, ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE,
        ui::EF_CONTROL_DOWN, ui::DomKey::ZOOM_TOGGLE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_ALT_DOWN, ui::DomKey::F4},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::DomCode::ZOOM_TOGGLE, ui::EF_ALT_DOWN,
        ui::DomKey::ZOOM_TOGGLE}},
      // F5 -> Launch App 1
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::SELECT_TASK, ui::EF_NONE,
        ui::DomKey::LAUNCH_MY_COMPUTER}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_CONTROL_DOWN, ui::DomKey::F5},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::SELECT_TASK,
        ui::EF_CONTROL_DOWN, ui::DomKey::LAUNCH_MY_COMPUTER}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_ALT_DOWN, ui::DomKey::F5},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::DomCode::SELECT_TASK, ui::EF_ALT_DOWN,
        ui::DomKey::LAUNCH_MY_COMPUTER}},
      // F6 -> Brightness down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_CONTROL_DOWN, ui::DomKey::F6},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN,
        ui::EF_CONTROL_DOWN, ui::DomKey::BRIGHTNESS_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_ALT_DOWN, ui::DomKey::F6},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::DomCode::BRIGHTNESS_DOWN, ui::EF_ALT_DOWN,
        ui::DomKey::BRIGHTNESS_DOWN}},
      // F7 -> Brightness up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_CONTROL_DOWN, ui::DomKey::F7},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_CONTROL_DOWN,
        ui::DomKey::BRIGHTNESS_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_ALT_DOWN, ui::DomKey::F7},
       {ui::VKEY_BRIGHTNESS_UP, ui::DomCode::BRIGHTNESS_UP, ui::EF_ALT_DOWN,
        ui::DomKey::BRIGHTNESS_UP}},
      // F8 -> Volume Mute
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8},
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_MUTE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_CONTROL_DOWN, ui::DomKey::F8},
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_CONTROL_DOWN,
        ui::DomKey::AUDIO_VOLUME_MUTE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_ALT_DOWN, ui::DomKey::F8},
       {ui::VKEY_VOLUME_MUTE, ui::DomCode::VOLUME_MUTE, ui::EF_ALT_DOWN,
        ui::DomKey::AUDIO_VOLUME_MUTE}},
      // F9 -> Volume Down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9},
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_CONTROL_DOWN, ui::DomKey::F9},
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_CONTROL_DOWN,
        ui::DomKey::AUDIO_VOLUME_DOWN}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_ALT_DOWN, ui::DomKey::F9},
       {ui::VKEY_VOLUME_DOWN, ui::DomCode::VOLUME_DOWN, ui::EF_ALT_DOWN,
        ui::DomKey::AUDIO_VOLUME_DOWN}},
      // F10 -> Volume Up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10},
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_NONE,
        ui::DomKey::AUDIO_VOLUME_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_CONTROL_DOWN, ui::DomKey::F10},
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_CONTROL_DOWN,
        ui::DomKey::AUDIO_VOLUME_UP}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_ALT_DOWN, ui::DomKey::F10},
       {ui::VKEY_VOLUME_UP, ui::DomCode::VOLUME_UP, ui::EF_ALT_DOWN,
        ui::DomKey::AUDIO_VOLUME_UP}},
      // F11 -> F11
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_CONTROL_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_CONTROL_DOWN, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_ALT_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_ALT_DOWN, ui::DomKey::F11}},
      // F12 -> F12
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_CONTROL_DOWN, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_CONTROL_DOWN, ui::DomKey::F12}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_ALT_DOWN, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_ALT_DOWN, ui::DomKey::F12}},

      // The number row should not be rewritten without Search key.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_1, ui::DomCode::DIGIT1, ui::EF_NONE,
        ui::DomKey::Constant<'1'>::Character},
       {ui::VKEY_1, ui::DomCode::DIGIT1, ui::EF_NONE,
        ui::DomKey::Constant<'1'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_2, ui::DomCode::DIGIT2, ui::EF_NONE,
        ui::DomKey::Constant<'2'>::Character},
       {ui::VKEY_2, ui::DomCode::DIGIT2, ui::EF_NONE,
        ui::DomKey::Constant<'2'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_3, ui::DomCode::DIGIT3, ui::EF_NONE,
        ui::DomKey::Constant<'3'>::Character},
       {ui::VKEY_3, ui::DomCode::DIGIT3, ui::EF_NONE,
        ui::DomKey::Constant<'3'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_4, ui::DomCode::DIGIT4, ui::EF_NONE,
        ui::DomKey::Constant<'4'>::Character},
       {ui::VKEY_4, ui::DomCode::DIGIT4, ui::EF_NONE,
        ui::DomKey::Constant<'4'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_5, ui::DomCode::DIGIT5, ui::EF_NONE,
        ui::DomKey::Constant<'5'>::Character},
       {ui::VKEY_5, ui::DomCode::DIGIT5, ui::EF_NONE,
        ui::DomKey::Constant<'5'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_6, ui::DomCode::DIGIT6, ui::EF_NONE,
        ui::DomKey::Constant<'6'>::Character},
       {ui::VKEY_6, ui::DomCode::DIGIT6, ui::EF_NONE,
        ui::DomKey::Constant<'6'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_7, ui::DomCode::DIGIT7, ui::EF_NONE,
        ui::DomKey::Constant<'7'>::Character},
       {ui::VKEY_7, ui::DomCode::DIGIT7, ui::EF_NONE,
        ui::DomKey::Constant<'7'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_8, ui::DomCode::DIGIT8, ui::EF_NONE,
        ui::DomKey::Constant<'8'>::Character},
       {ui::VKEY_8, ui::DomCode::DIGIT8, ui::EF_NONE,
        ui::DomKey::Constant<'8'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::DomCode::DIGIT9, ui::EF_NONE,
        ui::DomKey::Constant<'9'>::Character},
       {ui::VKEY_9, ui::DomCode::DIGIT9, ui::EF_NONE,
        ui::DomKey::Constant<'9'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_0, ui::DomCode::DIGIT0, ui::EF_NONE,
        ui::DomKey::Constant<'0'>::Character},
       {ui::VKEY_0, ui::DomCode::DIGIT0, ui::EF_NONE,
        ui::DomKey::Constant<'0'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_MINUS, ui::DomCode::MINUS, ui::EF_NONE,
        ui::DomKey::Constant<'-'>::Character},
       {ui::VKEY_OEM_MINUS, ui::DomCode::MINUS, ui::EF_NONE,
        ui::DomKey::Constant<'-'>::Character}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PLUS, ui::DomCode::EQUAL, ui::EF_NONE,
        ui::DomKey::Constant<'='>::Character},
       {ui::VKEY_OEM_PLUS, ui::DomCode::EQUAL, ui::EF_NONE,
        ui::DomKey::Constant<'='>::Character}},

      // The number row should be rewritten as the F<number> row with Search
      // key.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_1, ui::DomCode::DIGIT1, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'1'>::Character},
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_2, ui::DomCode::DIGIT2, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'2'>::Character},
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_3, ui::DomCode::DIGIT3, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'3'>::Character},
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_4, ui::DomCode::DIGIT4, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'4'>::Character},
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_5, ui::DomCode::DIGIT5, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'5'>::Character},
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_6, ui::DomCode::DIGIT6, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'6'>::Character},
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_7, ui::DomCode::DIGIT7, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'7'>::Character},
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_8, ui::DomCode::DIGIT8, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'8'>::Character},
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::DomCode::DIGIT9, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'9'>::Character},
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_0, ui::DomCode::DIGIT0, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'0'>::Character},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_MINUS, ui::DomCode::MINUS, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'-'>::Character},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PLUS, ui::DomCode::EQUAL, ui::EF_COMMAND_DOWN,
        ui::DomKey::Constant<'='>::Character},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12}},

      // The function keys should not be rewritten with Search key pressed.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_COMMAND_DOWN, ui::DomKey::F1},
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_COMMAND_DOWN, ui::DomKey::F2},
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_COMMAND_DOWN, ui::DomKey::F3},
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_COMMAND_DOWN, ui::DomKey::F4},
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_COMMAND_DOWN, ui::DomKey::F5},
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_COMMAND_DOWN, ui::DomKey::F6},
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_COMMAND_DOWN, ui::DomKey::F7},
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_COMMAND_DOWN, ui::DomKey::F8},
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_COMMAND_DOWN, ui::DomKey::F9},
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_COMMAND_DOWN, ui::DomKey::F10},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_COMMAND_DOWN, ui::DomKey::F11},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_COMMAND_DOWN, ui::DomKey::F12},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12}}};

  for (const auto& test : tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteExtendedKeysWithSearchRemapped) {
  // Remap Search to Control.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kControlKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // Alt+Search+Down -> End
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::DomKey::ARROW_DOWN},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_NONE, ui::DomKey::END}},

      // Shift+Alt+Search+Down -> Shift+End
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_DOWN},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_SHIFT_DOWN, ui::DomKey::END}},
  };

  for (const auto& test : tests) {
    CheckKeyTestCase(&rewriter, test);
  }
}

TEST_F(EventRewriterTest, TestRewriteKeyEventSentByXSendEvent) {
  // Remap Control to Alt.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapControlKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kAltKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  // Send left control press.
  {
    ui::KeyEvent keyevent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL,
                          ui::DomCode::CONTROL_LEFT, ui::EF_FINAL,
                          ui::DomKey::CONTROL, ui::EventTimeForNow());
    std::unique_ptr<ui::Event> new_event;
    // Control should NOT be remapped to Alt if EF_FINAL is set.
    EXPECT_EQ(ui::EVENT_REWRITE_CONTINUE,
              rewriter.RewriteEvent(keyevent, &new_event));
    EXPECT_FALSE(new_event);
  }
#if defined(USE_X11)
  // Send left control press, using XI2 native events.
  {
    ui::ScopedXI2Event xev;
    xev.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, 0);
    XEvent* xevent = xev;
    xevent->xkey.keycode = XKeysymToKeycode(gfx::GetXDisplay(), XK_Control_L);
    xevent->xkey.send_event = True;  // XSendEvent() always does this.
    ui::KeyEvent keyevent(xev);
    std::unique_ptr<ui::Event> new_event;
    // Control should NOT be remapped to Alt if send_event
    // flag in the event is True.
    EXPECT_EQ(ui::EVENT_REWRITE_CONTINUE,
              rewriter.RewriteEvent(keyevent, &new_event));
    EXPECT_FALSE(new_event);
  }
#endif
}

TEST_F(EventRewriterTest, TestRewriteNonNativeEvent) {
  // Remap Control to Alt.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapControlKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kAltKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  const int kTouchId = 2;
  gfx::Point location(0, 0);
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, location, kTouchId,
                       base::TimeDelta());
  press.set_flags(ui::EF_CONTROL_DOWN);
#if defined(USE_X11)
  ui::UpdateX11EventForFlags(&press);
#endif

  std::unique_ptr<ui::Event> new_event;
  rewriter.RewriteEvent(press, &new_event);
  EXPECT_TRUE(new_event);
  // Control should be remapped to Alt.
  EXPECT_EQ(ui::EF_ALT_DOWN,
            new_event->flags() & (ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
}

// Keeps a buffer of handled events.
class EventBuffer : public ui::test::TestEventProcessor {
 public:
  EventBuffer() {}
  ~EventBuffer() override {}

  void PopEvents(ScopedVector<ui::Event>* events) {
    events->clear();
    events->swap(events_);
  }

 private:
  // ui::EventProcessor overrides:
  ui::EventDispatchDetails OnEventFromSource(ui::Event* event) override {
    events_.push_back(ui::Event::Clone(*event));
    return ui::EventDispatchDetails();
  }

  ScopedVector<ui::Event> events_;

  DISALLOW_COPY_AND_ASSIGN(EventBuffer);
};

// Trivial EventSource that does nothing but send events.
class TestEventSource : public ui::EventSource {
 public:
  explicit TestEventSource(ui::EventProcessor* processor)
      : processor_(processor) {}
  ui::EventProcessor* GetEventProcessor() override { return processor_; }
  ui::EventDispatchDetails Send(ui::Event* event) {
    return SendEventToProcessor(event);
  }

 private:
  ui::EventProcessor* processor_;
};

// Tests of event rewriting that depend on the Ash window manager.
class EventRewriterAshTest : public ash::test::AshTestBase {
 public:
  EventRewriterAshTest()
      : source_(&buffer_),
        fake_user_manager_(new user_manager::FakeUserManager),
        user_manager_enabler_(fake_user_manager_) {}
  ~EventRewriterAshTest() override {}

  bool RewriteFunctionKeys(const ui::Event& event,
                           std::unique_ptr<ui::Event>* rewritten_event) {
    return rewriter_->RewriteEvent(event, rewritten_event);
  }

  ui::EventDispatchDetails Send(ui::Event* event) {
    return source_.Send(event);
  }

  void SendKeyEvent(ui::EventType type,
                    ui::KeyboardCode key_code,
                    ui::DomCode code,
                    ui::DomKey key) {
    ui::KeyEvent press(type, key_code, code, ui::EF_NONE, key,
                       ui::EventTimeForNow());
    ui::EventDispatchDetails details = Send(&press);
    CHECK(!details.dispatcher_destroyed);
  }

  void SendActivateStickyKeyPattern(ui::KeyboardCode key_code,
                                    ui::DomCode code,
                                    ui::DomKey key) {
    SendKeyEvent(ui::ET_KEY_PRESSED, key_code, code, key);
    SendKeyEvent(ui::ET_KEY_RELEASED, key_code, code, key);
  }

 protected:
  syncable_prefs::TestingPrefServiceSyncable* prefs() { return &prefs_; }

  void PopEvents(ScopedVector<ui::Event>* events) { buffer_.PopEvents(events); }

  void SetUp() override {
    AshTestBase::SetUp();
    sticky_keys_controller_ =
        ash::Shell::GetInstance()->sticky_keys_controller();
    rewriter_.reset(new EventRewriter(sticky_keys_controller_));
    chromeos::Preferences::RegisterProfilePrefs(prefs_.registry());
    rewriter_->set_pref_service_for_testing(&prefs_);
#if defined(USE_X11)
    ui::SetUpTouchPadForTest(kTouchPadDeviceId);
#endif
    source_.AddEventRewriter(rewriter_.get());
    sticky_keys_controller_->Enable(true);
  }

  void TearDown() override {
    rewriter_.reset();
    AshTestBase::TearDown();
  }

 protected:
  ash::StickyKeysController* sticky_keys_controller_;

 private:
  std::unique_ptr<EventRewriter> rewriter_;

  EventBuffer buffer_;
  TestEventSource source_;

  user_manager::FakeUserManager* fake_user_manager_;  // Not owned.
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  syncable_prefs::TestingPrefServiceSyncable prefs_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriterAshTest);
};

TEST_F(EventRewriterAshTest, TopRowKeysAreFunctionKeys) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(1));
  ash::wm::WindowState* window_state = ash::wm::GetWindowState(window.get());
  window_state->Activate();
  ScopedVector<ui::Event> events;

  // Create a simulated keypress of F1 targetted at the window.
  ui::KeyEvent press_f1(ui::ET_KEY_PRESSED, ui::VKEY_F1, ui::DomCode::F1,
                        ui::EF_NONE, ui::DomKey::F1, ui::EventTimeForNow());

  // Simulate an apps v2 window that has requested top row keys as function
  // keys. The event should not be rewritten.
  window_state->set_top_row_keys_are_function_keys(true);
  ui::EventDispatchDetails details = Send(&press_f1);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(
      GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_F1,
                                ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1),
      GetKeyEventAsString(*static_cast<ui::KeyEvent*>(events[0])));

  // The event should also not be rewritten if the send-function-keys pref is
  // additionally set, for both apps v2 and regular windows.
  BooleanPrefMember send_function_keys_pref;
  send_function_keys_pref.Init(prefs::kLanguageSendFunctionKeys, prefs());
  send_function_keys_pref.SetValue(true);
  window_state->set_top_row_keys_are_function_keys(false);
  details = Send(&press_f1);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(
      GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_F1,
                                ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1),
      GetKeyEventAsString(*static_cast<ui::KeyEvent*>(events[0])));

  // If the pref isn't set when an event is sent to a regular window, F1 is
  // rewritten to the back key.
  send_function_keys_pref.SetValue(false);
  details = Send(&press_f1);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_BROWSER_BACK,
                                      ui::DomCode::BROWSER_BACK, ui::EF_NONE,
                                      ui::DomKey::BROWSER_BACK),
            GetKeyEventAsString(*static_cast<ui::KeyEvent*>(events[0])));
}

TEST_F(EventRewriterTest, TestRewrittenModifierClick) {
#if defined(USE_X11)
  std::vector<int> device_list;
  device_list.push_back(10);
  ui::TouchFactory::GetInstance()->SetPointerDeviceForTest(device_list);

  // Remap Control to Alt.
  syncable_prefs::TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapControlKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kAltKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  // Check that Control + Left Button is converted (via Alt + Left Button)
  // to Right Button.
  ui::ScopedXI2Event xev;
  xev.InitGenericButtonEvent(10, ui::ET_MOUSE_PRESSED, gfx::Point(),
                             ui::EF_LEFT_MOUSE_BUTTON | ui::EF_CONTROL_DOWN);
  ui::MouseEvent press(xev);
  // Sanity check.
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_CONTROL_DOWN, press.flags());
  std::unique_ptr<ui::Event> new_event;
  const ui::MouseEvent* result =
      RewriteMouseButtonEvent(&rewriter, press, &new_event);
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
  EXPECT_FALSE(ui::EF_LEFT_MOUSE_BUTTON & result->flags());
  EXPECT_FALSE(ui::EF_CONTROL_DOWN & result->flags());
  EXPECT_FALSE(ui::EF_ALT_DOWN & result->flags());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
#endif
}

TEST_F(EventRewriterTest, DontRewriteIfNotRewritten) {
// TODO(kpschoedel): pending changes for crbug.com/360377
// to |chromeos::EventRewriter::RewriteLocatedEvent()
#if defined(USE_X11)
  std::vector<int> device_list;
  device_list.push_back(10);
  device_list.push_back(11);
  ui::TouchFactory::GetInstance()->SetPointerDeviceForTest(device_list);
#endif
  syncable_prefs::TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.set_pref_service_for_testing(&prefs);
  const int kLeftAndAltFlag = ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN;

  // Test Alt + Left click.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), kLeftAndAltFlag,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(10);
    // Sanity check.
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(kLeftAndAltFlag, press.flags());
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), kLeftAndAltFlag,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(10);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
#if defined(USE_X11)
  // Test Alt + Left click, using XI2 native events.
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(10, ui::ET_MOUSE_PRESSED, gfx::Point(),
                               kLeftAndAltFlag);
    ui::MouseEvent press(xev);
    // Sanity check.
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(kLeftAndAltFlag, press.flags());
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(10, ui::ET_MOUSE_RELEASED, gfx::Point(),
                               kLeftAndAltFlag);
    ui::MouseEvent release(xev);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
#endif

  // No ALT in frst click.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(10);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), kLeftAndAltFlag,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(10);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE((ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN) & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
#if defined(USE_X11)
  // No ALT in frst click, using XI2 native events.
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(10, ui::ET_MOUSE_PRESSED, gfx::Point(),
                               ui::EF_LEFT_MOUSE_BUTTON);
    ui::MouseEvent press(xev);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(10, ui::ET_MOUSE_RELEASED, gfx::Point(),
                               kLeftAndAltFlag);
    ui::MouseEvent release(xev);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE((ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN) & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
#endif

  // ALT on different device.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), kLeftAndAltFlag,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(11);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), kLeftAndAltFlag,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(10);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE((ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN) & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), kLeftAndAltFlag,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(11);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
#if defined(USE_X11)
  // ALT on different device, using XI2 native events.
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(11, ui::ET_MOUSE_PRESSED, gfx::Point(),
                               kLeftAndAltFlag);
    ui::MouseEvent press(xev);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(10, ui::ET_MOUSE_RELEASED, gfx::Point(),
                               kLeftAndAltFlag);
    ui::MouseEvent release(xev);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE((ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN) & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(11, ui::ET_MOUSE_RELEASED, gfx::Point(),
                               kLeftAndAltFlag);
    ui::MouseEvent release(xev);
    std::unique_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
#endif
}

TEST_F(EventRewriterAshTest, StickyKeyEventDispatchImpl) {
  // Test the actual key event dispatch implementation.
  ScopedVector<ui::Event> events;

  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_KEY_PRESSED, events[0]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[0])->key_code());

  // Test key press event is correctly modified and modifier release
  // event is sent.
  ui::KeyEvent press(ui::ET_KEY_PRESSED, ui::VKEY_C, ui::DomCode::US_C,
                     ui::EF_NONE, ui::DomKey::Constant<'c'>::Character,
                     ui::EventTimeForNow());
  ui::EventDispatchDetails details = Send(&press);
  PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_KEY_PRESSED, events[0]->type());
  EXPECT_EQ(ui::VKEY_C, static_cast<ui::KeyEvent*>(events[0])->key_code());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[1])->key_code());

  // Test key release event is not modified.
  ui::KeyEvent release(ui::ET_KEY_RELEASED, ui::VKEY_C, ui::DomCode::US_C,
                       ui::EF_NONE, ui::DomKey::Constant<'c'>::Character,
                       ui::EventTimeForNow());
  details = Send(&release);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[0]->type());
  EXPECT_EQ(ui::VKEY_C, static_cast<ui::KeyEvent*>(events[0])->key_code());
  EXPECT_FALSE(events[0]->flags() & ui::EF_CONTROL_DOWN);
}

TEST_F(EventRewriterAshTest, MouseEventDispatchImpl) {
  ScopedVector<ui::Event> events;

  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  PopEvents(&events);

  // Test mouse press event is correctly modified.
  gfx::Point location(0, 0);
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventDispatchDetails details = Send(&press);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0]->type());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);

  // Test mouse release event is correctly modified and modifier release
  // event is sent. The mouse event should have the correct DIP location.
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, location, location,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  details = Send(&release);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[0]->type());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[1])->key_code());
}

TEST_F(EventRewriterAshTest, MouseWheelEventDispatchImpl) {
  ScopedVector<ui::Event> events;

  // Test positive mouse wheel event is correctly modified and modifier release
  // event is sent.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  PopEvents(&events);
  gfx::Point location(0, 0);
  ui::MouseEvent mev(ui::ET_MOUSEWHEEL, location, location,
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseWheelEvent positive(mev, 0, ui::MouseWheelEvent::kWheelDelta);
  ui::EventDispatchDetails details = Send(&positive);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  EXPECT_TRUE(events[0]->IsMouseWheelEvent());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[1])->key_code());

  // Test negative mouse wheel event is correctly modified and modifier release
  // event is sent.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  PopEvents(&events);
  ui::MouseWheelEvent negative(mev, 0, -ui::MouseWheelEvent::kWheelDelta);
  details = Send(&negative);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  EXPECT_TRUE(events[0]->IsMouseWheelEvent());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[1])->key_code());
}

// Tests that if modifier keys are remapped, the flags of a mouse wheel event
// will be rewritten properly.
TEST_F(EventRewriterAshTest, MouseWheelEventModifiersRewritten) {
  // Remap Control to Alt.
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapControlKeyTo, prefs());
  control.SetValue(chromeos::input_method::kAltKey);

  // Generate a mouse wheel event that has a CONTROL_DOWN modifier flag and
  // expect that it will be rewritten to ALT_DOWN.
  ScopedVector<ui::Event> events;
  gfx::Point location(0, 0);
  ui::MouseEvent mev(
      ui::ET_MOUSEWHEEL, location, location, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_CONTROL_DOWN, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseWheelEvent positive(mev, 0, ui::MouseWheelEvent::kWheelDelta);
  ui::EventDispatchDetails details = Send(&positive);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_TRUE(events[0]->IsMouseWheelEvent());
  EXPECT_FALSE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(events[0]->flags() & ui::EF_ALT_DOWN);
}

class StickyKeysOverlayTest : public EventRewriterAshTest {
 public:
  StickyKeysOverlayTest() : overlay_(NULL) {}

  ~StickyKeysOverlayTest() override {}

  void SetUp() override {
    EventRewriterAshTest::SetUp();
    overlay_ = sticky_keys_controller_->GetOverlayForTest();
    ASSERT_TRUE(overlay_);
  }

  ash::StickyKeysOverlay* overlay_;
};

TEST_F(StickyKeysOverlayTest, OneModifierEnabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing modifier key should show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_T, ui::DomCode::US_T,
                               ui::DomKey::Constant<'t'>::Character);
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
}

TEST_F(StickyKeysOverlayTest, TwoModifiersEnabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing two modifiers should show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_N, ui::DomCode::US_N,
                               ui::DomKey::Constant<'n'>::Character);
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
}

TEST_F(StickyKeysOverlayTest, LockedModifier) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Pressing a modifier key twice should lock modifier and show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Pressing a normal key should not hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_D, ui::DomCode::US_D,
                               ui::DomKey::Constant<'d'>::Character);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
}

TEST_F(StickyKeysOverlayTest, LockedAndNormalModifier) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing a modifier key twice should lock modifier and show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing another modifier key should still show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing a normal key should not hide overlay but disable normal modifier.
  SendActivateStickyKeyPattern(ui::VKEY_D, ui::DomCode::US_D,
                               ui::DomKey::Constant<'d'>::Character);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
}

TEST_F(StickyKeysOverlayTest, ModifiersDisabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_COMMAND_DOWN));

  // Enable modifiers.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  SendActivateStickyKeyPattern(ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
                               ui::DomKey::META);
  SendActivateStickyKeyPattern(ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
                               ui::DomKey::META);

  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_COMMAND_DOWN));

  // Disable modifiers and overlay should be hidden.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  SendActivateStickyKeyPattern(ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
                               ui::DomKey::META);

  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_COMMAND_DOWN));
}

TEST_F(StickyKeysOverlayTest, ModifierVisibility) {
  // All but AltGr and Mod3 should initially be visible.
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_CONTROL_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_COMMAND_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn all modifiers on.
  sticky_keys_controller_->SetModifiersEnabled(true, true);
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_CONTROL_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off Mod3.
  sticky_keys_controller_->SetModifiersEnabled(false, true);
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off AltGr.
  sticky_keys_controller_->SetModifiersEnabled(true, false);
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off AltGr and Mod3.
  sticky_keys_controller_->SetModifiersEnabled(false, false);
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));
}

}  // namespace chromeos
