// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/event_rewriter.h"

#include <vector>

#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/sticky_keys/sticky_keys_overlay.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/prefs/pref_member.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chromeos/chromeos_switches.h"
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
const int kMasterKeyboardDeviceId = 3;
#endif
const int kKeyboardDeviceId = 2;

std::string GetExpectedResultAsString(ui::EventType ui_type,
                                      ui::KeyboardCode ui_keycode,
                                      ui::DomCode code,
                                      int ui_flags,  // ui::EventFlags
                                      ui::DomKey key,
                                      base::char16 character) {
  return base::StringPrintf("ui_keycode=0x%X ui_flags=0x%X ui_type=%d",
                            ui_keycode,
                            ui_flags & ~ui::EF_IS_REPEAT,
                            ui_type);
}

std::string GetKeyEventAsString(const ui::KeyEvent& keyevent) {
  return GetExpectedResultAsString(
      keyevent.type(), keyevent.key_code(), keyevent.code(), keyevent.flags(),
      keyevent.GetDomKey(), keyevent.GetCharacter());
}

std::string GetRewrittenEventAsString(chromeos::EventRewriter* rewriter,
                                      ui::EventType ui_type,
                                      ui::KeyboardCode ui_keycode,
                                      ui::DomCode code,
                                      int ui_flags,  // ui::EventFlags
                                      ui::DomKey key,
                                      base::char16 character) {
  const ui::KeyEvent event(ui_type, ui_keycode, code, ui_flags, key, character,
                           ui::EventTimeForNow());
  scoped_ptr<ui::Event> new_event;
  rewriter->RewriteEvent(event, &new_event);
  if (new_event)
    return GetKeyEventAsString(
        static_cast<const ui::KeyEvent&>(*new_event.get()));
  return GetKeyEventAsString(event);
}

// Table entry for simple single key event rewriting tests.
struct KeyTestCase {
  enum {
    // Test types:
    TEST_VKEY = 1 << 0,  // Test ui::KeyEvent with no native event
    TEST_X11 = 1 << 1,   // Test ui::KeyEvent with native XKeyEvent
    TEST_ALL = TEST_VKEY | TEST_X11,
    // Special test flags:
    NUMPAD = 1 << 8,  // Reset the XKB scan code on an X11 event based
                      // on the test DomCode, because
                      // |XKeysymForWindowsKeyCode()| can not distinguish
                      // between pairs like XK_Insert and XK_KP_Insert.
  };
  int test;
  ui::EventType type;
  struct Event {
    ui::KeyboardCode key_code;
    ui::DomCode code;
    int flags;  // ui::EventFlags
    ui::DomKey key;
    base::char16 character;
  } input, expected;
};

std::string GetTestCaseAsString(ui::EventType ui_type,
                                const KeyTestCase::Event& test) {
  return GetExpectedResultAsString(ui_type, test.key_code, test.code,
                                   test.flags, test.key, test.character);
}

#if defined(USE_X11)
// Check rewriting of an X11-based key event.
void CheckX11KeyTestCase(const std::string& expected,
                         chromeos::EventRewriter* rewriter,
                         const KeyTestCase& test,
                         XEvent* xevent) {
  ui::KeyEvent xkey_event(xevent);
  // Rewrite the event and check the result.
  scoped_ptr<ui::Event> new_event;
  rewriter->RewriteEvent(xkey_event, &new_event);
  ui::KeyEvent& rewritten_key_event =
      new_event ? *static_cast<ui::KeyEvent*>(new_event.get()) : xkey_event;
  EXPECT_EQ(expected, GetKeyEventAsString(rewritten_key_event));
  if ((rewritten_key_event.key_code() != ui::VKEY_UNKNOWN) &&
      (rewritten_key_event.native_event()->xkey.keycode != 0)) {
    // Build a new ui::KeyEvent from the rewritten native component,
    // and check that it also matches the rewritten event.
    EXPECT_TRUE(rewritten_key_event.native_event());
    ui::KeyEvent from_native_event(rewritten_key_event.native_event());
    EXPECT_EQ(expected, GetKeyEventAsString(from_native_event));
  }
}
#endif

// Tests a single stateless key rewrite operation.
void CheckKeyTestCase(chromeos::EventRewriter* rewriter,
                      const KeyTestCase& test) {
  std::string expected = GetTestCaseAsString(test.type, test.expected);

  if (test.test & KeyTestCase::TEST_VKEY) {
    // Check rewriting of a non-native-based key event.
    EXPECT_EQ(expected,
              GetRewrittenEventAsString(
                  rewriter, test.type, test.input.key_code, test.input.code,
                  test.input.flags, test.input.key, test.input.character));
  }

#if defined(USE_X11)
  if (test.test & KeyTestCase::TEST_X11) {
    ui::ScopedXI2Event xev;
    // Test an XKeyEvent.
    xev.InitKeyEvent(test.type, test.input.key_code, test.input.flags);
    XEvent* xevent = xev;
    DCHECK((xevent->type == KeyPress) || (xevent->type == KeyRelease));
    if (test.test & KeyTestCase::NUMPAD) {
      xevent->xkey.keycode =
          ui::KeycodeConverter::DomCodeToNativeKeycode(test.input.code);
    }
    int keycode = xevent->xkey.keycode;
    if (keycode) {
      CheckX11KeyTestCase(expected, rewriter, test, xevent);
      // Test an XI2 GenericEvent.
      xev.InitGenericKeyEvent(kMasterKeyboardDeviceId, kKeyboardDeviceId,
                              test.type, test.input.key_code, test.input.flags);
      xevent = xev;
      DCHECK(xevent->type == GenericEvent);
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(xevent->xcookie.data);
      DCHECK((xievent->evtype == XI_KeyPress) ||
             (xievent->evtype == XI_KeyRelease));
      xievent->detail = keycode;
      CheckX11KeyTestCase(expected, rewriter, test, xevent);
    }
  }
#endif
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
      scoped_ptr<ui::Event>* new_event) {
    rewriter->RewriteMouseButtonEventForTesting(event, new_event);
    return *new_event ? static_cast<const ui::MouseEvent*>(new_event->get())
                      : &event;
  }

  user_manager::FakeUserManager* fake_user_manager_;  // Not owned.
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  chromeos::input_method::MockInputMethodManager* input_method_manager_mock_;
};

TEST_F(EventRewriterTest, TestRewriteCommandToControl) {
  // First, test with a PC keyboard.
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase pc_keyboard_tests[] = {
      // VKEY_A, Alt modifier.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        'a'}},

      // VKEY_A, Win modifier.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        'a'}},

      // VKEY_A, Alt+Win modifier.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        'a'}},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0}},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN,
        ui::DomCode::OS_RIGHT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_RWIN,
        ui::DomCode::OS_RIGHT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0}},
  };

  for (size_t i = 0; i < arraysize(pc_keyboard_tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, pc_keyboard_tests[i]);
  }

  // An Apple keyboard reusing the ID, zero.
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "Apple Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);

  KeyTestCase apple_keyboard_tests[] = {
      // VKEY_A, Alt modifier.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        'a'}},

      // VKEY_A, Win modifier.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CHARACTER,
        'a'}},

      // VKEY_A, Alt+Win modifier.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::CHARACTER,
        'a'}},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::CONTROL,
        0}},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN,
        ui::DomCode::OS_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::CONTROL,
        0}},
  };

  for (size_t i = 0; i < arraysize(apple_keyboard_tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, apple_keyboard_tests[i]);
  }
}

