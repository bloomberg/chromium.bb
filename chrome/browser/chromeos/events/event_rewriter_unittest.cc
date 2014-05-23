// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/event_rewriter.h"

#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/Xlib.h>
#undef Bool
#undef None
#undef RootWindow

#include <vector>

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/prefs/pref_member.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/ime/fake_ime_keyboard.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/events/x/touch_factory_x11.h"
#include "ui/gfx/x/x11_types.h"

namespace {

std::string GetExpectedResultAsString(ui::KeyboardCode ui_keycode,
                                      int ui_flags,
                                      ui::EventType ui_type) {
  return base::StringPrintf("ui_keycode=0x%X ui_flags=0x%X ui_type=%d",
                            ui_keycode,
                            ui_flags,
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
  const ui::KeyEvent event(ui_type, ui_keycode, ui_flags, false);
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

// Tests a single stateless key rewrite operation.
// |i| is a an identifying number to locate failing tests in the tables.
void CheckKeyTestCase(size_t i,
                      chromeos::EventRewriter* rewriter,
                      const KeyTestCase& test) {
  std::string id = base::StringPrintf("(%zu) ", i);
  std::string expected =
      id + GetExpectedResultAsString(
               test.expected.key_code, test.expected.flags, test.type);

  if (test.test & KeyTestCase::TEST_VKEY) {
    // Check rewriting of a non-native-based key event.
    EXPECT_EQ(
        expected,
        id + GetRewrittenEventAsString(
                 rewriter, test.input.key_code, test.input.flags, test.type));
  }

#if defined(USE_X11)
  if (test.test & KeyTestCase::TEST_X11) {
    ui::ScopedXI2Event xev;
    xev.InitKeyEvent(test.type, test.input.key_code, test.input.flags);
    XEvent* xevent = xev;
    if (xevent->xkey.keycode) {
      ui::KeyEvent xkey_event(xevent, false);
      if (test.test & KeyTestCase::NUMPAD)
        xkey_event.set_flags(xkey_event.flags() | ui::EF_NUMPAD_KEY);
      // Verify that the X11-based key event is as expected.
      EXPECT_EQ(id + GetExpectedResultAsString(
                         test.input.key_code, test.input.flags, test.type),
                id + GetKeyEventAsString(xkey_event));
      // Rewrite the event and check the result.
      scoped_ptr<ui::Event> new_event;
      rewriter->RewriteEvent(xkey_event, &new_event);
      ui::KeyEvent& rewritten_key_event =
          new_event ? *static_cast<ui::KeyEvent*>(new_event.get()) : xkey_event;
      EXPECT_EQ(expected, id + GetKeyEventAsString(rewritten_key_event));
      // Build a new ui::KeyEvent from the rewritten native component,
      // and check that it also matches the rewritten event.
      ui::KeyEvent from_native_event(rewritten_key_event.native_event(), false);
      EXPECT_EQ(expected, id + GetKeyEventAsString(from_native_event));
    }
  }
#endif
}

}  // namespace

namespace chromeos {

class EventRewriterTest : public ash::test::AshTestBase {
 public:
  EventRewriterTest()
      : display_(gfx::GetXDisplay()),
        mock_user_manager_(new chromeos::MockUserManager),
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

  int RewriteMouseEvent(chromeos::EventRewriter* rewriter,
                        const ui::MouseEvent& event) {
    int flags = event.flags();
    rewriter->RewriteLocatedEventForTesting(event, &flags);
    return flags;
  }

  Display* display_;
  chromeos::MockUserManager* mock_user_manager_;  // Not owned.
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  chromeos::input_method::MockInputMethodManager* input_method_manager_mock_;
};

TEST_F(EventRewriterTest, TestRewriteCommandToControl) {
  // First, test with a PC keyboard.
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter;
  rewriter.DeviceAddedForTesting(0, "PC Keyboard");
  rewriter.set_last_device_id_for_testing(0);
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
    CheckKeyTestCase(1000 + i, &rewriter, pc_keyboard_tests[i]);
  }

  // An Apple keyboard reusing the ID, zero.
  rewriter.DeviceAddedForTesting(0, "Apple Keyboard");
  rewriter.set_last_device_id_for_testing(0);

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
    CheckKeyTestCase(2000 + i, &rewriter, apple_keyboard_tests[i]);
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

  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.DeviceAddedForTesting(0, "PC Keyboard");
  rewriter.set_last_device_id_for_testing(0);

  KeyTestCase pc_keyboard_tests[] = {// Control should be remapped to Alt.
                                     {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
                                      {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN},
                                      {ui::VKEY_MENU, ui::EF_ALT_DOWN}},
  };

  for (size_t i = 0; i < arraysize(pc_keyboard_tests); ++i) {
    CheckKeyTestCase(1000 + i, &rewriter, pc_keyboard_tests[i]);
  }

  // An Apple keyboard reusing the ID, zero.
  rewriter.DeviceAddedForTesting(0, "Apple Keyboard");
  rewriter.set_last_device_id_for_testing(0);

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
    CheckKeyTestCase(2000 + i, &rewriter, apple_keyboard_tests[i]);
  }
}

void EventRewriterTest::TestRewriteNumPadKeys() {
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter;
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
    CheckKeyTestCase(1000 + i, &rewriter, tests[i]);
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
  EventRewriter rewriter;
  rewriter.DeviceAddedForTesting(0, "Apple Keyboard");
  rewriter.set_last_device_id_for_testing(0);
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
    CheckKeyTestCase(1000 + i, &rewriter, tests[i]);
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
  EventRewriter rewriter;
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
    CheckKeyTestCase(1000 + i, &rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersNoRemapMultipleKeys) {
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter;
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
    CheckKeyTestCase(1000 + i, &rewriter, tests[i]);
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

  EventRewriter rewriter;
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
    CheckKeyTestCase(1000 + i, &rewriter, disabled_modifier_tests[i]);
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
    CheckKeyTestCase(2000 + i, &rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToControl) {
  // Remap Search to Control.
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kControlKey);

  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase s_tests[] = {
      // Press Search. Confirm the event is now VKEY_CONTROL.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_COMMAND_DOWN},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN}},
  };

  for (size_t i = 0; i < arraysize(s_tests); ++i) {
    CheckKeyTestCase(1000 + i, &rewriter, s_tests[i]);
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
    CheckKeyTestCase(2000 + i, &rewriter, sa_tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToEscape) {
  // Remap Search to ESC.
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kEscapeKey);

  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {// Press Search. Confirm the event is now VKEY_ESCAPE.
                         {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
                          {ui::VKEY_LWIN, ui::EF_COMMAND_DOWN},
                          {ui::VKEY_ESCAPE, ui::EF_NONE}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    CheckKeyTestCase(1000 + i, &rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapMany) {
  // Remap Search to Alt.
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember search;
  search.Init(prefs::kLanguageRemapSearchKeyTo, &prefs);
  search.SetValue(chromeos::input_method::kAltKey);

  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase s2a_tests[] = {
      // Press Search. Confirm the event is now VKEY_MENU.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_LWIN, ui::EF_COMMAND_DOWN},
       {ui::VKEY_MENU, ui::EF_ALT_DOWN}},
  };

  for (size_t i = 0; i < arraysize(s2a_tests); ++i) {
    CheckKeyTestCase(1000 + i, &rewriter, s2a_tests[i]);
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
  };

  for (size_t i = 0; i < arraysize(a2c_tests); ++i) {
    CheckKeyTestCase(2000 + i, &rewriter, a2c_tests[i]);
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
    CheckKeyTestCase(3000 + i, &rewriter, c2s_tests[i]);
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
  EventRewriter rewriter;
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
  EventRewriter rewriter;
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
  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.set_ime_keyboard_for_testing(&ime_keyboard);

  KeyTestCase tests[] = {
      // F15 should work as Ctrl when --has-chromeos-diamond-key is not
      // specified.
      {KeyTestCase::TEST_VKEY,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_F15, ui::EF_NONE},
       {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN}},

      // However, Mod2Mask should not be rewritten to CtrlMask when
      // --has-chromeos-diamond-key is not specified.
      {KeyTestCase::TEST_VKEY,
       ui::ET_KEY_PRESSED,
       {ui::VKEY_A, ui::EF_NONE},
       {ui::VKEY_A, ui::EF_NONE}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    CheckKeyTestCase(1000 + i, &rewriter, tests[i]);
  }
}
TEST_F(EventRewriterTest, TestRewriteDiamondKeyWithFlag) {
  const CommandLine original_cl(*CommandLine::ForCurrentProcess());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kHasChromeOSDiamondKey, "");

  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());

  chromeos::input_method::FakeImeKeyboard ime_keyboard;
  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.set_ime_keyboard_for_testing(&ime_keyboard);

  // By default, F15 should work as Control.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_PRESSED));

  IntegerPrefMember diamond;
  diamond.Init(prefs::kLanguageRemapDiamondKeyTo, &prefs);
  diamond.SetValue(chromeos::input_method::kVoidKey);

  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_UNKNOWN, ui::EF_NONE, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_PRESSED));

  diamond.SetValue(chromeos::input_method::kControlKey);

  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_PRESSED));

