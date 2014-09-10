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
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/ime/fake_ime_keyboard.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/test_event_processor.h"

#if defined(USE_X11)
#include <X11/keysym.h>

#include "ui/events/event_utils.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/events/x/touch_factory_x11.h"
#include "ui/gfx/x/x11_types.h"
#endif

namespace {

// The device id of the test touchpad device.
const unsigned int kTouchPadDeviceId = 1;
const int kKeyboardDeviceId = 2;

std::string GetExpectedResultAsString(ui::KeyboardCode ui_keycode,
                                      int ui_flags,
                                      ui::EventType ui_type) {
  return base::StringPrintf("ui_keycode=0x%X ui_flags=0x%X ui_type=%d",
                            ui_keycode,
                            ui_flags & ~ui::EF_IS_REPEAT,
                            ui_type);
}

std::string GetKeyEventAsString(const ui::KeyEvent& keyevent) {
  return GetExpectedResultAsString(
      keyevent.key_code(), keyevent.flags(), keyevent.type());
}

std::string GetRewrittenEventAsString(chromeos::EventRewriter* rewriter,
                                      ui::KeyboardCode ui_keycode,
                                      int ui_flags,
                                      ui::EventType ui_type) {
  const ui::KeyEvent event(ui_type, ui_keycode, ui_flags);
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
    TEST_X11  = 1 << 1,  // Test ui::KeyEvent with native XKeyEvent
    TEST_ALL  = TEST_VKEY|TEST_X11,
    // Special test flags:
    NUMPAD    = 1 << 8,  // Set EF_NUMPAD_KEY on native-based event, because
                         // |XKeysymForWindowsKeyCode()| can not distinguish
                         // between pairs like XK_Insert and XK_KP_Insert.
  };
  int test;
  ui::EventType type;
  struct {
    ui::KeyboardCode key_code;
    int flags;
  } input, expected;
};

#if defined(USE_X11)
// Check rewriting of an X11-based key event.
void CheckX11KeyTestCase(const std::string& expected,
                         chromeos::EventRewriter* rewriter,
                         const KeyTestCase& test,
                         XEvent* xevent) {
  ui::KeyEvent xkey_event(xevent);
  if (test.test & KeyTestCase::NUMPAD)
    xkey_event.set_flags(xkey_event.flags() | ui::EF_NUMPAD_KEY);
  // Verify that the X11-based key event is as expected.
  EXPECT_EQ(GetExpectedResultAsString(
                     test.input.key_code, test.input.flags, test.type),
            GetKeyEventAsString(xkey_event));
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
// |i| is a an identifying number to locate failing tests in the tables.
void CheckKeyTestCase(chromeos::EventRewriter* rewriter,
                      const KeyTestCase& test) {
  std::string expected =
      GetExpectedResultAsString(
               test.expected.key_code, test.expected.flags, test.type);

  if (test.test & KeyTestCase::TEST_VKEY) {
    // Check rewriting of a non-native-based key event.
    EXPECT_EQ(
        expected,
        GetRewrittenEventAsString(
                 rewriter, test.input.key_code, test.input.flags, test.type));
  }

#if defined(USE_X11)
  if (test.test & KeyTestCase::TEST_X11) {
    ui::ScopedXI2Event xev;
    // Test an XKeyEvent.
    xev.InitKeyEvent(test.type, test.input.key_code, test.input.flags);
    XEvent* xevent = xev;
    DCHECK((xevent->type == KeyPress) || (xevent->type == KeyRelease));
    if (xevent->xkey.keycode)
      CheckX11KeyTestCase(expected, rewriter, test, xevent);
    // Test an XI2 GenericEvent.
    xev.InitGenericKeyEvent(
        kKeyboardDeviceId, test.type, test.input.key_code, test.input.flags);
    xevent = xev;
    DCHECK(xevent->type == GenericEvent);
    XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xevent->xcookie.data);
    DCHECK((xievent->evtype == XI_KeyPress) ||
           (xievent->evtype == XI_KeyRelease));
    if (xievent->detail)
      CheckX11KeyTestCase(expected, rewriter, test, xevent);
  }
#endif
}

// Table entry for simple single function key event rewriting tests.
struct FunctionKeyTestCase {
  ui::EventType type;
  struct {
    ui::KeyboardCode key_code;
    int flags;
  } input, vkey_expected, native_expected;
};

// Tests a single stateless function key rewrite operation.
// |i| is a an identifying number to locate failing tests in the tables.
// Function key mapping differs from the other key mappings because the
// EF_FUNCTION_KEY flag is set during ui::KeyEvent construction when passing in
// a native X11 event and the flag is not set when using other constructors.
void CheckFunctionKeyTestCase(chromeos::EventRewriter* rewriter,
                              const FunctionKeyTestCase& test) {
  std::string vkey_expected =
      GetExpectedResultAsString(
          test.vkey_expected.key_code,
          test.vkey_expected.flags,
          test.type);
  // Check rewriting of a non-native-based key event.
  EXPECT_EQ(
      vkey_expected,
      GetRewrittenEventAsString(
               rewriter, test.input.key_code, test.input.flags, test.type));

#if defined(USE_X11)
  ui::ScopedXI2Event xev;
  xev.InitKeyEvent(test.type, test.input.key_code, test.input.flags);
  XEvent* xevent = xev;
  if (xevent->xkey.keycode) {
    ui::KeyEvent xkey_event(xevent);
    // Rewrite the event and check the result.
    scoped_ptr<ui::Event> new_event;
    rewriter->RewriteEvent(xkey_event, &new_event);
    ui::KeyEvent& rewritten_key_event =
        new_event ? *static_cast<ui::KeyEvent*>(new_event.get()) : xkey_event;
    std::string native_expected =
        GetExpectedResultAsString(
            test.native_expected.key_code,
            test.native_expected.flags,
            test.type);
    EXPECT_EQ(native_expected, GetKeyEventAsString(rewritten_key_event));
  }
#endif
}

}  // namespace