// For crbug.com/133896.
TEST_F(EventRewriterTest, TestRewriteCommandToControlWithControlRemapped) {
  // Remap Control to Alt.
  TestingPrefServiceSyncable prefs;
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
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0},
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0}},
  };

  for (size_t i = 0; i < arraysize(pc_keyboard_tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, pc_keyboard_tests[i]);
  }

  // An Apple keyboard reusing the ID, zero.
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "Apple Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);

  KeyTestCase apple_keyboard_tests[] = {
      // VKEY_LWIN (left Command key) with  Alt modifier. The remapped Command
      // key should never be re-remapped to Alt.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::CONTROL,
        0}},

      // VKEY_RWIN (right Command key) with  Alt modifier. The remapped Command
      // key should never be re-remapped to Alt.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN,
        ui::DomCode::OS_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::CONTROL,
        0}},
  };

  for (size_t i = 0; i < arraysize(apple_keyboard_tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, apple_keyboard_tests[i]);
  }
}

void EventRewriterTest::TestRewriteNumPadKeys() {
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // XK_KP_Insert (= NumPad 0 without Num Lock), no modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_INSERT,
        ui::DomCode::NUMPAD0,
        ui::EF_NONE,
        ui::DomKey::INSERT,
        0},
       {ui::VKEY_NUMPAD0,
        ui::DomCode::NUMPAD0,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '0'}},

      // XK_KP_Insert (= NumPad 0 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_INSERT,
        ui::DomCode::NUMPAD0,
        ui::EF_ALT_DOWN,
        ui::DomKey::INSERT,
        0},
       {ui::VKEY_NUMPAD0,
        ui::DomCode::NUMPAD0,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        '0'}},

      // XK_KP_Delete (= NumPad . without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_DELETE,
        ui::DomCode::NUMPAD_DECIMAL,
        ui::EF_ALT_DOWN,
        ui::DomKey::DEL,
        0},
       {ui::VKEY_DECIMAL,
        ui::DomCode::NUMPAD_DECIMAL,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        '.'}},

      // XK_KP_End (= NumPad 1 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_END,
        ui::DomCode::NUMPAD1,
        ui::EF_ALT_DOWN,
        ui::DomKey::END,
        0},
       {ui::VKEY_NUMPAD1,
        ui::DomCode::NUMPAD1,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        '1'}},

      // XK_KP_Down (= NumPad 2 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN,
        ui::DomCode::NUMPAD2,
        ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_DOWN,
        0},
       {ui::VKEY_NUMPAD2,
        ui::DomCode::NUMPAD2,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        '2'}},

      // XK_KP_Next (= NumPad 3 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_NEXT,
        ui::DomCode::NUMPAD3,
        ui::EF_ALT_DOWN,
        ui::DomKey::PAGE_DOWN,
        0},
       {ui::VKEY_NUMPAD3,
        ui::DomCode::NUMPAD3,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        '3'}},

      // XK_KP_Left (= NumPad 4 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LEFT,
        ui::DomCode::NUMPAD4,
        ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_LEFT,
        0},
       {ui::VKEY_NUMPAD4,
        ui::DomCode::NUMPAD4,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        '4'}},

      // XK_KP_Begin (= NumPad 5 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_CLEAR,
        ui::DomCode::NUMPAD5,
        ui::EF_ALT_DOWN,
        ui::DomKey::CLEAR,
        0},
       {ui::VKEY_NUMPAD5,
        ui::DomCode::NUMPAD5,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        '5'}},

      // XK_KP_Right (= NumPad 6 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_RIGHT,
        ui::DomCode::NUMPAD6,
        ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_RIGHT,
        0},
       {ui::VKEY_NUMPAD6,
        ui::DomCode::NUMPAD6,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        '6'}},

      // XK_KP_Home (= NumPad 7 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_HOME,
        ui::DomCode::NUMPAD7,
        ui::EF_ALT_DOWN,
        ui::DomKey::HOME,
        0},
       {ui::VKEY_NUMPAD7,
        ui::DomCode::NUMPAD7,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        '7'}},

      // XK_KP_Up (= NumPad 8 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_UP,
        ui::DomCode::NUMPAD8,
        ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_UP,
        0},
       {ui::VKEY_NUMPAD8,
        ui::DomCode::NUMPAD8,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        '8'}},

      // XK_KP_Prior (= NumPad 9 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_PRIOR,
        ui::DomCode::NUMPAD9,
        ui::EF_ALT_DOWN,
        ui::DomKey::PAGE_UP,
        0},
       {ui::VKEY_NUMPAD9,
        ui::DomCode::NUMPAD9,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        '9'}},

      // XK_KP_0 (= NumPad 0 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD0,
        ui::DomCode::NUMPAD0,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '0'},
       {ui::VKEY_NUMPAD0,
        ui::DomCode::NUMPAD0,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '0'}},

      // XK_KP_DECIMAL (= NumPad . with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_DECIMAL,
        ui::DomCode::NUMPAD_DECIMAL,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '.'},
       {ui::VKEY_DECIMAL,
        ui::DomCode::NUMPAD_DECIMAL,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '.'}},

      // XK_KP_1 (= NumPad 1 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD1,
        ui::DomCode::NUMPAD1,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '1'},
       {ui::VKEY_NUMPAD1,
        ui::DomCode::NUMPAD1,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '1'}},

      // XK_KP_2 (= NumPad 2 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD2,
        ui::DomCode::NUMPAD2,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '2'},
       {ui::VKEY_NUMPAD2,
        ui::DomCode::NUMPAD2,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '2'}},

      // XK_KP_3 (= NumPad 3 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD3,
        ui::DomCode::NUMPAD3,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '3'},
       {ui::VKEY_NUMPAD3,
        ui::DomCode::NUMPAD3,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '3'}},

      // XK_KP_4 (= NumPad 4 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD4,
        ui::DomCode::NUMPAD4,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '4'},
       {ui::VKEY_NUMPAD4,
        ui::DomCode::NUMPAD4,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '4'}},

      // XK_KP_5 (= NumPad 5 with Num Lock), Num Lock
      // modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD5,
        ui::DomCode::NUMPAD5,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '5'},
       {ui::VKEY_NUMPAD5,
        ui::DomCode::NUMPAD5,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '5'}},

      // XK_KP_6 (= NumPad 6 with Num Lock), Num Lock
      // modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD6,
        ui::DomCode::NUMPAD6,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '6'},
       {ui::VKEY_NUMPAD6,
        ui::DomCode::NUMPAD6,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '6'}},

      // XK_KP_7 (= NumPad 7 with Num Lock), Num Lock
      // modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD7,
        ui::DomCode::NUMPAD7,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '7'},
       {ui::VKEY_NUMPAD7,
        ui::DomCode::NUMPAD7,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '7'}},

      // XK_KP_8 (= NumPad 8 with Num Lock), Num Lock
      // modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD8,
        ui::DomCode::NUMPAD8,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '8'},
       {ui::VKEY_NUMPAD8,
        ui::DomCode::NUMPAD8,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '8'}},

      // XK_KP_9 (= NumPad 9 with Num Lock), Num Lock
      // modifier.
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD9,
        ui::DomCode::NUMPAD9,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '9'},
       {ui::VKEY_NUMPAD9,
        ui::DomCode::NUMPAD9,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '9'}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
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
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "Apple Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // XK_KP_End (= NumPad 1 without Num Lock), Win modifier.
      // The result should be "Num Pad 1 with Control + Num Lock modifiers".
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_END,
        ui::DomCode::NUMPAD1,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::END,
        0},
       {ui::VKEY_NUMPAD1,
        ui::DomCode::NUMPAD1,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CHARACTER,
        '1'}},

      // XK_KP_1 (= NumPad 1 with Num Lock), Win modifier.
      // The result should also be "Num Pad 1 with Control + Num Lock
      // modifiers".
      {KeyTestCase::TEST_ALL | KeyTestCase::NUMPAD,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD1,
        ui::DomCode::NUMPAD1,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '1'},
       {ui::VKEY_NUMPAD1,
        ui::DomCode::NUMPAD1,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CHARACTER,
        '1'}}};

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
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
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // Press Search. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::OS_LEFT, ui::EF_NONE, ui::DomKey::OS, 0},
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0}},

      // Press left Control. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0}},

      // Press right Control. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0}},

      // Press left Alt. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0},
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0}},

      // Press right Alt. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0},
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0}},

      // Test KeyRelease event, just in case.
      // Release Search. Confirm the release event is not rewritten.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_RELEASED,
       {ui::VKEY_LWIN, ui::DomCode::OS_LEFT, ui::EF_NONE, ui::DomKey::OS, 0},
       {ui::VKEY_LWIN, ui::DomCode::OS_LEFT, ui::EF_NONE, ui::DomKey::OS, 0}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersNoRemapMultipleKeys) {
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // Press Alt with Shift. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0},
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0}},

      // Press Search with Caps Lock mask. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_CAPS_LOCK_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_CAPS_LOCK_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0}},

      // Release Search with Caps Lock mask. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_RELEASED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_CAPS_LOCK_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_CAPS_LOCK_DOWN,
        ui::DomKey::OS,
        0}},

      // Press Shift+Ctrl+Alt+Search+A. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_B,
        ui::DomCode::KEY_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        'B'},
       {ui::VKEY_B,
        ui::DomCode::KEY_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        'B'}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersDisableSome) {
  // Disable Search and Control keys.
  TestingPrefServiceSyncable prefs;
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
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0},
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0}},

      // Press Search. Confirm the event is now VKEY_UNKNOWN.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::DomCode::OS_LEFT, ui::EF_NONE, ui::DomKey::OS, 0},
       {ui::VKEY_UNKNOWN,
        ui::DomCode::NONE,
        ui::EF_NONE,
        ui::DomKey::UNIDENTIFIED,
        0}},

      // Press Control. Confirm the event is now VKEY_UNKNOWN.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0},
       {ui::VKEY_UNKNOWN,
        ui::DomCode::NONE,
        ui::EF_NONE,
        ui::DomKey::UNIDENTIFIED,
        0}},

      // Press Control+Search. Confirm the event is now VKEY_UNKNOWN
      // without any modifiers.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_UNKNOWN,
        ui::DomCode::NONE,
        ui::EF_NONE,
        ui::DomKey::UNIDENTIFIED,
        0}},

      // Press Control+Search+a. Confirm the event is now VKEY_A without any
      // modifiers.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        'a'}},

      // Press Control+Search+Alt+a. Confirm the event is now VKEY_A only with
      // the Alt modifier.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        'a'}},
  };

  for (size_t i = 0; i < arraysize(disabled_modifier_tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, disabled_modifier_tests[i]);
  }

  // Remap Alt to Control.
  IntegerPrefMember alt;
  alt.Init(prefs::kLanguageRemapAltKeyTo, &prefs);
  alt.SetValue(chromeos::input_method::kControlKey);

  KeyTestCase tests[] = {
      // Press left Alt. Confirm the event is now VKEY_CONTROL
      // even though the Control key itself is disabled.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0}},

      // Press Alt+a. Confirm the event is now Control+a even though the Control
      // key itself is disabled.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CHARACTER,
        'a'}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToControl) {
  // Remap Search to Control.
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kControlKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase s_tests[] = {
      // Press Search. Confirm the event is now VKEY_CONTROL.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0}},
  };

  for (size_t i = 0; i < arraysize(s_tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, s_tests[i]);
  }

  // Remap Alt to Control too.
  IntegerPrefMember alt;
  alt.Init(prefs::kLanguageRemapAltKeyTo, &prefs);
  alt.SetValue(chromeos::input_method::kControlKey);

  KeyTestCase sa_tests[] = {
      // Press Alt. Confirm the event is now VKEY_CONTROL.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0}},

      // Press Alt+Search. Confirm the event is now VKEY_CONTROL.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0}},

      // Press Control+Alt+Search. Confirm the event is now VKEY_CONTROL.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0}},

      // Press Shift+Control+Alt+Search. Confirm the event is now Control with
      // Shift and Control modifiers.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0}},

      // Press Shift+Control+Alt+Search+B. Confirm the event is now B with Shift
      // and Control modifiers.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_B,
        ui::DomCode::KEY_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        'B'},
       {ui::VKEY_B,
        ui::DomCode::KEY_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::CHARACTER,
        'B'}},
  };

  for (size_t i = 0; i < arraysize(sa_tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, sa_tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToEscape) {
  // Remap Search to ESC.
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kEscapeKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // Press Search. Confirm the event is now VKEY_ESCAPE.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_ESCAPE,
        ui::DomCode::ESCAPE,
        ui::EF_NONE,
        ui::DomKey::ESCAPE,
        0}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapMany) {
  // Remap Search to Alt.
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kAltKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase s2a_tests[] = {
      // Press Search. Confirm the event is now VKEY_MENU.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0}},
  };

  for (size_t i = 0; i < arraysize(s2a_tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, s2a_tests[i]);
  }

  // Remap Alt to Control.
  IntegerPrefMember alt;
  alt.Init(prefs::kLanguageRemapAltKeyTo, &prefs);
  alt.SetValue(chromeos::input_method::kControlKey);

  KeyTestCase a2c_tests[] = {
      // Press left Alt. Confirm the event is now VKEY_CONTROL.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_ALT_DOWN,
        ui::DomKey::ALT,
        0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0}},
      // Press Shift+comma. Verify that only the flags are changed.
      // The X11 portion of the test addresses crbug.com/390263 by verifying
      // that the X keycode remains that for ',<' and not for 105-key '<>'.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_COMMA,
        ui::DomCode::COMMA,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::UNIDENTIFIED,
        0},
       {ui::VKEY_OEM_COMMA,
        ui::DomCode::COMMA,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::UNIDENTIFIED,
        0}},
      // Press Shift+9. Verify that only the flags are changed.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_9,
        ui::DomCode::DIGIT9,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::CHARACTER,
        '9'},
       {ui::VKEY_9,
        ui::DomCode::DIGIT9,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::CHARACTER,
        '9'}},
  };

  for (size_t i = 0; i < arraysize(a2c_tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, a2c_tests[i]);
  }

  // Remap Control to Search.
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapControlKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kSearchKey);

  KeyTestCase c2s_tests[] = {
      // Press left Control. Confirm the event is now VKEY_LWIN.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0},
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0}},

      // Then, press all of the three, Control+Alt+Search.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::ALT,
        0}},

      // Press Shift+Control+Alt+Search.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::DomCode::OS_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::OS,
        0},
       {ui::VKEY_MENU,
        ui::DomCode::ALT_LEFT,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::ALT,
        0}},

      // Press Shift+Control+Alt+Search+B
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_B,
        ui::DomCode::KEY_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        'B'},
       {ui::VKEY_B,
        ui::DomCode::KEY_B,
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
            ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        'B'}},
  };

  for (size_t i = 0; i < arraysize(c2s_tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, c2s_tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToCapsLock) {
  // Remap Search to Caps Lock.
  TestingPrefServiceSyncable prefs;
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
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK,
                                      ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                      ui::DomKey::CAPS_LOCK, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_LWIN, ui::DomCode::OS_LEFT,
                                      ui::EF_COMMAND_DOWN, ui::DomKey::OS, 0));
  // Confirm that the Caps Lock status is changed.
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);

  // Release Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK, ui::EF_NONE,
                                      ui::DomKey::CAPS_LOCK, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_LWIN, ui::DomCode::OS_LEFT,
                                      ui::EF_NONE, ui::DomKey::OS, 0));
  // Confirm that the Caps Lock status is not changed.
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);

  // Press Search.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
          ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN, ui::DomKey::CAPS_LOCK, 0),
      GetRewrittenEventAsString(
          &rewriter, ui::ET_KEY_PRESSED, ui::VKEY_LWIN, ui::DomCode::OS_LEFT,
          ui::EF_COMMAND_DOWN | ui::EF_CAPS_LOCK_DOWN, ui::DomKey::OS, 0));
  // Confirm that the Caps Lock status is changed.
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Release Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK, ui::EF_NONE,
                                      ui::DomKey::CAPS_LOCK, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_LWIN, ui::DomCode::OS_LEFT,
                                      ui::EF_NONE, ui::DomKey::OS, 0));
  // Confirm that the Caps Lock status is not changed.
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Press Caps Lock (on an external keyboard).
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK,
                                      ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                      ui::DomKey::CAPS_LOCK, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                                      ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                      ui::DomKey::CAPS_LOCK, 0));

  // Confirm that calling RewriteForTesting() does not change the state of
  // |ime_keyboard|. In this case, X Window system itself should change the
  // Caps Lock state, not ash::EventRewriter.
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Release Caps Lock (on an external keyboard).
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK, ui::EF_NONE,
                                      ui::DomKey::CAPS_LOCK, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_CAPITAL, ui::DomCode::CAPS_LOCK,
                                      ui::EF_NONE, ui::DomKey::CAPS_LOCK, 0));
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);
}