  diamond.SetValue(chromeos::input_method::kAltKey);

  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_MENU, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_PRESSED));

  diamond.SetValue(chromeos::input_method::kCapsLockKey);

  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CAPITAL,
                                      ui::EF_CAPS_LOCK_DOWN | ui::EF_MOD3_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_PRESSED));

  *CommandLine::ForCurrentProcess() = original_cl;
}

TEST_F(EventRewriterTest, TestRewriteCapsLockToControl) {
  // Remap CapsLock to Control.
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapCapsLockKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kControlKey);

  EventRewriter rewriter;
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
    CheckKeyTestCase(1000 + i, &rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteCapsLockMod3InUse) {
  // Remap CapsLock to Control.
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapCapsLockKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kControlKey);

  EventRewriter rewriter;
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
  EventRewriter rewriter;
  rewriter.DeviceAddedForTesting(0, "PC Keyboard");
  rewriter.set_last_device_id_for_testing(0);
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
    CheckKeyTestCase(1000 + i, &rewriter, tests[i]);
  }
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeys) {
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  KeyTestCase tests[] = {
      // F1 -> Back
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::EF_NONE},
       {ui::VKEY_BROWSER_BACK, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::EF_ALT_DOWN},
       {ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN}},
      // F2 -> Forward
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::EF_NONE},
       {ui::VKEY_BROWSER_FORWARD, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::EF_ALT_DOWN},
       {ui::VKEY_BROWSER_FORWARD, ui::EF_ALT_DOWN}},
      // F3 -> Refresh
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::EF_NONE},
       {ui::VKEY_BROWSER_REFRESH, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BROWSER_REFRESH, ui::EF_CONTROL_DOWN}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::EF_ALT_DOWN},
       {ui::VKEY_BROWSER_REFRESH, ui::EF_ALT_DOWN}},
      // F4 -> Launch App 2
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::EF_NONE},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::EF_CONTROL_DOWN},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_CONTROL_DOWN}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::EF_ALT_DOWN},
       {ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_ALT_DOWN}},
      // F5 -> Launch App 1
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::EF_NONE},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::EF_CONTROL_DOWN},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::EF_ALT_DOWN},
       {ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_ALT_DOWN}},
      // F6 -> Brightness down
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::EF_NONE},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::EF_ALT_DOWN},
       {ui::VKEY_BRIGHTNESS_DOWN, ui::EF_ALT_DOWN}},
      // F7 -> Brightness up
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::EF_NONE},
       {ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::EF_CONTROL_DOWN},
       {ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::EF_ALT_DOWN},
       {ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN}},
      // F8 -> Volume Mute
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::EF_NONE},
       {ui::VKEY_VOLUME_MUTE, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::EF_CONTROL_DOWN},
       {ui::VKEY_VOLUME_MUTE, ui::EF_CONTROL_DOWN}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::EF_ALT_DOWN},
       {ui::VKEY_VOLUME_MUTE, ui::EF_ALT_DOWN}},
      // F9 -> Volume Down
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::EF_NONE},
       {ui::VKEY_VOLUME_DOWN, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::EF_CONTROL_DOWN},
       {ui::VKEY_VOLUME_DOWN, ui::EF_CONTROL_DOWN}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::EF_ALT_DOWN},
       {ui::VKEY_VOLUME_DOWN, ui::EF_ALT_DOWN}},
      // F10 -> Volume Up
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::EF_NONE},
       {ui::VKEY_VOLUME_UP, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::EF_CONTROL_DOWN},
       {ui::VKEY_VOLUME_UP, ui::EF_CONTROL_DOWN}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::EF_ALT_DOWN},
       {ui::VKEY_VOLUME_UP, ui::EF_ALT_DOWN}},
      // F11 -> F11
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::EF_NONE},
       {ui::VKEY_F11, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::EF_CONTROL_DOWN},
       {ui::VKEY_F11, ui::EF_CONTROL_DOWN}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::EF_ALT_DOWN},
       {ui::VKEY_F11, ui::EF_ALT_DOWN}},
      // F12 -> F12
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::EF_NONE},
       {ui::VKEY_F12, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::EF_CONTROL_DOWN},
       {ui::VKEY_F12, ui::EF_CONTROL_DOWN}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::EF_ALT_DOWN},
       {ui::VKEY_F12, ui::EF_ALT_DOWN}},

      // The number row should not be rewritten without Search key.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_1, ui::EF_NONE},
       {ui::VKEY_1, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_2, ui::EF_NONE},
       {ui::VKEY_2, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_3, ui::EF_NONE},
       {ui::VKEY_3, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_4, ui::EF_NONE},
       {ui::VKEY_4, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_5, ui::EF_NONE},
       {ui::VKEY_5, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_6, ui::EF_NONE},
       {ui::VKEY_6, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_7, ui::EF_NONE},
       {ui::VKEY_7, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_8, ui::EF_NONE},
       {ui::VKEY_8, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::EF_NONE},
       {ui::VKEY_9, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_0, ui::EF_NONE},
       {ui::VKEY_0, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_MINUS, ui::EF_NONE},
       {ui::VKEY_OEM_MINUS, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PLUS, ui::EF_NONE},
       {ui::VKEY_OEM_PLUS, ui::EF_NONE}},

      // The number row should be rewritten as the F<number> row with Search
      // key.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_1, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F1, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_2, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F2, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_3, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F3, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_4, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F4, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_5, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F5, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_6, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F6, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_7, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F7, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_8, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F8, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_9, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F9, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_0, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F10, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_MINUS, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F11, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_OEM_PLUS, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F12, ui::EF_NONE}},

      // The function keys should not be rewritten with Search key pressed.
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F1, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F1, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F2, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F2, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F3, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F3, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F4, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F4, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F5, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F5, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F6, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F6, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F7, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F7, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F8, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F8, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F9, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F9, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F10, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F10, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F11, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F11, ui::EF_NONE}},
      {KeyTestCase::TEST_ALL, ui::ET_KEY_PRESSED,
       {ui::VKEY_F12, ui::EF_COMMAND_DOWN},
       {ui::VKEY_F12, ui::EF_NONE}},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    CheckKeyTestCase(1000 + i, &rewriter, tests[i]);
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

  EventRewriter rewriter;
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
    CheckKeyTestCase(1000 + i, &rewriter, tests[i]);
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

  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  // Send left control press.
  std::string rewritten_event;
  {
    ui::ScopedXI2Event xev;
    xev.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, 0);
    XEvent* xevent = xev;
    xevent->xkey.keycode = XKeysymToKeycode(gfx::GetXDisplay(), XK_Control_L);
    xevent->xkey.send_event = True;  // XSendEvent() always does this.
    ui::KeyEvent keyevent(xev, false /* is_char */);
    scoped_ptr<ui::Event> new_event;
    // Control should NOT be remapped to Alt if send_event
    // flag in the event is True.
    EXPECT_EQ(ui::EVENT_REWRITE_CONTINUE,
              rewriter.RewriteEvent(keyevent, &new_event));
    EXPECT_FALSE(new_event);
  }
}