namespace chromeos {

class EventRewriterTest : public ash::test::AshTestBase {
 public:
  EventRewriterTest()
      : mock_user_manager_(new chromeos::MockUserManager),
        user_manager_enabler_(mock_user_manager_),
        input_method_manager_mock_(NULL) {}
  virtual ~EventRewriterTest() {}

  virtual void SetUp() {
    // Mocking user manager because the real one needs to be called on UI thread
    EXPECT_CALL(*mock_user_manager_, IsLoggedInAsGuest())
        .WillRepeatedly(testing::Return(false));
    input_method_manager_mock_ =
        new chromeos::input_method::MockInputMethodManager;
    chromeos::input_method::InitializeForTesting(
        input_method_manager_mock_);  // pass ownership

    AshTestBase::SetUp();
  }

  virtual void TearDown() {
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

  chromeos::MockUserManager* mock_user_manager_;  // Not owned.
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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_ALT_DOWN},
       {ui::VKEY_A, ui::EF_ALT_DOWN}},

      // VKEY_A, Win modifier.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_COMMAND_DOWN},
       {ui::VKEY_A, ui::EF_COMMAND_DOWN}},

      // VKEY_A, Alt+Win modifier.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN},
       {ui::VKEY_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN}},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN},
       {ui::VKEY_LWIN, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN}},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN},
       {ui::VKEY_RWIN, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN}},
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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_ALT_DOWN},
       {ui::VKEY_A, ui::EF_ALT_DOWN}},

      // VKEY_A, Win modifier.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_COMMAND_DOWN},
       {ui::VKEY_A, ui::EF_CONTROL_DOWN}},

      // VKEY_A, Alt+Win modifier.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN},
       {ui::VKEY_A, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN}},

      // VKEY_LWIN (left Windows key), Alt modifier.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}},

      // VKEY_RWIN (right Windows key), Alt modifier.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}},
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

  KeyTestCase pc_keyboard_tests[] = {// Control should be remapped to Alt.
                                     {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
                                      {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN},
                                      {ui::VKEY_MENU, ui::EF_ALT_DOWN}},
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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}},

      // VKEY_RWIN (right Command key) with  Alt modifier. The remapped Command
      // key should never be re-remapped to Alt.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_RWIN, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}},
  };

  for (size_t i = 0; i < arraysize(apple_keyboard_tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, apple_keyboard_tests[i]);
  }
}

void EventRewriterTest::TestRewriteNumPadKeys() {
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.set_last_keyboard_device_id_for_testing(kKeyboardDeviceId);
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // XK_KP_Insert (= NumPad 0 without Num Lock), no modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_INSERT, ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD0, ui::EF_NUMPAD_KEY}},

      // XK_KP_Insert (= NumPad 0 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_INSERT, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD0, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY}},

      // XK_KP_Delete (= NumPad . without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_DELETE, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_DECIMAL, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY}},

      // XK_KP_End (= NumPad 1 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_END, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD1, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY}},

      // XK_KP_Down (= NumPad 2 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD2, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY}},

      // XK_KP_Next (= NumPad 3 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_NEXT, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD3, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY}},

      // XK_KP_Left (= NumPad 4 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_LEFT, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD4, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY}},

      // XK_KP_Begin (= NumPad 5 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_CLEAR, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD5, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY}},

      // XK_KP_Right (= NumPad 6 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_RIGHT, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD6, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY}},

      // XK_KP_Home (= NumPad 7 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_HOME, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD7, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY}},

      // XK_KP_Up (= NumPad 8 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD8, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY}},

      // XK_KP_Prior (= NumPad 9 without Num Lock), Alt modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_PRIOR, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD9, ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY}},

      // XK_KP_0 (= NumPad 0 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD0, ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD0, ui::EF_NUMPAD_KEY}},

      // XK_KP_DECIMAL (= NumPad . with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_DECIMAL, ui::EF_NUMPAD_KEY},
       {ui::VKEY_DECIMAL, ui::EF_NUMPAD_KEY}},

      // XK_KP_1 (= NumPad 1 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD1, ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD1, ui::EF_NUMPAD_KEY}},

      // XK_KP_2 (= NumPad 2 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD2, ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD2, ui::EF_NUMPAD_KEY}},

      // XK_KP_3 (= NumPad 3 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD3, ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD3, ui::EF_NUMPAD_KEY}},

      // XK_KP_4 (= NumPad 4 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD4, ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD4, ui::EF_NUMPAD_KEY}},

      // XK_KP_5 (= NumPad 5 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD5, ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD5, ui::EF_NUMPAD_KEY}},

      // XK_KP_6 (= NumPad 6 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD6, ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD6, ui::EF_NUMPAD_KEY}},

      // XK_KP_7 (= NumPad 7 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD7, ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD7, ui::EF_NUMPAD_KEY}},

      // XK_KP_8 (= NumPad 8 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD8, ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD8, ui::EF_NUMPAD_KEY}},

      // XK_KP_9 (= NumPad 9 with Num Lock), Num Lock modifier.
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD9, ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD9, ui::EF_NUMPAD_KEY}},
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
  const CommandLine original_cl(*CommandLine::ForCurrentProcess());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kHasChromeOSDiamondKey, "");

  TestRewriteNumPadKeys();
  *CommandLine::ForCurrentProcess() = original_cl;
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
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_END, ui::EF_COMMAND_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD1, ui::EF_CONTROL_DOWN | ui::EF_NUMPAD_KEY}},

      // XK_KP_1 (= NumPad 1 with Num Lock), Win modifier.
      // The result should also be "Num Pad 1 with Control + Num Lock
      // modifiers".
      {KeyTestCase::TEST_ALL|KeyTestCase::NUMPAD, ui::ET_KEY_PRESSED,
       {ui::VKEY_NUMPAD1, ui::EF_COMMAND_DOWN | ui::EF_NUMPAD_KEY},
       {ui::VKEY_NUMPAD1, ui::EF_CONTROL_DOWN | ui::EF_NUMPAD_KEY}},
  };

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
  const CommandLine original_cl(*CommandLine::ForCurrentProcess());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kHasChromeOSDiamondKey, "");

  TestRewriteNumPadKeysOnAppleKeyboard();
  *CommandLine::ForCurrentProcess() = original_cl;
}