TEST_F(EventRewriterTest, TestRewriteCapsLock) {
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());

  chromeos::input_method::FakeImeKeyboard ime_keyboard;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.set_ime_keyboard_for_testing(&ime_keyboard);
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // On Chrome OS, CapsLock is mapped to F16 with Mod3Mask.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK,
                                      ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                      ui::DomKey::CAPS_LOCK, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_F16, ui::DomCode::F16,
                                      ui::EF_MOD3_DOWN, ui::DomKey::F16, 0));
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);
}

TEST_F(EventRewriterTest, TestRewriteDiamondKey) {
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());

  chromeos::input_method::FakeImeKeyboard ime_keyboard;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.set_ime_keyboard_for_testing(&ime_keyboard);

  KeyTestCase tests[] = {
      // F15 should work as Ctrl when --has-chromeos-diamond-key is not
      // specified.
      {KeyTestCase::TEST_VKEY,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F15, ui::DomCode::F15, ui::EF_NONE, ui::DomKey::F15, 0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CONTROL,
        0}},

      {KeyTestCase::TEST_VKEY,
       ui::ET_KEY_RELEASED,
       {ui::VKEY_F15, ui::DomCode::F15, ui::EF_NONE, ui::DomKey::F15, 0},
       {ui::VKEY_CONTROL,
        ui::DomCode::CONTROL_LEFT,
        ui::EF_NONE,
        ui::DomKey::CONTROL,
        0}},

      // However, Mod2Mask should not be rewritten to CtrlMask when
      // --has-chromeos-diamond-key is not specified.
      {KeyTestCase::TEST_VKEY,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        'a'}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteDiamondKeyWithFlag) {
  const base::CommandLine original_cl(*base::CommandLine::ForCurrentProcess());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kHasChromeOSDiamondKey, "");

  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());

  chromeos::input_method::FakeImeKeyboard ime_keyboard;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.set_ime_keyboard_for_testing(&ime_keyboard);

  // By default, F15 should work as Control.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                ui::EF_CONTROL_DOWN, ui::DomKey::CONTROL, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15, 0));
  // Check that Control is applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_CONTROL_DOWN,
                                      ui::DomKey::CHARACTER, 'a'),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'));
  // Release F15
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL,
                                      ui::DomCode::CONTROL_LEFT, ui::EF_NONE,
                                      ui::DomKey::CONTROL, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15, 0));
  // Check that Control is no longer applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'));

  IntegerPrefMember diamond;
  diamond.Init(prefs::kLanguageRemapDiamondKeyTo, &prefs);
  diamond.SetValue(chromeos::input_method::kVoidKey);

  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN,
                                      ui::DomCode::NONE, ui::EF_NONE,
                                      ui::DomKey::NONE, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15, 0));
  // Check that no modifier is applied to another key.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'));

  diamond.SetValue(chromeos::input_method::kControlKey);

  EXPECT_EQ(GetExpectedResultAsString(
                ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                ui::EF_CONTROL_DOWN, ui::DomKey::CONTROL, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15, 0));
  // Check that Control is applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_CONTROL_DOWN,
                                      ui::DomKey::CHARACTER, 'a'),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'));
  // Release F15
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL,
                                      ui::DomCode::CONTROL_LEFT, ui::EF_NONE,
                                      ui::DomKey::CONTROL, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15, 0));
  // Check that Control is no longer applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'));

  diamond.SetValue(chromeos::input_method::kAltKey);

  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_MENU,
                                      ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN,
                                      ui::DomKey::ALT, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15, 0));
  // Check that Alt is applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_ALT_DOWN,
                                      ui::DomKey::CHARACTER, 'a'),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'));
  // Release F15
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_MENU,
                                      ui::DomCode::ALT_LEFT, ui::EF_NONE,
                                      ui::DomKey::ALT, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15, 0));
  // Check that Alt is no longer applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'));

  diamond.SetValue(chromeos::input_method::kCapsLockKey);

  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK,
                                      ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                      ui::DomKey::CAPS_LOCK, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15, 0));
  // Check that Caps is applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A,
                                      ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                      ui::DomKey::CHARACTER, 'a'),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'));
  // Release F15
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_RELEASED, ui::VKEY_CAPITAL,
                                      ui::DomCode::CAPS_LOCK, ui::EF_NONE,
                                      ui::DomKey::CAPS_LOCK, 0),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_RELEASED,
                                      ui::VKEY_F15, ui::DomCode::F15,
                                      ui::EF_NONE, ui::DomKey::F15, 0));
  // Check that Control is no longer applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'));

  *base::CommandLine::ForCurrentProcess() = original_cl;
}