TEST_F(EventRewriterTest, TestRewriteNonNativeEvent) {
  // Remap Control to Alt.
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapControlKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kAltKey);

  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  const int kTouchId = 2;
  gfx::Point location(0, 0);
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, location, kTouchId, base::TimeDelta());
  press.set_flags(ui::EF_CONTROL_DOWN);

  scoped_ptr<ui::Event> new_event;
  rewriter.RewriteEvent(press, &new_event);
  EXPECT_TRUE(new_event);
  // Control should be remapped to Alt.
  EXPECT_EQ(ui::EF_ALT_DOWN,
            new_event->flags() & (ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
}

// Tests of event rewriting that depend on the Ash window manager.
class EventRewriterAshTest : public ash::test::AshTestBase {
 public:
  EventRewriterAshTest()
      : mock_user_manager_(new chromeos::MockUserManager),
        user_manager_enabler_(mock_user_manager_) {}
  virtual ~EventRewriterAshTest() {}

  bool RewriteFunctionKeys(const ui::Event& event,
                           scoped_ptr<ui::Event>* rewritten_event) {
    return rewriter_->RewriteEvent(event, rewritten_event);
  }

 protected:
  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    rewriter_.reset(new EventRewriter());
    chromeos::Preferences::RegisterProfilePrefs(prefs_.registry());
    rewriter_->set_pref_service_for_testing(&prefs_);
  }

  virtual void TearDown() OVERRIDE {
    rewriter_.reset();
    AshTestBase::TearDown();
  }

  TestingPrefServiceSyncable prefs_;

 private:
  scoped_ptr<EventRewriter> rewriter_;

  chromeos::MockUserManager* mock_user_manager_;  // Not owned.
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriterAshTest);
};

