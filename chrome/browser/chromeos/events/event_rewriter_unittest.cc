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

std::string GetKeyEventAsString(const ui::KeyEvent& keyevent) {
  return base::StringPrintf("ui_keycode=0x%X ui_flags=0x%X ui_type=%d",
                            keyevent.key_code(),
                            keyevent.flags(),
                            keyevent.type());
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

std::string GetExpectedResultAsString(ui::KeyboardCode ui_keycode,
                                      int ui_flags,
                                      ui::EventType ui_type) {
  return base::StringPrintf("ui_keycode=0x%X ui_flags=0x%X ui_type=%d",
                            ui_keycode,
                            ui_flags,
                            ui_type);
}

std::string GetRewrittenEventNumbered(size_t i,
                                      chromeos::EventRewriter* rewriter,
                                      ui::KeyboardCode ui_keycode,
                                      int ui_flags,
                                      ui::EventType ui_type) {
  return base::StringPrintf("(%zu) ", i) +
         GetRewrittenEventAsString(rewriter, ui_keycode, ui_flags, ui_type);
}

std::string GetExpectedResultNumbered(size_t i,
                                      ui::KeyboardCode ui_keycode,
                                      int ui_flags,
                                      ui::EventType ui_type) {
  return base::StringPrintf("(%zu) ", i) +
         GetExpectedResultAsString(ui_keycode, ui_flags, ui_type);
}

}  // namespace

namespace chromeos {

class EventRewriterTest : public ash::test::AshTestBase {
 public:
  EventRewriterTest()
      : display_(gfx::GetXDisplay()),
        mock_user_manager_(new chromeos::MockUserManager),
        user_manager_enabler_(mock_user_manager_),
        input_method_manager_mock_(NULL) {
  }
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

  // VKEY_A, Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_A, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_A, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED));

  // VKEY_A, Win modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_A, ui::EF_COMMAND_DOWN, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_A, ui::EF_COMMAND_DOWN, ui::ET_KEY_PRESSED));

  // VKEY_A, Alt+Win modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED));

  // VKEY_LWIN (left Windows key), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_LWIN,
                                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED));

  // VKEY_RWIN (right Windows key), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_RWIN,
                                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_RWIN,
                                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED));

  // An Apple keyboard reusing the ID, zero.
  rewriter.DeviceAddedForTesting(0, "Apple Keyboard");
  rewriter.set_last_device_id_for_testing(0);

  // VKEY_A, Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_A, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_A, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED));

  // VKEY_A, Win modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_A, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_A, ui::EF_COMMAND_DOWN, ui::ET_KEY_PRESSED));

  // VKEY_A, Alt+Win modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED));

  // VKEY_LWIN (left Windows key), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED));

  // VKEY_RWIN (right Windows key), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_RWIN,
                                      ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED));
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

  // Control should be remapped to Alt.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_MENU, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED));

  // An Apple keyboard reusing the ID, zero.
  rewriter.DeviceAddedForTesting(0, "Apple Keyboard");
  rewriter.set_last_device_id_for_testing(0);

  // VKEY_LWIN (left Command key) with  Alt modifier. The remapped Command key
  // should never be re-remapped to Alt.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED));

  // VKEY_RWIN (right Command key) with  Alt modifier. The remapped Command key
  // should never be re-remapped to Alt.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_RWIN,
                                      ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED));
}