TEST_F(EventRewriterTest, TestRewriteCapsLockToControl) {
  // Remap CapsLock to Control.
  TestingPrefServiceSyncable prefs;
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
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_MOD3_DOWN,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CHARACTER,
        'a'}},

      // Press Control+CapsLock+a. Confirm that Mod3Mask is rewritten to
      // ControlMask
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_CONTROL_DOWN | ui::EF_MOD3_DOWN,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::CHARACTER,
        'a'}},

      // Press Alt+CapsLock+a. Confirm that Mod3Mask is rewritten to
      // ControlMask.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_ALT_DOWN | ui::EF_MOD3_DOWN,
        ui::DomKey::CHARACTER,
        'a'},
       {ui::VKEY_A,
        ui::DomCode::KEY_A,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::CHARACTER,
        'a'}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteCapsLockMod3InUse) {
  // Remap CapsLock to Control.
  TestingPrefServiceSyncable prefs;
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
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'),
            GetRewrittenEventAsString(&rewriter, ui::ET_KEY_PRESSED, ui::VKEY_A,
                                      ui::DomCode::KEY_A, ui::EF_NONE,
                                      ui::DomKey::CHARACTER, 'a'));

  input_method_manager_mock_->set_mod3_used(false);
}

TEST_F(EventRewriterTest, TestRewriteExtendedKeys) {
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // Alt+Backspace -> Delete
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK,
        ui::DomCode::BACKSPACE,
        ui::EF_ALT_DOWN,
        ui::DomKey::BACKSPACE,
        0},
       {ui::VKEY_DELETE, ui::DomCode::DEL, ui::EF_NONE, ui::DomKey::DEL, 0}},
      // Control+Alt+Backspace -> Control+Delete
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK,
        ui::DomCode::BACKSPACE,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::BACKSPACE,
        0},
       {ui::VKEY_DELETE,
        ui::DomCode::DEL,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::DEL,
        0}},
      // Search+Alt+Backspace -> Alt+Backspace
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK,
        ui::DomCode::BACKSPACE,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::BACKSPACE,
        0},
       {ui::VKEY_BACK,
        ui::DomCode::BACKSPACE,
        ui::EF_ALT_DOWN,
        ui::DomKey::BACKSPACE,
        0}},
      // Search+Control+Alt+Backspace -> Control+Alt+Backspace
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK,
        ui::DomCode::BACKSPACE,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::BACKSPACE,
        0},
       {ui::VKEY_BACK,
        ui::DomCode::BACKSPACE,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::BACKSPACE,
        0}},
      // Alt+Up -> Prior
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_UP,
        ui::DomCode::ARROW_UP,
        ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_UP,
        0},
       {ui::VKEY_PRIOR,
        ui::DomCode::PAGE_UP,
        ui::EF_NONE,
        ui::DomKey::PAGE_UP,
        0}},
      // Alt+Down -> Next
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN,
        ui::DomCode::ARROW_DOWN,
        ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_DOWN,
        0},
       {ui::VKEY_NEXT,
        ui::DomCode::PAGE_DOWN,
        ui::EF_NONE,
        ui::DomKey::PAGE_DOWN,
        0}},
      // Ctrl+Alt+Up -> Home
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_UP,
        ui::DomCode::ARROW_UP,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::ARROW_UP,
        0},
       {ui::VKEY_HOME, ui::DomCode::HOME, ui::EF_NONE, ui::DomKey::HOME, 0}},
      // Ctrl+Alt+Down -> End
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN,
        ui::DomCode::ARROW_DOWN,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::ARROW_DOWN,
        0},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_NONE, ui::DomKey::END, 0}},

      // Search+Alt+Up -> Alt+Up
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_UP,
        ui::DomCode::ARROW_UP,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_UP,
        0},
       {ui::VKEY_UP,
        ui::DomCode::ARROW_UP,
        ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_UP,
        0}},
      // Search+Alt+Down -> Alt+Down
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN,
        ui::DomCode::ARROW_DOWN,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_DOWN,
        0},
       {ui::VKEY_DOWN,
        ui::DomCode::ARROW_DOWN,
        ui::EF_ALT_DOWN,
        ui::DomKey::ARROW_DOWN,
        0}},
      // Search+Ctrl+Alt+Up -> Search+Ctrl+Alt+Up
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_UP,
        ui::DomCode::ARROW_UP,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::ARROW_UP,
        0},
       {ui::VKEY_UP,
        ui::DomCode::ARROW_UP,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::ARROW_UP,
        0}},
      // Search+Ctrl+Alt+Down -> Ctrl+Alt+Down
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN,
        ui::DomCode::ARROW_DOWN,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::ARROW_DOWN,
        0},
       {ui::VKEY_DOWN,
        ui::DomCode::ARROW_DOWN,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::ARROW_DOWN,
        0}},

      // Period -> Period
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PERIOD,
        ui::DomCode::PERIOD,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '.'},
       {ui::VKEY_OEM_PERIOD,
        ui::DomCode::PERIOD,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '.'}},

      // Search+Backspace -> Delete
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK,
        ui::DomCode::BACKSPACE,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::BACKSPACE,
        0},
       {ui::VKEY_DELETE, ui::DomCode::DEL, ui::EF_NONE, ui::DomKey::DEL, 0}},
      // Search+Up -> Prior
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_UP,
        ui::DomCode::ARROW_UP,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_UP,
        0},
       {ui::VKEY_PRIOR,
        ui::DomCode::PAGE_UP,
        ui::EF_NONE,
        ui::DomKey::PAGE_UP,
        0}},
      // Search+Down -> Next
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN,
        ui::DomCode::ARROW_DOWN,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_DOWN,
        0},
       {ui::VKEY_NEXT,
        ui::DomCode::PAGE_DOWN,
        ui::EF_NONE,
        ui::DomKey::PAGE_DOWN,
        0}},
      // Search+Left -> Home
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LEFT,
        ui::DomCode::ARROW_LEFT,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_LEFT,
        0},
       {ui::VKEY_HOME, ui::DomCode::HOME, ui::EF_NONE, ui::DomKey::HOME, 0}},
      // Control+Search+Left -> Home
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_LEFT,
        ui::DomCode::ARROW_LEFT,
        ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::ARROW_LEFT,
        0},
       {ui::VKEY_HOME,
        ui::DomCode::HOME,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::HOME,
        0}},
      // Search+Right -> End
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_RIGHT,
        ui::DomCode::ARROW_RIGHT,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_RIGHT,
        0},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_NONE, ui::DomKey::END, 0}},
      // Control+Search+Right -> End
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_RIGHT,
        ui::DomCode::ARROW_RIGHT,
        ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::ARROW_RIGHT,
        0},
       {ui::VKEY_END,
        ui::DomCode::END,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::END,
        0}},
      // Search+Period -> Insert
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PERIOD,
        ui::DomCode::PERIOD,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '.'},
       {ui::VKEY_INSERT,
        ui::DomCode::INSERT,
        ui::EF_NONE,
        ui::DomKey::INSERT,
        0}},
      // Control+Search+Period -> Control+Insert
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PERIOD,
        ui::DomCode::PERIOD,
        ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
        ui::DomKey::CHARACTER,
        '.'},
       {ui::VKEY_INSERT,
        ui::DomCode::INSERT,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::INSERT,
        0}}};

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeys) {
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // F1 -> Back
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1, 0},
       {ui::VKEY_BROWSER_BACK,
        ui::DomCode::BROWSER_BACK,
        ui::EF_NONE,
        ui::DomKey::BROWSER_BACK,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_CONTROL_DOWN, ui::DomKey::F1, 0},
       {ui::VKEY_BROWSER_BACK,
        ui::DomCode::BROWSER_BACK,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::BROWSER_BACK,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_ALT_DOWN, ui::DomKey::F1, 0},
       {ui::VKEY_BROWSER_BACK,
        ui::DomCode::BROWSER_BACK,
        ui::EF_ALT_DOWN,
        ui::DomKey::BROWSER_BACK,
        0}},
      // F2 -> Forward
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2, 0},
       {ui::VKEY_BROWSER_FORWARD,
        ui::DomCode::BROWSER_FORWARD,
        ui::EF_NONE,
        ui::DomKey::BROWSER_FORWARD,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_CONTROL_DOWN, ui::DomKey::F2, 0},
       {ui::VKEY_BROWSER_FORWARD,
        ui::DomCode::BROWSER_FORWARD,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::BROWSER_FORWARD,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_ALT_DOWN, ui::DomKey::F2, 0},
       {ui::VKEY_BROWSER_FORWARD,
        ui::DomCode::BROWSER_FORWARD,
        ui::EF_ALT_DOWN,
        ui::DomKey::BROWSER_FORWARD,
        0}},
      // F3 -> Refresh
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3, 0},
       {ui::VKEY_BROWSER_REFRESH,
        ui::DomCode::BROWSER_REFRESH,
        ui::EF_NONE,
        ui::DomKey::BROWSER_REFRESH,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_CONTROL_DOWN, ui::DomKey::F3, 0},
       {ui::VKEY_BROWSER_REFRESH,
        ui::DomCode::BROWSER_REFRESH,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::BROWSER_REFRESH,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_ALT_DOWN, ui::DomKey::F3, 0},
       {ui::VKEY_BROWSER_REFRESH,
        ui::DomCode::BROWSER_REFRESH,
        ui::EF_ALT_DOWN,
        ui::DomKey::BROWSER_REFRESH,
        0}},
      // F4 -> Launch App 2
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4, 0},
       {ui::VKEY_MEDIA_LAUNCH_APP2,
        ui::DomCode::ZOOM_TOGGLE,
        ui::EF_NONE,
        ui::DomKey::ZOOM_TOGGLE,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_CONTROL_DOWN, ui::DomKey::F4, 0},
       {ui::VKEY_MEDIA_LAUNCH_APP2,
        ui::DomCode::ZOOM_TOGGLE,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::ZOOM_TOGGLE,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_ALT_DOWN, ui::DomKey::F4, 0},
       {ui::VKEY_MEDIA_LAUNCH_APP2,
        ui::DomCode::ZOOM_TOGGLE,
        ui::EF_ALT_DOWN,
        ui::DomKey::ZOOM_TOGGLE,
        0}},
      // F5 -> Launch App 1
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5, 0},
       {ui::VKEY_MEDIA_LAUNCH_APP1,
        ui::DomCode::SELECT_TASK,
        ui::EF_NONE,
        ui::DomKey::LAUNCH_MY_COMPUTER,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_CONTROL_DOWN, ui::DomKey::F5, 0},
       {ui::VKEY_MEDIA_LAUNCH_APP1,
        ui::DomCode::SELECT_TASK,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::LAUNCH_MY_COMPUTER,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_ALT_DOWN, ui::DomKey::F5, 0},
       {ui::VKEY_MEDIA_LAUNCH_APP1,
        ui::DomCode::SELECT_TASK,
        ui::EF_ALT_DOWN,
        ui::DomKey::LAUNCH_MY_COMPUTER,
        0}},
      // F6 -> Brightness down
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6, 0},
       {ui::VKEY_BRIGHTNESS_DOWN,
        ui::DomCode::BRIGHTNESS_DOWN,
        ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_DOWN,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_CONTROL_DOWN, ui::DomKey::F6, 0},
       {ui::VKEY_BRIGHTNESS_DOWN,
        ui::DomCode::BRIGHTNESS_DOWN,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::BRIGHTNESS_DOWN,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_ALT_DOWN, ui::DomKey::F6, 0},
       {ui::VKEY_BRIGHTNESS_DOWN,
        ui::DomCode::BRIGHTNESS_DOWN,
        ui::EF_ALT_DOWN,
        ui::DomKey::BRIGHTNESS_DOWN,
        0}},
      // F7 -> Brightness up
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7, 0},
       {ui::VKEY_BRIGHTNESS_UP,
        ui::DomCode::BRIGHTNESS_UP,
        ui::EF_NONE,
        ui::DomKey::BRIGHTNESS_UP,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_CONTROL_DOWN, ui::DomKey::F7, 0},
       {ui::VKEY_BRIGHTNESS_UP,
        ui::DomCode::BRIGHTNESS_UP,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::BRIGHTNESS_UP,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_ALT_DOWN, ui::DomKey::F7, 0},
       {ui::VKEY_BRIGHTNESS_UP,
        ui::DomCode::BRIGHTNESS_UP,
        ui::EF_ALT_DOWN,
        ui::DomKey::BRIGHTNESS_UP,
        0}},
      // F8 -> Volume Mute
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8, 0},
       {ui::VKEY_VOLUME_MUTE,
        ui::DomCode::VOLUME_MUTE,
        ui::EF_NONE,
        ui::DomKey::VOLUME_MUTE,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_CONTROL_DOWN, ui::DomKey::F8, 0},
       {ui::VKEY_VOLUME_MUTE,
        ui::DomCode::VOLUME_MUTE,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::VOLUME_MUTE,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_ALT_DOWN, ui::DomKey::F8, 0},
       {ui::VKEY_VOLUME_MUTE,
        ui::DomCode::VOLUME_MUTE,
        ui::EF_ALT_DOWN,
        ui::DomKey::VOLUME_MUTE,
        0}},
      // F9 -> Volume Down
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9, 0},
       {ui::VKEY_VOLUME_DOWN,
        ui::DomCode::VOLUME_DOWN,
        ui::EF_NONE,
        ui::DomKey::VOLUME_DOWN,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_CONTROL_DOWN, ui::DomKey::F9, 0},
       {ui::VKEY_VOLUME_DOWN,
        ui::DomCode::VOLUME_DOWN,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::VOLUME_DOWN,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_ALT_DOWN, ui::DomKey::F9, 0},
       {ui::VKEY_VOLUME_DOWN,
        ui::DomCode::VOLUME_DOWN,
        ui::EF_ALT_DOWN,
        ui::DomKey::VOLUME_DOWN,
        0}},
      // F10 -> Volume Up
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10, 0},
       {ui::VKEY_VOLUME_UP,
        ui::DomCode::VOLUME_UP,
        ui::EF_NONE,
        ui::DomKey::VOLUME_UP,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F10,
        ui::DomCode::F10,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::F10,
        0},
       {ui::VKEY_VOLUME_UP,
        ui::DomCode::VOLUME_UP,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::VOLUME_UP,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_ALT_DOWN, ui::DomKey::F10, 0},
       {ui::VKEY_VOLUME_UP,
        ui::DomCode::VOLUME_UP,
        ui::EF_ALT_DOWN,
        ui::DomKey::VOLUME_UP,
        0}},
      // F11 -> F11
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11, 0},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F11,
        ui::DomCode::F11,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::F11,
        0},
       {ui::VKEY_F11,
        ui::DomCode::F11,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::F11,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_ALT_DOWN, ui::DomKey::F11, 0},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_ALT_DOWN, ui::DomKey::F11, 0}},
      // F12 -> F12
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12, 0},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F12,
        ui::DomCode::F12,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::F12,
        0},
       {ui::VKEY_F12,
        ui::DomCode::F12,
        ui::EF_CONTROL_DOWN,
        ui::DomKey::F12,
        0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_ALT_DOWN, ui::DomKey::F12, 0},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_ALT_DOWN, ui::DomKey::F12, 0}},

      // The number row should not be rewritten without Search key.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_1,
        ui::DomCode::DIGIT1,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '1'},
       {ui::VKEY_1,
        ui::DomCode::DIGIT1,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '1'}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_2,
        ui::DomCode::DIGIT2,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '2'},
       {ui::VKEY_2,
        ui::DomCode::DIGIT2,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '2'}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_3,
        ui::DomCode::DIGIT3,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '3'},
       {ui::VKEY_3,
        ui::DomCode::DIGIT3,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '3'}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_4,
        ui::DomCode::DIGIT4,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '4'},
       {ui::VKEY_4,
        ui::DomCode::DIGIT4,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '4'}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_5,
        ui::DomCode::DIGIT5,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '5'},
       {ui::VKEY_5,
        ui::DomCode::DIGIT5,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '5'}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_6,
        ui::DomCode::DIGIT6,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '6'},
       {ui::VKEY_6,
        ui::DomCode::DIGIT6,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '6'}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_7,
        ui::DomCode::DIGIT7,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '7'},
       {ui::VKEY_7,
        ui::DomCode::DIGIT7,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '7'}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_8,
        ui::DomCode::DIGIT8,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '8'},
       {ui::VKEY_8,
        ui::DomCode::DIGIT8,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '8'}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_9,
        ui::DomCode::DIGIT9,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '9'},
       {ui::VKEY_9,
        ui::DomCode::DIGIT9,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '9'}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_0,
        ui::DomCode::DIGIT0,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '0'},
       {ui::VKEY_0,
        ui::DomCode::DIGIT0,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '0'}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_MINUS,
        ui::DomCode::MINUS,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '-'},
       {ui::VKEY_OEM_MINUS,
        ui::DomCode::MINUS,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '-'}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PLUS,
        ui::DomCode::EQUAL,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '='},
       {ui::VKEY_OEM_PLUS,
        ui::DomCode::EQUAL,
        ui::EF_NONE,
        ui::DomKey::CHARACTER,
        '='}},

      // The number row should be rewritten as the F<number> row with Search
      // key.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_1,
        ui::DomCode::DIGIT1,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '1'},
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_2,
        ui::DomCode::DIGIT2,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '2'},
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_3,
        ui::DomCode::DIGIT3,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '3'},
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_4,
        ui::DomCode::DIGIT4,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '4'},
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_5,
        ui::DomCode::DIGIT5,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '5'},
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_6,
        ui::DomCode::DIGIT6,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '6'},
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_7,
        ui::DomCode::DIGIT7,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '7'},
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_8,
        ui::DomCode::DIGIT8,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '8'},
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_9,
        ui::DomCode::DIGIT9,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '9'},
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_0,
        ui::DomCode::DIGIT0,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '0'},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_MINUS,
        ui::DomCode::MINUS,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '-'},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PLUS,
        ui::DomCode::EQUAL,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::CHARACTER,
        '='},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12, 0}},

      // The function keys should not be rewritten with Search key pressed.
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_COMMAND_DOWN, ui::DomKey::F1, 0},
       {ui::VKEY_F1, ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_COMMAND_DOWN, ui::DomKey::F2, 0},
       {ui::VKEY_F2, ui::DomCode::F2, ui::EF_NONE, ui::DomKey::F2, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_COMMAND_DOWN, ui::DomKey::F3, 0},
       {ui::VKEY_F3, ui::DomCode::F3, ui::EF_NONE, ui::DomKey::F3, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_COMMAND_DOWN, ui::DomKey::F4, 0},
       {ui::VKEY_F4, ui::DomCode::F4, ui::EF_NONE, ui::DomKey::F4, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_COMMAND_DOWN, ui::DomKey::F5, 0},
       {ui::VKEY_F5, ui::DomCode::F5, ui::EF_NONE, ui::DomKey::F5, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_COMMAND_DOWN, ui::DomKey::F6, 0},
       {ui::VKEY_F6, ui::DomCode::F6, ui::EF_NONE, ui::DomKey::F6, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_COMMAND_DOWN, ui::DomKey::F7, 0},
       {ui::VKEY_F7, ui::DomCode::F7, ui::EF_NONE, ui::DomKey::F7, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_COMMAND_DOWN, ui::DomKey::F8, 0},
       {ui::VKEY_F8, ui::DomCode::F8, ui::EF_NONE, ui::DomKey::F8, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_COMMAND_DOWN, ui::DomKey::F9, 0},
       {ui::VKEY_F9, ui::DomCode::F9, ui::EF_NONE, ui::DomKey::F9, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F10,
        ui::DomCode::F10,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::F10,
        0},
       {ui::VKEY_F10, ui::DomCode::F10, ui::EF_NONE, ui::DomKey::F10, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F11,
        ui::DomCode::F11,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::F11,
        0},
       {ui::VKEY_F11, ui::DomCode::F11, ui::EF_NONE, ui::DomKey::F11, 0}},
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F12,
        ui::DomCode::F12,
        ui::EF_COMMAND_DOWN,
        ui::DomKey::F12,
        0},
       {ui::VKEY_F12, ui::DomCode::F12, ui::EF_NONE, ui::DomKey::F12, 0}}};

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteExtendedKeysWithSearchRemapped) {
  const base::CommandLine original_cl(*base::CommandLine::ForCurrentProcess());

  // Remap Search to Control.
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kControlKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kHasChromeOSKeyboard, "");

  KeyTestCase tests[] = {
      // Alt+Search+Down -> End
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN,
        ui::DomCode::ARROW_DOWN,
        ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_DOWN,
        0},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_NONE, ui::DomKey::END, 0}},

      // Shift+Alt+Search+Down -> Shift+End
      {KeyTestCase::TEST_ALL,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN,
        ui::DomCode::ARROW_DOWN,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
        ui::DomKey::ARROW_DOWN,
        0},
       {ui::VKEY_END, ui::DomCode::END, ui::EF_SHIFT_DOWN, ui::DomKey::END, 0}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
  }

  *base::CommandLine::ForCurrentProcess() = original_cl;
}