TEST_F(EventRewriterAshTest, TopRowKeysAreFunctionKeys) {
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(1));
  ash::wm::WindowState* window_state = ash::wm::GetWindowState(window.get());
  window_state->Activate();

  // Create a simulated keypress of F1 targetted at the window.
  ui::KeyEvent press_f1(ui::ET_KEY_PRESSED, ui::VKEY_F1, 0, false);

  // Simulate an apps v2 window that has requested top row keys as function
  // keys. The event should not be rewritten.
  window_state->set_top_row_keys_are_function_keys(true);
  scoped_ptr<ui::Event> rewritten_event;
  ASSERT_FALSE(RewriteFunctionKeys(press_f1, &rewritten_event));
  ASSERT_FALSE(rewritten_event);
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_F1, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetKeyEventAsString(press_f1));

  // The event should also not be rewritten if the send-function-keys pref is
  // additionally set, for both apps v2 and regular windows.
  BooleanPrefMember send_function_keys_pref;
  send_function_keys_pref.Init(prefs::kLanguageSendFunctionKeys, &prefs_);
  send_function_keys_pref.SetValue(true);
  window_state->set_top_row_keys_are_function_keys(false);
  ASSERT_FALSE(RewriteFunctionKeys(press_f1, &rewritten_event));
  ASSERT_FALSE(rewritten_event);
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_F1, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetKeyEventAsString(press_f1));

  // If the pref isn't set when an event is sent to a regular window, F1 is
  // rewritten to the back key.
  send_function_keys_pref.SetValue(false);
  ASSERT_TRUE(RewriteFunctionKeys(press_f1, &rewritten_event));
  ASSERT_TRUE(rewritten_event);
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_BROWSER_BACK, ui::EF_NONE, ui::ET_KEY_PRESSED),
            GetKeyEventAsString(
                *static_cast<const ui::KeyEvent*>(rewritten_event.get())));
}