void EventRewriterTest::TestRewriteNumPadKeys() {
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  // XK_KP_Insert (= NumPad 0 without Num Lock), no modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_NUMPAD0, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_INSERT, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED));

  // XK_KP_Insert (= NumPad 0 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD0,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_INSERT,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));

  // XK_KP_Delete (= NumPad . without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_DECIMAL,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_DELETE,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));

  // XK_KP_End (= NumPad 1 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD1,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_END,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));

  // XK_KP_Down (= NumPad 2 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD2,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_DOWN,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));

  // XK_KP_Next (= NumPad 3 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD3,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NEXT,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));

  // XK_KP_Left (= NumPad 4 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD4,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LEFT,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));

  // XK_KP_Begin (= NumPad 5 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD5,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CLEAR,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));

  // XK_KP_Right (= NumPad 6 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD6,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_RIGHT,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));

  // XK_KP_Home (= NumPad 7 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD7,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_HOME,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));

  // XK_KP_Up (= NumPad 8 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD8,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_UP,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));

  // XK_KP_Prior (= NumPad 9 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD9,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_PRIOR,
                                      ui::EF_ALT_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));

  // XK_KP_0 (= NumPad 0 with Num Lock), Num Lock modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_NUMPAD0, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_NUMPAD0, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED));

  // XK_KP_DECIMAL (= NumPad . with Num Lock), Num Lock modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_DECIMAL, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_DECIMAL, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED));

  // XK_KP_1 (= NumPad 1 with Num Lock), Num Lock modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_NUMPAD1, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_NUMPAD1, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED));

  // XK_KP_2 (= NumPad 2 with Num Lock), Num Lock modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_NUMPAD2, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_NUMPAD2, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED));

  // XK_KP_3 (= NumPad 3 with Num Lock), Num Lock modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_NUMPAD3, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_NUMPAD3, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED));

  // XK_KP_4 (= NumPad 4 with Num Lock), Num Lock modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_NUMPAD4, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_NUMPAD4, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED));

  // XK_KP_5 (= NumPad 5 with Num Lock), Num Lock modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_NUMPAD5, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_NUMPAD5, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED));

  // XK_KP_6 (= NumPad 6 with Num Lock), Num Lock modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_NUMPAD6, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_NUMPAD6, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED));

  // XK_KP_7 (= NumPad 7 with Num Lock), Num Lock modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_NUMPAD7, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_NUMPAD7, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED));

  // XK_KP_8 (= NumPad 8 with Num Lock), Num Lock modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_NUMPAD8, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_NUMPAD8, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED));

  // XK_KP_9 (= NumPad 9 with Num Lock), Num Lock modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_NUMPAD9, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_NUMPAD9, ui::EF_NUMPAD_KEY, ui::ET_KEY_PRESSED));
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

  // XK_KP_End (= NumPad 1 without Num Lock), Win modifier.
  // The result should be "Num Pad 1 with Control + Num Lock modifiers".
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD1,
                                      ui::EF_CONTROL_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_END,
                                      ui::EF_COMMAND_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));

  // XK_KP_1 (= NumPad 1 with Num Lock), Win modifier.
  // The result should also be "Num Pad 1 with Control + Num Lock modifiers".
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD1,
                                      ui::EF_CONTROL_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NUMPAD1,
                                      ui::EF_COMMAND_DOWN | ui::EF_NUMPAD_KEY,
                                      ui::ET_KEY_PRESSED));
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

  // Press Search. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_LWIN, ui::EF_COMMAND_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_LWIN, ui::EF_NONE, ui::ET_KEY_PRESSED));

  // Press left Control. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED));

  // Press right Control. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED));

  // Press left Alt. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_MENU, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_MENU, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED));

  // Press right Alt. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_MENU, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_MENU, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED));

  // Test KeyRelease event, just in case.
  // Release Search. Confirm the release event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_LWIN, ui::EF_NONE, ui::ET_KEY_RELEASED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_LWIN, ui::EF_NONE, ui::ET_KEY_RELEASED));
}