TEST_F(EventRewriterTest, TestRewriteKeyEventSentByXSendEvent) {
  // Remap Control to Alt.
  TestingPrefServiceSyncable prefs;
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
                          ui::DomKey::CONTROL, 0, ui::EventTimeForNow());
    scoped_ptr<ui::Event> new_event;
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
    scoped_ptr<ui::Event> new_event;
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
  TestingPrefServiceSyncable prefs;
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

  scoped_ptr<ui::Event> new_event;
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
    if (event->IsKeyEvent()) {
      events_.push_back(new ui::KeyEvent(*static_cast<ui::KeyEvent*>(event)));
    } else if (event->IsMouseWheelEvent()) {
      events_.push_back(
          new ui::MouseWheelEvent(*static_cast<ui::MouseWheelEvent*>(event)));
    } else if (event->IsMouseEvent()) {
      events_.push_back(
          new ui::MouseEvent(*static_cast<ui::MouseEvent*>(event)));
    }
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
                           scoped_ptr<ui::Event>* rewritten_event) {
    return rewriter_->RewriteEvent(event, rewritten_event);
  }

  ui::EventDispatchDetails Send(ui::Event* event) {
    return source_.Send(event);
  }

  void SendKeyEvent(ui::EventType type,
                    ui::KeyboardCode key_code,
                    ui::DomCode code,
                    ui::DomKey key,
                    base::char16 character) {
    ui::KeyEvent press(type, key_code, code, ui::EF_NONE, key, character,
                       ui::EventTimeForNow());
    ui::EventDispatchDetails details = Send(&press);
    CHECK(!details.dispatcher_destroyed);
  }

  void SendActivateStickyKeyPattern(ui::KeyboardCode key_code,
                                    ui::DomCode code,
                                    ui::DomKey key,
                                    base::char16 character) {
    SendKeyEvent(ui::ET_KEY_PRESSED, key_code, code, key, character);
    SendKeyEvent(ui::ET_KEY_RELEASED, key_code, code, key, character);
  }

 protected:
  TestingPrefServiceSyncable* prefs() { return &prefs_; }

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
  scoped_ptr<EventRewriter> rewriter_;

  EventBuffer buffer_;
  TestEventSource source_;

  user_manager::FakeUserManager* fake_user_manager_;  // Not owned.
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  TestingPrefServiceSyncable prefs_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriterAshTest);
};