TEST_F(EventRewriterTest, TestRewriteModifiersNoRemap) {
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // Press Search. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_NONE},
       {ui::VKEY_LWIN, ui::EF_COMMAND_DOWN}},

      // Press left Control. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN}},

      // Press right Control. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN}},

      // Press left Alt. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::EF_ALT_DOWN},
       {ui::VKEY_MENU, ui::EF_ALT_DOWN}},

      // Press right Alt. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::EF_ALT_DOWN},
       {ui::VKEY_MENU, ui::EF_ALT_DOWN}},

      // Test KeyRelease event, just in case.
      // Release Search. Confirm the release event is not rewritten.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_RELEASED,
       {ui::VKEY_LWIN, ui::EF_NONE},
       {ui::VKEY_LWIN, ui::EF_NONE}},
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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN},
       {ui::VKEY_MENU, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN}},

      // Press Search with Caps Lock mask. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_CAPS_LOCK_DOWN | ui::EF_COMMAND_DOWN},
       {ui::VKEY_LWIN, ui::EF_CAPS_LOCK_DOWN | ui::EF_COMMAND_DOWN}},

      // Release Search with Caps Lock mask. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_RELEASED,
       {ui::VKEY_LWIN, ui::EF_CAPS_LOCK_DOWN},
       {ui::VKEY_LWIN, ui::EF_CAPS_LOCK_DOWN}},

      // Press Shift+Ctrl+Alt+Search+A. Confirm the event is not rewritten.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
                        ui::EF_COMMAND_DOWN},
       {ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
                        ui::EF_COMMAND_DOWN}},
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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN},
       {ui::VKEY_MENU, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN}},

      // Press Search. Confirm the event is now VKEY_UNKNOWN.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_NONE},
       {ui::VKEY_UNKNOWN, ui::EF_NONE}},

      // Press Control. Confirm the event is now VKEY_UNKNOWN.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN},
       {ui::VKEY_UNKNOWN, ui::EF_NONE}},

      // Press Control+Search. Confirm the event is now VKEY_UNKNOWN
      // without any modifiers.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_CONTROL_DOWN},
       {ui::VKEY_UNKNOWN, ui::EF_NONE}},

      // Press Control+Search+a. Confirm the event is now VKEY_A without any
      // modifiers.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_CONTROL_DOWN},
       {ui::VKEY_A, ui::EF_NONE}},

      // Press Control+Search+Alt+a. Confirm the event is now VKEY_A only with
      // the Alt modifier.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN},
       {ui::VKEY_A, ui::EF_ALT_DOWN}},
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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::EF_ALT_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN}},

      // Press Alt+a. Confirm the event is now Control+a even though the Control
      // key itself is disabled.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_ALT_DOWN},
       {ui::VKEY_A, ui::EF_CONTROL_DOWN}},
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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_COMMAND_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN}},
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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::EF_ALT_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN}},

      // Press Alt+Search. Confirm the event is now VKEY_CONTROL.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN}},

      // Press Control+Alt+Search. Confirm the event is now VKEY_CONTROL.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN}},

      // Press Shift+Control+Alt+Search. Confirm the event is now Control with
      // Shift and Control modifiers.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                           ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN},
       {ui::VKEY_CONTROL, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}},

      // Press Shift+Control+Alt+Search+B. Confirm the event is now B with Shift
      // and Control modifiers.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
                        ui::EF_COMMAND_DOWN},
       {ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}},
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

  KeyTestCase tests[] = {// Press Search. Confirm the event is now VKEY_ESCAPE.
                         {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
                          {ui::VKEY_LWIN, ui::EF_COMMAND_DOWN},
                          {ui::VKEY_ESCAPE, ui::EF_NONE}},
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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_COMMAND_DOWN},
       {ui::VKEY_MENU, ui::EF_ALT_DOWN}},
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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_MENU, ui::EF_ALT_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN}},
      // Press Shift+comma. Verify that only the flags are changed.
      // The X11 portion of the test addresses crbug.com/390263 by verifying
      // that the X keycode remains that for ',<' and not for 105-key '<>'.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_COMMA, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN},
       {ui::VKEY_OEM_COMMA, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}},
      // Press Shift+9. Verify that only the flags are changed.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN},
       {ui::VKEY_9, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}},
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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN},
       {ui::VKEY_LWIN, ui::EF_COMMAND_DOWN}},

      // Then, press all of the three, Control+Alt+Search.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN},
       {ui::VKEY_MENU,
        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN}},

      // Press Shift+Control+Alt+Search.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                           ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN},
       {ui::VKEY_MENU, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                           ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN}},

      // Press Shift+Control+Alt+Search+B
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
                        ui::EF_COMMAND_DOWN},
       {ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
                        ui::EF_COMMAND_DOWN}},
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
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_CAPITAL,
                                ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_LWIN, ui::EF_COMMAND_DOWN, ui::ET_KEY_PRESSED));
  // Confirm that the Caps Lock status is changed.
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);

  // Release Search.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CAPITAL, ui::EF_NONE, ui::ET_KEY_RELEASED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_LWIN, ui::EF_NONE, ui::ET_KEY_RELEASED));
  // Confirm that the Caps Lock status is not changed.
  EXPECT_TRUE(ime_keyboard.caps_lock_is_enabled_);

  // Press Search.
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_CAPITAL,
                                ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(&rewriter,
                                ui::VKEY_LWIN,
                                ui::EF_COMMAND_DOWN | ui::EF_CAPS_LOCK_DOWN,
                                ui::ET_KEY_PRESSED));
  // Confirm that the Caps Lock status is changed.
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Release Search.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CAPITAL, ui::EF_NONE, ui::ET_KEY_RELEASED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_LWIN, ui::EF_NONE, ui::ET_KEY_RELEASED));
  // Confirm that the Caps Lock status is not changed.
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Press Caps Lock (on an external keyboard).
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CAPITAL,
                                      ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CAPITAL,
                                      ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                      ui::ET_KEY_PRESSED));

  // Confirm that calling RewriteForTesting() does not change the state of
  // |ime_keyboard|. In this case, X Window system itself should change the
  // Caps Lock state, not ash::EventRewriter.
  EXPECT_FALSE(ime_keyboard.caps_lock_is_enabled_);

  // Release Caps Lock (on an external keyboard).
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CAPITAL, ui::EF_NONE, ui::ET_KEY_RELEASED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_CAPITAL, ui::EF_NONE, ui::ET_KEY_RELEASED));
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
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CAPITAL,
                                      ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F16, ui::EF_MOD3_DOWN, ui::ET_KEY_PRESSED));
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
       {ui::VKEY_F15, ui::EF_NONE},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN}},

      {KeyTestCase::TEST_VKEY,
       ui::ET_KEY_RELEASED,
       {ui::VKEY_F15, ui::EF_NONE},
       {ui::VKEY_CONTROL, ui::EF_NONE}},

      // However, Mod2Mask should not be rewritten to CtrlMask when
      // --has-chromeos-diamond-key is not specified.
      {KeyTestCase::TEST_VKEY,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_NONE},
       {ui::VKEY_A, ui::EF_NONE}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteDiamondKeyWithFlag) {
  const CommandLine original_cl(*CommandLine::ForCurrentProcess());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
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
                ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_PRESSED));
  // Check that Control is applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_A, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED));
  // Release F15
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_NONE, ui::ET_KEY_RELEASED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_RELEASED));
  // Check that Control is no longer applied to a subsequent key press.
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED));

  IntegerPrefMember diamond;
  diamond.Init(prefs::kLanguageRemapDiamondKeyTo, &prefs);
  diamond.SetValue(chromeos::input_method::kVoidKey);

  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_UNKNOWN, ui::EF_NONE, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_PRESSED));
  // Check that no modifier is applied to another key.
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED));

  diamond.SetValue(chromeos::input_method::kControlKey);

  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_PRESSED));
  // Check that Control is applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_A, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED));
  // Release F15
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_NONE, ui::ET_KEY_RELEASED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_RELEASED));
  // Check that Control is no longer applied to a subsequent key press.
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED));

  diamond.SetValue(chromeos::input_method::kAltKey);

  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_MENU, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_PRESSED));
  // Check that Alt is applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_A, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED));
  // Release F15
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_MENU, ui::EF_NONE, ui::ET_KEY_RELEASED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_RELEASED));
  // Check that Alt is no longer applied to a subsequent key press.
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED));

  diamond.SetValue(chromeos::input_method::kCapsLockKey);

  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CAPITAL,
                                      ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_PRESSED));
  // Check that Caps is applied to a subsequent key press.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED));
  // Release F15
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CAPITAL, ui::EF_NONE, ui::ET_KEY_RELEASED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_RELEASED));
  // Check that Control is no longer applied to a subsequent key press.
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED));

  *CommandLine::ForCurrentProcess() = original_cl;
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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_MOD3_DOWN},
       {ui::VKEY_A, ui::EF_CONTROL_DOWN}},

      // Press Control+CapsLock+a. Confirm that Mod3Mask is rewritten to
      // ControlMask
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_MOD3_DOWN},
       {ui::VKEY_A, ui::EF_CONTROL_DOWN}},

      // Press Alt+CapsLock+a. Confirm that Mod3Mask is rewritten to
      // ControlMask.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_ALT_DOWN | ui::EF_MOD3_DOWN},
       {ui::VKEY_A, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN}},
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
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED));

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
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::EF_ALT_DOWN},
       {ui::VKEY_DELETE, ui::EF_NONE}},
      // Control+Alt+Backspace -> Control+Delete
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN},
       {ui::VKEY_DELETE, ui::EF_CONTROL_DOWN}},
      // Search+Alt+Backspace -> Alt+Backspace
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
       {ui::VKEY_BACK, ui::EF_ALT_DOWN}},
      // Search+Control+Alt+Backspace -> Control+Alt+Backspace
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN},
       {ui::VKEY_BACK, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN}},
      // Alt+Up -> Prior
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::EF_ALT_DOWN},
       {ui::VKEY_PRIOR, ui::EF_NONE}},
      // Alt+Down -> Next
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::EF_ALT_DOWN},
       {ui::VKEY_NEXT, ui::EF_NONE}},
      // Ctrl+Alt+Up -> Home
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN},
       {ui::VKEY_HOME, ui::EF_NONE}},
      // Ctrl+Alt+Down -> End
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN},
       {ui::VKEY_END, ui::EF_NONE}},

      // Search+Alt+Up -> Alt+Up
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
       {ui::VKEY_UP, ui::EF_ALT_DOWN}},
      // Search+Alt+Down -> Alt+Down
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
       {ui::VKEY_DOWN, ui::EF_ALT_DOWN}},
      // Search+Ctrl+Alt+Up -> Search+Ctrl+Alt+Up
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_UP,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN},
       {ui::VKEY_UP, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN}},
      // Search+Ctrl+Alt+Down -> Ctrl+Alt+Down
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN,
        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN},
       {ui::VKEY_DOWN, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN}},

      // Period -> Period
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PERIOD, ui::EF_NONE},
       {ui::VKEY_OEM_PERIOD, ui::EF_NONE}},

      // Search+Backspace -> Delete
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_BACK, ui::EF_COMMAND_DOWN},
       {ui::VKEY_DELETE, ui::EF_NONE}},
      // Search+Up -> Prior
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_UP, ui::EF_COMMAND_DOWN},
       {ui::VKEY_PRIOR, ui::EF_NONE}},
      // Search+Down -> Next
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::EF_COMMAND_DOWN},
       {ui::VKEY_NEXT, ui::EF_NONE}},
      // Search+Left -> Home
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LEFT, ui::EF_COMMAND_DOWN},
       {ui::VKEY_HOME, ui::EF_NONE}},
      // Control+Search+Left -> Home
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LEFT, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN},
       {ui::VKEY_HOME, ui::EF_CONTROL_DOWN}},
      // Search+Right -> End
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN},
       {ui::VKEY_END, ui::EF_NONE}},
      // Control+Search+Right -> End
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN},
       {ui::VKEY_END, ui::EF_CONTROL_DOWN}},
      // Search+Period -> Insert
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PERIOD, ui::EF_COMMAND_DOWN},
       {ui::VKEY_INSERT, ui::EF_NONE}},
      // Control+Search+Period -> Control+Insert
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PERIOD, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN},
       {ui::VKEY_INSERT, ui::EF_CONTROL_DOWN}}};

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

  FunctionKeyTestCase tests[] = {
      // F1 -> Back
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::EF_NONE},
       {ui::VKEY_BROWSER_BACK, ui::EF_NONE},
       {ui::VKEY_BROWSER_BACK, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::EF_ALT_DOWN},
       {ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN},
       {ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN | ui::EF_FUNCTION_KEY}},
      // F2 -> Forward
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::EF_NONE},
       {ui::VKEY_BROWSER_FORWARD, ui::EF_NONE},
       {ui::VKEY_BROWSER_FORWARD, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::EF_ALT_DOWN},
       {ui::VKEY_BROWSER_FORWARD, ui::EF_ALT_DOWN},
       {ui::VKEY_BROWSER_FORWARD, ui::EF_ALT_DOWN | ui::EF_FUNCTION_KEY}},
      // F3 -> Refresh
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::EF_NONE},
       {ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
       {ui::VKEY_BROWSER_REFRESH, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BROWSER_REFRESH, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BROWSER_REFRESH, ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::EF_ALT_DOWN},
       {ui::VKEY_BROWSER_REFRESH, ui::EF_ALT_DOWN},
       {ui::VKEY_BROWSER_REFRESH, ui::EF_ALT_DOWN | ui::EF_FUNCTION_KEY}},
      // F4 -> Launch App 2
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::EF_NONE},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_NONE},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::EF_CONTROL_DOWN},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_CONTROL_DOWN},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::EF_ALT_DOWN},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_ALT_DOWN},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_ALT_DOWN | ui::EF_FUNCTION_KEY}},
      // F5 -> Launch App 1
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::EF_NONE},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::EF_CONTROL_DOWN},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::EF_ALT_DOWN},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_ALT_DOWN},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_ALT_DOWN | ui::EF_FUNCTION_KEY}},
      // F6 -> Brightness down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::EF_NONE},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::EF_ALT_DOWN},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::EF_ALT_DOWN},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::EF_ALT_DOWN | ui::EF_FUNCTION_KEY}},
      // F7 -> Brightness up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::EF_NONE},
       {ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE},
       {ui::VKEY_BRIGHTNESS_UP, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::EF_ALT_DOWN},
       {ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN},
       {ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN | ui::EF_FUNCTION_KEY}},
      // F8 -> Volume Mute
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::EF_NONE},
       {ui::VKEY_VOLUME_MUTE, ui::EF_NONE},
       {ui::VKEY_VOLUME_MUTE, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::EF_CONTROL_DOWN},
       {ui::VKEY_VOLUME_MUTE, ui::EF_CONTROL_DOWN},
       {ui::VKEY_VOLUME_MUTE, ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::EF_ALT_DOWN},
       {ui::VKEY_VOLUME_MUTE, ui::EF_ALT_DOWN},
       {ui::VKEY_VOLUME_MUTE, ui::EF_ALT_DOWN | ui::EF_FUNCTION_KEY}},
      // F9 -> Volume Down
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::EF_NONE},
       {ui::VKEY_VOLUME_DOWN, ui::EF_NONE},
       {ui::VKEY_VOLUME_DOWN, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::EF_CONTROL_DOWN},
       {ui::VKEY_VOLUME_DOWN, ui::EF_CONTROL_DOWN},
       {ui::VKEY_VOLUME_DOWN, ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::EF_ALT_DOWN},
       {ui::VKEY_VOLUME_DOWN, ui::EF_ALT_DOWN},
       {ui::VKEY_VOLUME_DOWN, ui::EF_ALT_DOWN | ui::EF_FUNCTION_KEY}},
      // F10 -> Volume Up
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::EF_NONE},
       {ui::VKEY_VOLUME_UP, ui::EF_NONE},
       {ui::VKEY_VOLUME_UP, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::EF_CONTROL_DOWN},
       {ui::VKEY_VOLUME_UP, ui::EF_CONTROL_DOWN},
       {ui::VKEY_VOLUME_UP, ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::EF_ALT_DOWN},
       {ui::VKEY_VOLUME_UP, ui::EF_ALT_DOWN},
       {ui::VKEY_VOLUME_UP, ui::EF_ALT_DOWN | ui::EF_FUNCTION_KEY}},
      // F11 -> F11
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::EF_NONE},
       {ui::VKEY_F11, ui::EF_NONE},
       {ui::VKEY_F11, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::EF_CONTROL_DOWN},
       {ui::VKEY_F11, ui::EF_CONTROL_DOWN},
       {ui::VKEY_F11, ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::EF_ALT_DOWN},
       {ui::VKEY_F11, ui::EF_ALT_DOWN},
       {ui::VKEY_F11, ui::EF_ALT_DOWN | ui::EF_FUNCTION_KEY}},
      // F12 -> F12
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::EF_NONE},
       {ui::VKEY_F12, ui::EF_NONE},
       {ui::VKEY_F12, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::EF_CONTROL_DOWN},
       {ui::VKEY_F12, ui::EF_CONTROL_DOWN},
       {ui::VKEY_F12, ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::EF_ALT_DOWN},
       {ui::VKEY_F12, ui::EF_ALT_DOWN},
       {ui::VKEY_F12, ui::EF_ALT_DOWN | ui::EF_FUNCTION_KEY}},

      // The number row should not be rewritten without Search key.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_1, ui::EF_NONE},
       {ui::VKEY_1, ui::EF_NONE},
       {ui::VKEY_1, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_2, ui::EF_NONE},
       {ui::VKEY_2, ui::EF_NONE},
       {ui::VKEY_2, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_3, ui::EF_NONE},
       {ui::VKEY_3, ui::EF_NONE},
       {ui::VKEY_3, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_4, ui::EF_NONE},
       {ui::VKEY_4, ui::EF_NONE},
       {ui::VKEY_4, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_5, ui::EF_NONE},
       {ui::VKEY_5, ui::EF_NONE},
       {ui::VKEY_5, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_6, ui::EF_NONE},
       {ui::VKEY_6, ui::EF_NONE},
       {ui::VKEY_6, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_7, ui::EF_NONE},
       {ui::VKEY_7, ui::EF_NONE},
       {ui::VKEY_7, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_8, ui::EF_NONE},
       {ui::VKEY_8, ui::EF_NONE},
       {ui::VKEY_8, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::EF_NONE},
       {ui::VKEY_9, ui::EF_NONE},
       {ui::VKEY_9, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_0, ui::EF_NONE},
       {ui::VKEY_0, ui::EF_NONE},
       {ui::VKEY_0, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_MINUS, ui::EF_NONE},
       {ui::VKEY_OEM_MINUS, ui::EF_NONE},
       {ui::VKEY_OEM_MINUS, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PLUS, ui::EF_NONE},
       {ui::VKEY_OEM_PLUS, ui::EF_NONE},
       {ui::VKEY_OEM_PLUS, ui::EF_NONE}},

      // The number row should be rewritten as the F<number> row with Search
      // key.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_1, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F1, ui::EF_NONE},
       {ui::VKEY_F1, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_2, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F2, ui::EF_NONE},
       {ui::VKEY_F2, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_3, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F3, ui::EF_NONE},
       {ui::VKEY_F3, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_4, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F4, ui::EF_NONE},
       {ui::VKEY_F4, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_5, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F5, ui::EF_NONE},
       {ui::VKEY_F5, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_6, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F6, ui::EF_NONE},
       {ui::VKEY_F6, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_7, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F7, ui::EF_NONE},
       {ui::VKEY_F7, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_8, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F8, ui::EF_NONE},
       {ui::VKEY_F8, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F9, ui::EF_NONE},
       {ui::VKEY_F9, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_0, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F10, ui::EF_NONE},
       {ui::VKEY_F10, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_MINUS, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F11, ui::EF_NONE},
       {ui::VKEY_F11, ui::EF_NONE}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PLUS, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F12, ui::EF_NONE},
       {ui::VKEY_F12, ui::EF_NONE}},

      // The function keys should not be rewritten with Search key pressed.
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F1, ui::EF_NONE},
       {ui::VKEY_F1, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F2, ui::EF_NONE},
       {ui::VKEY_F2, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F3, ui::EF_NONE},
       {ui::VKEY_F3, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F4, ui::EF_NONE},
       {ui::VKEY_F4, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F5, ui::EF_NONE},
       {ui::VKEY_F5, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F6, ui::EF_NONE},
       {ui::VKEY_F6, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F7, ui::EF_NONE},
       {ui::VKEY_F7, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F8, ui::EF_NONE},
       {ui::VKEY_F8, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F9, ui::EF_NONE},
       {ui::VKEY_F9, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F10, ui::EF_NONE},
       {ui::VKEY_F10, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F11, ui::EF_NONE},
       {ui::VKEY_F11, ui::EF_FUNCTION_KEY}},
      {ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F12, ui::EF_NONE},
       {ui::VKEY_F12, ui::EF_FUNCTION_KEY}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckFunctionKeyTestCase(&rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteExtendedKeysWithSearchRemapped) {
  const CommandLine original_cl(*CommandLine::ForCurrentProcess());

  // Remap Search to Control.
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kControlKey);

  EventRewriter rewriter(NULL);
  rewriter.KeyboardDeviceAddedForTesting(kKeyboardDeviceId, "PC Keyboard");
  rewriter.set_pref_service_for_testing(&prefs);

  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kHasChromeOSKeyboard, "");

  KeyTestCase tests[] = {
      // Alt+Search+Down -> End
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN},
       {ui::VKEY_END, ui::EF_NONE}},

      // Shift+Alt+Search+Down -> Shift+End
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_DOWN,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN},
       {ui::VKEY_END, ui::EF_SHIFT_DOWN}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(i);
    CheckKeyTestCase(&rewriter, tests[i]);
  }

  *CommandLine::ForCurrentProcess() = original_cl;
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
    ui::KeyEvent keyevent(
        ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, ui::EF_FINAL);
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
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, location, kTouchId, base::TimeDelta());
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
  virtual ~EventBuffer() {}

  void PopEvents(ScopedVector<ui::Event>* events) {
    events->clear();
    events->swap(events_);
  }

 private:
  // ui::EventProcessor overrides:
  virtual ui::EventDispatchDetails OnEventFromSource(
      ui::Event* event) OVERRIDE {
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
  virtual ui::EventProcessor* GetEventProcessor() OVERRIDE {
    return processor_;
  }
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
        mock_user_manager_(new chromeos::MockUserManager),
        user_manager_enabler_(mock_user_manager_) {}
  virtual ~EventRewriterAshTest() {}

  bool RewriteFunctionKeys(const ui::Event& event,
                           scoped_ptr<ui::Event>* rewritten_event) {
    return rewriter_->RewriteEvent(event, rewritten_event);
  }

  ui::EventDispatchDetails Send(ui::Event* event) {
    return source_.Send(event);
  }

  void SendKeyEvent(ui::EventType type, ui::KeyboardCode key_code) {
    ui::KeyEvent press(type, key_code, ui::EF_NONE);
    ui::EventDispatchDetails details = Send(&press);
    CHECK(!details.dispatcher_destroyed);
  }

  void SendActivateStickyKeyPattern(ui::KeyboardCode key_code) {
    SendKeyEvent(ui::ET_KEY_PRESSED, key_code);
    SendKeyEvent(ui::ET_KEY_RELEASED, key_code);
  }

 protected:
  TestingPrefServiceSyncable* prefs() { return &prefs_; }

  void PopEvents(ScopedVector<ui::Event>* events) {
    buffer_.PopEvents(events);
  }

  virtual void SetUp() OVERRIDE {
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

  virtual void TearDown() OVERRIDE {
    rewriter_.reset();
    AshTestBase::TearDown();
  }

 protected:
  ash::StickyKeysController* sticky_keys_controller_;

 private:
  scoped_ptr<EventRewriter> rewriter_;

  EventBuffer buffer_;
  TestEventSource source_;

  chromeos::MockUserManager* mock_user_manager_;  // Not owned.
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
  ui::KeyEvent press_f1(ui::ET_KEY_PRESSED, ui::VKEY_F1, ui::EF_NONE);

  // Simulate an apps v2 window that has requested top row keys as function
  // keys. The event should not be rewritten.
  window_state->set_top_row_keys_are_function_keys(true);
  ui::EventDispatchDetails details = Send(&press_f1);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_F1, ui::EF_NONE, ui::ET_KEY_PRESSED),
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
      GetExpectedResultAsString(ui::VKEY_F1, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetKeyEventAsString(*static_cast<ui::KeyEvent*>(events[0])));

  // If the pref isn't set when an event is sent to a regular window, F1 is
  // rewritten to the back key.
  send_function_keys_pref.SetValue(false);
  details = Send(&press_f1);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_BROWSER_BACK, ui::EF_NONE, ui::ET_KEY_PRESSED),
            GetKeyEventAsString(*static_cast<ui::KeyEvent*>(events[0])));
}