TEST_F(EventRewriterTest, DontRewriteIfNotRewritten) {
  std::vector<unsigned int> device_list;
  device_list.push_back(10);
  device_list.push_back(11);
  ui::TouchFactory::GetInstance()->SetPointerDeviceForTest(device_list);
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);
  const int kLeftAndAltFlag = ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN;
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(
        10, ui::ET_MOUSE_PRESSED, gfx::Point(), kLeftAndAltFlag);
    ui::MouseEvent press(xev);
    // Sanity check.
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(kLeftAndAltFlag, press.flags());
    int flags = RewriteMouseEvent(&rewriter, press);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & flags);
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(
        10, ui::ET_MOUSE_RELEASED, gfx::Point(), kLeftAndAltFlag);
    ui::MouseEvent release(xev);
    int flags = RewriteMouseEvent(&rewriter, release);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & flags);
  }

  // No ALT in frst click.
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(
        10, ui::ET_MOUSE_PRESSED, gfx::Point(), ui::EF_LEFT_MOUSE_BUTTON);
    ui::MouseEvent press(xev);
    int flags = RewriteMouseEvent(&rewriter, press);
    EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & flags);
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(
        10, ui::ET_MOUSE_RELEASED, gfx::Point(), kLeftAndAltFlag);
    ui::MouseEvent release(xev);
    int flags = RewriteMouseEvent(&rewriter, release);
    EXPECT_TRUE((ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN) & flags);
  }

  // ALT on different device.
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(
        11, ui::ET_MOUSE_PRESSED, gfx::Point(), kLeftAndAltFlag);
    ui::MouseEvent press(xev);
    int flags = RewriteMouseEvent(&rewriter, press);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & flags);
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(
        10, ui::ET_MOUSE_RELEASED, gfx::Point(), kLeftAndAltFlag);
    ui::MouseEvent release(xev);
    int flags = RewriteMouseEvent(&rewriter, release);
    EXPECT_TRUE((ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN) & flags);
  }
  {
    ui::ScopedXI2Event xev;
    xev.InitGenericButtonEvent(
        11, ui::ET_MOUSE_RELEASED, gfx::Point(), kLeftAndAltFlag);
    ui::MouseEvent release(xev);
    int flags = RewriteMouseEvent(&rewriter, release);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & flags);
  }
}

}  // namespace chromeos