TEST_F(EventRewriterAshTest, TopRowKeysAreFunctionKeys) {
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(1));
  ash::wm::WindowState* window_state = ash::wm::GetWindowState(window.get());
  window_state->Activate();
  ScopedVector<ui::Event> events;

  // Create a simulated keypress of F1 targetted at the window.
  ui::KeyEvent press_f1(ui::ET_KEY_PRESSED, ui::VKEY_F1, ui::DomCode::F1,
                        ui::EF_NONE, ui::DomKey::F1, 0, ui::EventTimeForNow());

  // Simulate an apps v2 window that has requested top row keys as function
  // keys. The event should not be rewritten.
  window_state->set_top_row_keys_are_function_keys(true);
  ui::EventDispatchDetails details = Send(&press_f1);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_F1,
                                      ui::DomCode::F1, ui::EF_NONE,
                                      ui::DomKey::F1, 0),
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
  EXPECT_EQ(GetExpectedResultAsString(ui::ET_KEY_PRESSED, ui::VKEY_F1,
                                      ui::DomCode::F1, ui::EF_NONE,
                                      ui::DomKey::F1, 0),
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
                                      ui::DomKey::BROWSER_BACK, 0),
            GetKeyEventAsString(*static_cast<ui::KeyEvent*>(events[0])));
}