TEST_F(EventRewriterTest, TestRewrittenModifierClick) {
  std::vector<unsigned int> device_list;
  device_list.push_back(10);
  ui::TouchFactory::GetInstance()->SetPointerDeviceForTest(device_list);

  // Remap Control to Alt.
  TestingPrefServiceSyncable prefs;
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
  xev.InitGenericButtonEvent(10,
                             ui::ET_MOUSE_PRESSED,
                             gfx::Point(),
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
  EXPECT_FALSE(ui::EF_ALT_DOWN & result->flags());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
}

TEST_F(EventRewriterTest, DontRewriteIfNotRewritten) {
  // TODO(kpschoedel): pending changes for crbug.com/360377
  // to |chromeos::EventRewriter::RewriteLocatedEvent()
  std::vector<unsigned int> device_list;
  device_list.push_back(10);
  device_list.push_back(11);
  ui::TouchFactory::GetInstance()->SetPointerDeviceForTest(device_list);
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter(NULL);
  rewriter.set_pref_service_for_testing(&prefs);
  const int kLeftAndAltFlag = ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN;

  // Test Alt + Left click.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED,
                         gfx::Point(),
                         gfx::Point(),
                         kLeftAndAltFlag,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(10);
    // Sanity check.
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(kLeftAndAltFlag, press.flags());
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED,
                           gfx::Point(),
                           gfx::Point(),
                           kLeftAndAltFlag,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(10);
    scoped_ptr<ui::Event> new_event;
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
    xev.InitGenericButtonEvent(
        10, ui::ET_MOUSE_PRESSED, gfx::Point(), kLeftAndAltFlag);
    ui::MouseEvent press(xev);
    // Sanity check.
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(kLeftAndAltFlag, press.flags());
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(
        10, ui::ET_MOUSE_RELEASED, gfx::Point(), kLeftAndAltFlag);
    ui::MouseEvent release(xev);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
#endif

  // No ALT in frst click.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED,
                         gfx::Point(),
                         gfx::Point(),
                         ui::EF_LEFT_MOUSE_BUTTON,
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
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED,
                           gfx::Point(),
                           gfx::Point(),
                           kLeftAndAltFlag,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(10);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE((ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN) & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
#if defined(USE_X11)
  // No ALT in frst click, using XI2 native events.
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(
        10, ui::ET_MOUSE_PRESSED, gfx::Point(), ui::EF_LEFT_MOUSE_BUTTON);
    ui::MouseEvent press(xev);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(
        10, ui::ET_MOUSE_RELEASED, gfx::Point(), kLeftAndAltFlag);
    ui::MouseEvent release(xev);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE((ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN) & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
#endif

  // ALT on different device.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED,
                         gfx::Point(),
                         gfx::Point(),
                         kLeftAndAltFlag,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(11);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED,
                           gfx::Point(),
                           gfx::Point(),
                           kLeftAndAltFlag,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(10);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE((ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN) & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED,
                           gfx::Point(),
                           gfx::Point(),
                           kLeftAndAltFlag,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(11);
    scoped_ptr<ui::Event> new_event;
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
    xev.InitGenericButtonEvent(
        11, ui::ET_MOUSE_PRESSED, gfx::Point(), kLeftAndAltFlag);
    ui::MouseEvent press(xev);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, press, &new_event);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result->flags());
    EXPECT_FALSE(kLeftAndAltFlag & result->flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(
        10, ui::ET_MOUSE_RELEASED, gfx::Point(), kLeftAndAltFlag);
    ui::MouseEvent release(xev);
    scoped_ptr<ui::Event> new_event;
    const ui::MouseEvent* result =
        RewriteMouseButtonEvent(&rewriter, release, &new_event);
    EXPECT_TRUE((ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN) & result->flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result->changed_button_flags());
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(
        11, ui::ET_MOUSE_RELEASED, gfx::Point(), kLeftAndAltFlag);
    ui::MouseEvent release(xev);
    scoped_ptr<ui::Event> new_event;
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

  SendActivateStickyKeyPattern(ui::VKEY_CONTROL);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_KEY_PRESSED, events[0]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[0])->key_code());

  // Test key press event is correctly modified and modifier release
  // event is sent.
  ui::KeyEvent press(ui::ET_KEY_PRESSED, ui::VKEY_C, ui::EF_NONE);
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
  ui::KeyEvent release(ui::ET_KEY_RELEASED, ui::VKEY_C, ui::EF_NONE);
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

  SendActivateStickyKeyPattern(ui::VKEY_CONTROL);
  PopEvents(&events);

  // Test mouse press event is correctly modified.
  gfx::Point location(0, 0);
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED,
                       location,
                       location,
                       ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventDispatchDetails details = Send(&press);
  ASSERT_FALSE(details.dispatcher_destroyed);
  PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0]->type());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);

  // Test mouse release event is correctly modified and modifier release
  // event is sent. The mouse event should have the correct DIP location.
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED,
                         location,
                         location,
                         ui::EF_LEFT_MOUSE_BUTTON,
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
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL);
  PopEvents(&events);
  gfx::Point location(0, 0);
  ui::MouseEvent mev(ui::ET_MOUSEWHEEL,
                     location,
                     location,
                     ui::EF_LEFT_MOUSE_BUTTON,
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
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL);
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

  virtual ~StickyKeysOverlayTest() {}

  virtual void SetUp() OVERRIDE {
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
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_T);
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
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_N);
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
  SendActivateStickyKeyPattern(ui::VKEY_LMENU);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Pressing a normal key should not hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_D);
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
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing another modifier key should still show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing a normal key should not hide overlay but disable normal modifier.
  SendActivateStickyKeyPattern(ui::VKEY_D);
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

  // Enable modifiers.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU);

  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Disable modifiers and overlay should be hidden.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU);

  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ash::STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
}

TEST_F(StickyKeysOverlayTest, ModifierVisibility) {
  // All but AltGr and Mod3 should initially be visible.
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_CONTROL_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALT_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn all modifiers on.
  sticky_keys_controller_->SetModifiersEnabled(true, true);
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_CONTROL_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALT_DOWN));
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