TEST_F(EventRewriterTest, TestRewriteModifiersNoRemapMultipleKeys) {
  TestingPrefServiceSyncable prefs;
  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  // Press Alt with Shift. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_MENU,
                                      ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_MENU,
                                      ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED));

  // Press Search with Caps Lock mask. Confirm the event is not rewritten.
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_LWIN,
                                ui::EF_CAPS_LOCK_DOWN | ui::EF_COMMAND_DOWN,
                                ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(&rewriter,
                                ui::VKEY_LWIN,
                                ui::EF_CAPS_LOCK_DOWN | ui::EF_COMMAND_DOWN,
                                ui::ET_KEY_PRESSED));

  // Release Search with Caps Lock mask. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_LWIN, ui::EF_CAPS_LOCK_DOWN, ui::ET_KEY_RELEASED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_CAPS_LOCK_DOWN,
                                      ui::ET_KEY_RELEASED));

  // Press Shift+Ctrl+Alt+Search+A. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_B,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_B,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED));
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

  // Press Alt with Shift. This key press shouldn't be affected by the
  // pref. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_MENU,
                                      ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_MENU,
                                      ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED));

  // Press Search. Confirm the event is now VKEY_UNKNOWN.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_UNKNOWN, ui::EF_NONE, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_LWIN, ui::EF_NONE, ui::ET_KEY_PRESSED));

  // Press Control. Confirm the event is now VKEY_UNKNOWN.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_UNKNOWN, ui::EF_NONE, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_CONTROL, ui::EF_NONE, ui::ET_KEY_PRESSED));

  // Press Control+Search. Confirm the event is now VKEY_UNKNOWN
  // without any modifiers.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_UNKNOWN, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_LWIN, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED));

  // Press Control+Search+a. Confirm the event is now VKEY_A without any
  // modifiers.
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_A, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED));

  // Press Control+Search+Alt+a. Confirm the event is now VKEY_A only with
  // the Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_A, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED));

  // Remap Alt to Control.
  IntegerPrefMember alt;
  alt.Init(prefs::kLanguageRemapAltKeyTo, &prefs);
  alt.SetValue(chromeos::input_method::kControlKey);

  // Press left Alt. Confirm the event is now VKEY_CONTROL
  // even though the Control key itself is disabled.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_MENU, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED));

  // Press Alt+a. Confirm the event is now Control+a even though the Control
  // key itself is disabled.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_A, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_A, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED));
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

  // Press Search. Confirm the event is now VKEY_CONTROL.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_LWIN, ui::EF_COMMAND_DOWN, ui::ET_KEY_PRESSED));

  // Remap Alt to Control too.
  IntegerPrefMember alt;
  alt.Init(prefs::kLanguageRemapAltKeyTo, &prefs);
  alt.SetValue(chromeos::input_method::kControlKey);

  // Press Alt. Confirm the event is now VKEY_CONTROL.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_MENU, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED));

  // Press Alt+Search. Confirm the event is now VKEY_CONTROL.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED));

  // Press Control+Alt+Search. Confirm the event is now VKEY_CONTROL.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter,
                ui::VKEY_LWIN,
                ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                ui::ET_KEY_PRESSED));

  // Press Shift+Control+Alt+Search. Confirm the event is now Control with
  // Shift and Control modifiers.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED));

  // Press Shift+Control+Alt+Search+B. Confirm the event is now B with Shift
  // and Control modifiers.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_B,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_B,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED));
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

  // Press Search. Confirm the event is now VKEY_ESCAPE.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_ESCAPE, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_LWIN, ui::EF_COMMAND_DOWN, ui::ET_KEY_PRESSED));
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

  // Press Search. Confirm the event is now VKEY_MENU.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_MENU, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_LWIN, ui::EF_COMMAND_DOWN, ui::ET_KEY_PRESSED));

  // Remap Alt to Control.
  IntegerPrefMember alt;
  alt.Init(prefs::kLanguageRemapAltKeyTo, &prefs);
  alt.SetValue(chromeos::input_method::kControlKey);

  // Press left Alt. Confirm the event is now VKEY_CONTROL.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_MENU, ui::EF_ALT_DOWN, ui::ET_KEY_PRESSED));

  // Remap Control to Search.
  IntegerPrefMember control;
  control.Init(prefs::kLanguageRemapControlKeyTo, &prefs);
  control.SetValue(chromeos::input_method::kSearchKey);

  // Press left Control. Confirm the event is now VKEY_LWIN.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_LWIN, ui::EF_COMMAND_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED));

  // Then, press all of the three, Control+Alt+Search.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_MENU,
                ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter,
                ui::VKEY_LWIN,
                ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                ui::ET_KEY_PRESSED));

  // Press Shift+Control+Alt+Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_MENU,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED));

  // Press Shift+Control+Alt+Search+B
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_B,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_B,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                      ui::ET_KEY_PRESSED));
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

  // F15 should work as Ctrl when --has-chromeos-diamond-key is not specified.
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter, ui::VKEY_F15, ui::EF_NONE, ui::ET_KEY_PRESSED));

  // However, Mod2Mask should not be rewritten to CtrlMask when
  // --has-chromeos-diamond-key is not specified.
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_A, ui::EF_NONE, ui::ET_KEY_PRESSED));
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

  // Press CapsLock+a. Confirm that Mod3Mask is rewritten to ControlMask.
  // On Chrome OS, CapsLock works as a Mod3 modifier.
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_A, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_A, ui::EF_MOD3_DOWN, ui::ET_KEY_PRESSED));

  // Press Control+CapsLock+a. Confirm that Mod3Mask is rewritten to ControlMask
  EXPECT_EQ(
      GetExpectedResultAsString(
          ui::VKEY_A, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(
          &rewriter, ui::VKEY_A, ui::EF_CONTROL_DOWN, ui::ET_KEY_PRESSED));

  // Press Alt+CapsLock+a. Confirm that Mod3Mask is rewritten to ControlMask.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      ui::EF_ALT_DOWN | ui::EF_MOD3_DOWN,
                                      ui::ET_KEY_PRESSED));
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

  struct {
    ui::KeyboardCode input;
    unsigned int input_mods;
    ui::KeyboardCode output;
    unsigned int output_mods;
  } chromeos_tests[] = {
        // Alt+Backspace -> Delete
        {ui::VKEY_BACK, ui::EF_ALT_DOWN, ui::VKEY_DELETE, ui::EF_NONE},
        // Control+Alt+Backspace -> Control+Delete
        {ui::VKEY_BACK, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::VKEY_DELETE,
         ui::EF_CONTROL_DOWN},
        // Search+Alt+Backspace -> Alt+Backspace
        {ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::VKEY_BACK,
         ui::EF_ALT_DOWN},
        // Search+Control+Alt+Backspace -> Control+Alt+Backspace
        {ui::VKEY_BACK,
         ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
         ui::VKEY_BACK, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN},
        // Alt+Up -> Prior
        {ui::VKEY_UP, ui::EF_ALT_DOWN, ui::VKEY_PRIOR, ui::EF_NONE},
        // Alt+Down -> Next
        {ui::VKEY_DOWN, ui::EF_ALT_DOWN, ui::VKEY_NEXT, ui::EF_NONE},
        // Ctrl+Alt+Up -> Home
        {ui::VKEY_UP, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::VKEY_HOME,
         ui::EF_NONE},
        // Ctrl+Alt+Down -> End
        {ui::VKEY_DOWN, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::VKEY_END,
         ui::EF_NONE},

        // Search+Alt+Up -> Alt+Up
        {ui::VKEY_UP, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::VKEY_UP,
         ui::EF_ALT_DOWN},
        // Search+Alt+Down -> Alt+Down
        {ui::VKEY_DOWN, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, ui::VKEY_DOWN,
         ui::EF_ALT_DOWN},
        // Search+Ctrl+Alt+Up -> Search+Ctrl+Alt+Up
        {ui::VKEY_UP,
         ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
         ui::VKEY_UP, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN},
        // Search+Ctrl+Alt+Down -> Ctrl+Alt+Down
        {ui::VKEY_DOWN,
         ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
         ui::VKEY_DOWN, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN},

        // Period -> Period
        {ui::VKEY_OEM_PERIOD, ui::EF_NONE, ui::VKEY_OEM_PERIOD, ui::EF_NONE},

        // Search+Backspace -> Delete
        {ui::VKEY_BACK, ui::EF_COMMAND_DOWN, ui::VKEY_DELETE, ui::EF_NONE},
        // Search+Up -> Prior
        {ui::VKEY_UP, ui::EF_COMMAND_DOWN, ui::VKEY_PRIOR, ui::EF_NONE},
        // Search+Down -> Next
        {ui::VKEY_DOWN, ui::EF_COMMAND_DOWN, ui::VKEY_NEXT, ui::EF_NONE},
        // Search+Left -> Home
        {ui::VKEY_LEFT, ui::EF_COMMAND_DOWN, ui::VKEY_HOME, ui::EF_NONE},
        // Control+Search+Left -> Home
        {ui::VKEY_LEFT, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
         ui::VKEY_HOME, ui::EF_CONTROL_DOWN},
        // Search+Right -> End
        {ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN, ui::VKEY_END, ui::EF_NONE},
        // Control+Search+Right -> End
        {ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
         ui::VKEY_END, ui::EF_CONTROL_DOWN},
        // Search+Period -> Insert
        {ui::VKEY_OEM_PERIOD, ui::EF_COMMAND_DOWN, ui::VKEY_INSERT,
         ui::EF_NONE},
        // Control+Search+Period -> Control+Insert
        {ui::VKEY_OEM_PERIOD, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
         ui::VKEY_INSERT, ui::EF_CONTROL_DOWN}};

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(chromeos_tests); ++i) {
    EXPECT_EQ(GetExpectedResultNumbered(i,
                                        chromeos_tests[i].output,
                                        chromeos_tests[i].output_mods,
                                        ui::ET_KEY_PRESSED),
              GetRewrittenEventNumbered(i,
                                        &rewriter,
                                        chromeos_tests[i].input,
                                        chromeos_tests[i].input_mods,
                                        ui::ET_KEY_PRESSED));
  }
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeys) {
  TestingPrefServiceSyncable prefs;
  chromeos::Preferences::RegisterProfilePrefs(prefs.registry());
  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  struct {
    ui::KeyboardCode input;
    unsigned int input_mods;
    ui::KeyboardCode output;
    unsigned int output_mods;
  } tests[] = {
        // F1 -> Back
        {ui::VKEY_F1, ui::EF_NONE, ui::VKEY_BROWSER_BACK, ui::EF_NONE},
        {ui::VKEY_F1, ui::EF_CONTROL_DOWN, ui::VKEY_BROWSER_BACK,
         ui::EF_CONTROL_DOWN},
        {ui::VKEY_F1, ui::EF_ALT_DOWN, ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN},
        // F2 -> Forward
        {ui::VKEY_F2, ui::EF_NONE, ui::VKEY_BROWSER_FORWARD, ui::EF_NONE},
        {ui::VKEY_F2, ui::EF_CONTROL_DOWN, ui::VKEY_BROWSER_FORWARD,
         ui::EF_CONTROL_DOWN},
        {ui::VKEY_F2, ui::EF_ALT_DOWN, ui::VKEY_BROWSER_FORWARD,
         ui::EF_ALT_DOWN},
        // F3 -> Refresh
        {ui::VKEY_F3, ui::EF_NONE, ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
        {ui::VKEY_F3, ui::EF_CONTROL_DOWN, ui::VKEY_BROWSER_REFRESH,
         ui::EF_CONTROL_DOWN},
        {ui::VKEY_F3, ui::EF_ALT_DOWN, ui::VKEY_BROWSER_REFRESH,
         ui::EF_ALT_DOWN},
        // F4 -> Launch App 2
        {ui::VKEY_F4, ui::EF_NONE, ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_NONE},
        {ui::VKEY_F4, ui::EF_CONTROL_DOWN, ui::VKEY_MEDIA_LAUNCH_APP2,
         ui::EF_CONTROL_DOWN},
        {ui::VKEY_F4, ui::EF_ALT_DOWN, ui::VKEY_MEDIA_LAUNCH_APP2,
         ui::EF_ALT_DOWN},
        // F5 -> Launch App 1
        {ui::VKEY_F5, ui::EF_NONE, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE},
        {ui::VKEY_F5, ui::EF_CONTROL_DOWN, ui::VKEY_MEDIA_LAUNCH_APP1,
         ui::EF_CONTROL_DOWN},
        {ui::VKEY_F5, ui::EF_ALT_DOWN, ui::VKEY_MEDIA_LAUNCH_APP1,
         ui::EF_ALT_DOWN},
        // F6 -> Brightness down
        {ui::VKEY_F6, ui::EF_NONE, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE},
        {ui::VKEY_F6, ui::EF_CONTROL_DOWN, ui::VKEY_BRIGHTNESS_DOWN,
         ui::EF_CONTROL_DOWN},
        {ui::VKEY_F6, ui::EF_ALT_DOWN, ui::VKEY_BRIGHTNESS_DOWN,
         ui::EF_ALT_DOWN},
        // F7 -> Brightness up
        {ui::VKEY_F7, ui::EF_NONE, ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE},
        {ui::VKEY_F7, ui::EF_CONTROL_DOWN, ui::VKEY_BRIGHTNESS_UP,
         ui::EF_CONTROL_DOWN},
        {ui::VKEY_F7, ui::EF_ALT_DOWN, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN},
        // F8 -> Volume Mute
        {ui::VKEY_F8, ui::EF_NONE, ui::VKEY_VOLUME_MUTE, ui::EF_NONE},
        {ui::VKEY_F8, ui::EF_CONTROL_DOWN, ui::VKEY_VOLUME_MUTE,
         ui::EF_CONTROL_DOWN},
        {ui::VKEY_F8, ui::EF_ALT_DOWN, ui::VKEY_VOLUME_MUTE, ui::EF_ALT_DOWN},
        // F9 -> Volume Down
        {ui::VKEY_F9, ui::EF_NONE, ui::VKEY_VOLUME_DOWN, ui::EF_NONE},
        {ui::VKEY_F9, ui::EF_CONTROL_DOWN, ui::VKEY_VOLUME_DOWN,
         ui::EF_CONTROL_DOWN},
        {ui::VKEY_F9, ui::EF_ALT_DOWN, ui::VKEY_VOLUME_DOWN, ui::EF_ALT_DOWN},
        // F10 -> Volume Up
        {ui::VKEY_F10, ui::EF_NONE, ui::VKEY_VOLUME_UP, ui::EF_NONE},
        {ui::VKEY_F10, ui::EF_CONTROL_DOWN, ui::VKEY_VOLUME_UP,
         ui::EF_CONTROL_DOWN},
        {ui::VKEY_F10, ui::EF_ALT_DOWN, ui::VKEY_VOLUME_UP, ui::EF_ALT_DOWN},
        // F11 -> F11
        {ui::VKEY_F11, ui::EF_NONE, ui::VKEY_F11, ui::EF_NONE},
        {ui::VKEY_F11, ui::EF_CONTROL_DOWN, ui::VKEY_F11, ui::EF_CONTROL_DOWN},
        {ui::VKEY_F11, ui::EF_ALT_DOWN, ui::VKEY_F11, ui::EF_ALT_DOWN},
        // F12 -> F12
        {ui::VKEY_F12, ui::EF_NONE, ui::VKEY_F12, ui::EF_NONE},
        {ui::VKEY_F12, ui::EF_CONTROL_DOWN, ui::VKEY_F12, ui::EF_CONTROL_DOWN},
        {ui::VKEY_F12, ui::EF_ALT_DOWN, ui::VKEY_F12, ui::EF_ALT_DOWN},

        // The number row should not be rewritten without Search key.
        {ui::VKEY_1, ui::EF_NONE, ui::VKEY_1, ui::EF_NONE},
        {ui::VKEY_2, ui::EF_NONE, ui::VKEY_2, ui::EF_NONE},
        {ui::VKEY_3, ui::EF_NONE, ui::VKEY_3, ui::EF_NONE},
        {ui::VKEY_4, ui::EF_NONE, ui::VKEY_4, ui::EF_NONE},
        {ui::VKEY_5, ui::EF_NONE, ui::VKEY_5, ui::EF_NONE},
        {ui::VKEY_6, ui::EF_NONE, ui::VKEY_6, ui::EF_NONE},
        {ui::VKEY_7, ui::EF_NONE, ui::VKEY_7, ui::EF_NONE},
        {ui::VKEY_8, ui::EF_NONE, ui::VKEY_8, ui::EF_NONE},
        {ui::VKEY_9, ui::EF_NONE, ui::VKEY_9, ui::EF_NONE},
        {ui::VKEY_0, ui::EF_NONE, ui::VKEY_0, ui::EF_NONE},
        {ui::VKEY_OEM_MINUS, ui::EF_NONE, ui::VKEY_OEM_MINUS, ui::EF_NONE},
        {ui::VKEY_OEM_PLUS, ui::EF_NONE, ui::VKEY_OEM_PLUS, ui::EF_NONE},

        // The number row should be rewritten as the F<number> row with Search
        // key.
        {ui::VKEY_1, ui::EF_COMMAND_DOWN, ui::VKEY_F1, ui::EF_NONE},
        {ui::VKEY_2, ui::EF_COMMAND_DOWN, ui::VKEY_F2, ui::EF_NONE},
        {ui::VKEY_3, ui::EF_COMMAND_DOWN, ui::VKEY_F3, ui::EF_NONE},
        {ui::VKEY_4, ui::EF_COMMAND_DOWN, ui::VKEY_F4, ui::EF_NONE},
        {ui::VKEY_5, ui::EF_COMMAND_DOWN, ui::VKEY_F5, ui::EF_NONE},
        {ui::VKEY_6, ui::EF_COMMAND_DOWN, ui::VKEY_F6, ui::EF_NONE},
        {ui::VKEY_7, ui::EF_COMMAND_DOWN, ui::VKEY_F7, ui::EF_NONE},
        {ui::VKEY_8, ui::EF_COMMAND_DOWN, ui::VKEY_F8, ui::EF_NONE},
        {ui::VKEY_9, ui::EF_COMMAND_DOWN, ui::VKEY_F9, ui::EF_NONE},
        {ui::VKEY_0, ui::EF_COMMAND_DOWN, ui::VKEY_F10, ui::EF_NONE},
        {ui::VKEY_OEM_MINUS, ui::EF_COMMAND_DOWN, ui::VKEY_F11, ui::EF_NONE},
        {ui::VKEY_OEM_PLUS, ui::EF_COMMAND_DOWN, ui::VKEY_F12, ui::EF_NONE},

        // The function keys should not be rewritten with Search key pressed.
        {ui::VKEY_F1, ui::EF_COMMAND_DOWN, ui::VKEY_F1, ui::EF_NONE},
        {ui::VKEY_F2, ui::EF_COMMAND_DOWN, ui::VKEY_F2, ui::EF_NONE},
        {ui::VKEY_F3, ui::EF_COMMAND_DOWN, ui::VKEY_F3, ui::EF_NONE},
        {ui::VKEY_F4, ui::EF_COMMAND_DOWN, ui::VKEY_F4, ui::EF_NONE},
        {ui::VKEY_F5, ui::EF_COMMAND_DOWN, ui::VKEY_F5, ui::EF_NONE},
        {ui::VKEY_F6, ui::EF_COMMAND_DOWN, ui::VKEY_F6, ui::EF_NONE},
        {ui::VKEY_F7, ui::EF_COMMAND_DOWN, ui::VKEY_F7, ui::EF_NONE},
        {ui::VKEY_F8, ui::EF_COMMAND_DOWN, ui::VKEY_F8, ui::EF_NONE},
        {ui::VKEY_F9, ui::EF_COMMAND_DOWN, ui::VKEY_F9, ui::EF_NONE},
        {ui::VKEY_F10, ui::EF_COMMAND_DOWN, ui::VKEY_F10, ui::EF_NONE},
        {ui::VKEY_F11, ui::EF_COMMAND_DOWN, ui::VKEY_F11, ui::EF_NONE},
        {ui::VKEY_F12, ui::EF_COMMAND_DOWN, ui::VKEY_F12, ui::EF_NONE},
    };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    EXPECT_EQ(GetExpectedResultNumbered(
                  i, tests[i].output, tests[i].output_mods, ui::ET_KEY_PRESSED),
              GetRewrittenEventNumbered(i,
                                        &rewriter,
                                        tests[i].input,
                                        tests[i].input_mods,
                                        ui::ET_KEY_PRESSED));
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

  // Alt+Search+Down -> End
  EXPECT_EQ(
      GetExpectedResultAsString(ui::VKEY_END, ui::EF_NONE, ui::ET_KEY_PRESSED),
      GetRewrittenEventAsString(&rewriter,
                                ui::VKEY_DOWN,
                                ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                                ui::ET_KEY_PRESSED));

  // Shift+Alt+Search+Down -> Shift+End
  EXPECT_EQ(GetExpectedResultAsString(
                ui::VKEY_END, ui::EF_SHIFT_DOWN, ui::ET_KEY_PRESSED),
            GetRewrittenEventAsString(
                &rewriter,
                ui::VKEY_DOWN,
                ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
                ui::ET_KEY_PRESSED));

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