TEST_F(EventRewriterTest, TestRewrittenModifierClick) {
#if defined(USE_X11)
  std::vector<int> device_list;
  device_list.push_back(10);
  ui::TouchFactory::GetInstance()->SetPointerDeviceForTest(device_list);

  // Remap Control to Search.
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapControlKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kSearchKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  // Check that Control + Left Button is converted (via Search + Left Button)
  // to Right Button.
  ui::ScopedXI2Event xev;
  xev.InitGenericButtonEvent(10, ui::ET_MOUSE_PRESSED, gfx::Point(),
                             ui::EF_LEFT_MOUSE_BUTTON | ui::EF_CONTROL_DOWN);
  ui::MouseEvent press(xev);
  // Sanity check.
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_CONTROL_DOWN, press.flags());
  scoped_ptr<ui::Event> new_event;
  const ui::MouseEvent* result =
      RewriteMouseButtonEvent(&rewriter, press, &new_event);
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
  EXPECT_FALSE(ui::EF_LEFT_MOUSE_BUTTON & result->flags());
  EXPECT_FALSE(ui::EF_CONTROL_DOWN & result->flags());
  EXPECT_FALSE(ui::EF_COMMAND_DOWN & result->flags());
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
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.set_pref_service_for_testing(&prefs);
  const int kLeftAndSearchFlag = ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN;

  // Test Search + Left click.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), kLeftAndSearchFlag,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(10);
    // Sanity check.
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(kLeftAndSearchFlag, press.flags());
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndSearchFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), kLeftAndSearchFlag,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(10);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndSearchFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
#if defined(USE_X11)
  // Test Search + Left click, using XI2 native events.
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(10, ui::ET_MOUSE_PRESSED, gfx::Point(),
                               kLeftAndSearchFlag);
    ui::MouseEvent press(xev);
    // Sanity check.
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(kLeftAndSearchFlag, press.flags());
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndSearchFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(10, ui::ET_MOUSE_RELEASED, gfx::Point(),
                               kLeftAndSearchFlag);
    ui::MouseEvent release(xev);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndSearchFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
#endif

  // No SEARCH in first click.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(10);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), kLeftAndSearchFlag,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(10);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN, result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
#if defined(USE_X11)
  // No SEARCH in first click, using XI2 native events.
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(10, ui::ET_MOUSE_PRESSED, gfx::Point(),
                               ui::EF_LEFT_MOUSE_BUTTON);
    ui::MouseEvent press(xev);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(10, ui::ET_MOUSE_RELEASED, gfx::Point(),
                               kLeftAndSearchFlag);
    ui::MouseEvent release(xev);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN, result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
#endif

  // SEARCH on different device.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), kLeftAndSearchFlag,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(11);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndSearchFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), kLeftAndSearchFlag,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(10);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN, result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), kLeftAndSearchFlag,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(11);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndSearchFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
#if defined(USE_X11)
  // SEARCH on different device, using XI2 native events.
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(11, ui::ET_MOUSE_PRESSED, gfx::Point(),
                               kLeftAndSearchFlag);
    ui::MouseEvent press(xev);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndSearchFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(10, ui::ET_MOUSE_RELEASED, gfx::Point(),
                               kLeftAndSearchFlag);
    ui::MouseEvent release(xev);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN, result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(11, ui::ET_MOUSE_RELEASED, gfx::Point(),
                               kLeftAndSearchFlag);
    ui::MouseEvent release(xev);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndSearchFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
#endif
}

TEST_F(EventRewriterAshTest, StickyKeyEventDispatchImpl) {
  // Test the actual key event dispatch implementation.
  ScopedVector<ui::Event> events;

  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL, 0);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_KEY_PRESSED, events[0]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[0])->key_code());

  // Test key press event is correctly modified and modifier release
  // event is sent.
  ui::KeyEvent press(ui::ET_KEY_PRESSED, ui::VKEY_C, ui::DomCode::KEY_C,
                     ui::EF_NONE, ui::DomKey::CHARACTER, 'c',
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
  ui::KeyEvent release(ui::ET_KEY_RELEASED, ui::VKEY_C, ui::DomCode::KEY_C,
                       ui::EF_NONE, ui::DomKey::CHARACTER, 'c',
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
                               ui::DomKey::CONTROL, 0);
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
                               ui::DomKey::CONTROL, 0);
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
                               ui::DomKey::CONTROL, 0);
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
                               ui::DomKey::CONTROL, 0);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_T, ui::DomCode::KEY_T,
                               ui::DomKey::CHARACTER, 't');
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
                               ui::DomKey::SHIFT, 0);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL, 0);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_N, ui::DomCode::KEY_N,
                               ui::DomKey::CHARACTER, 'n');
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
                               ui::DomKey::ALT, 0);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT, 0);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Pressing a normal key should not hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_D, ui::DomCode::KEY_D,
                               ui::DomKey::CHARACTER, 'd');
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
                               ui::DomKey::CONTROL, 0);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL, 0);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing another modifier key should still show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT, 0);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing a normal key should not hide overlay but disable normal modifier.
  SendActivateStickyKeyPattern(ui::VKEY_D, ui::DomCode::KEY_D,
                               ui::DomKey::CHARACTER, 'd');
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
                               ui::DomKey::CONTROL, 0);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT, 0);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT, 0);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT, 0);
  SendActivateStickyKeyPattern(ui::VKEY_COMMAND, ui::DomCode::OS_LEFT,
                               ui::DomKey::OS, 0);
  SendActivateStickyKeyPattern(ui::VKEY_COMMAND, ui::DomCode::OS_LEFT,
                               ui::DomKey::OS, 0);

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
                               ui::DomKey::CONTROL, 0);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL, 0);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT, 0);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT, 0);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT, 0);
  SendActivateStickyKeyPattern(ui::VKEY_COMMAND, ui::DomCode::OS_LEFT,
                               ui::DomKey::OS, 0);

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
