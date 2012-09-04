// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/event_rewriter.h"

#include "base/basictypes.h"
#include "base/stringprintf.h"
#include "chrome/browser/api/prefs/pref_member.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/events/event.h"

#if defined(OS_CHROMEOS)
#include <X11/keysym.h>
#include <X11/Xlib.h>

#include "chrome/browser/chromeos/input_method/mock_xkeyboard.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/preferences.h"
#include "ui/base/x/x11_util.h"

namespace {

void InitXKeyEvent(ui::KeyboardCode ui_keycode,
                   int ui_flags,
                   ui::EventType ui_type,
                   KeyCode x_keycode,
                   unsigned int x_state,
                   XEvent* event) {
  ui::InitXKeyEventForTesting(ui_type,
                              ui_keycode,
                              ui_flags,
                              event);
  event->xkey.keycode = x_keycode;
  event->xkey.state = x_state;
}

std::string GetRewrittenEventAsString(EventRewriter* rewriter,
                                      ui::KeyboardCode ui_keycode,
                                      int ui_flags,
                                      ui::EventType ui_type,
                                      KeyCode x_keycode,
                                      unsigned int x_state) {
  XEvent xev;
  InitXKeyEvent(ui_keycode, ui_flags, ui_type, x_keycode, x_state, &xev);
  ui::KeyEvent keyevent(&xev, false /* is_char */);
  rewriter->RewriteForTesting(&keyevent);
  return StringPrintf(
      "ui_keycode=%d ui_flags=%d ui_type=%d x_keycode=%u x_state=%u x_type=%d",
      keyevent.key_code(), keyevent.flags(), keyevent.type(),
      xev.xkey.keycode, xev.xkey.state, xev.xkey.type);
}

std::string GetExpectedResultAsString(ui::KeyboardCode ui_keycode,
                                      int ui_flags,
                                      ui::EventType ui_type,
                                      KeyCode x_keycode,
                                      unsigned int x_state,
                                      int x_type) {
  return StringPrintf(
      "ui_keycode=%d ui_flags=%d ui_type=%d x_keycode=%u x_state=%u x_type=%d",
      ui_keycode, ui_flags, ui_type, x_keycode, x_state, x_type);
}

class EventRewriterTest : public testing::Test {
 public:
  EventRewriterTest()
      : display_(ui::GetXDisplay()),
        keycode_a_(XKeysymToKeycode(display_, XK_a)),
        keycode_alt_l_(XKeysymToKeycode(display_, XK_Alt_L)),
        keycode_alt_r_(XKeysymToKeycode(display_, XK_Alt_R)),
        keycode_b_(XKeysymToKeycode(display_, XK_B)),
        keycode_caps_lock_(XKeysymToKeycode(display_, XK_Caps_Lock)),
        keycode_control_l_(XKeysymToKeycode(display_, XK_Control_L)),
        keycode_control_r_(XKeysymToKeycode(display_, XK_Control_R)),
        keycode_meta_l_(XKeysymToKeycode(display_, XK_Meta_L)),
        keycode_meta_r_(XKeysymToKeycode(display_, XK_Meta_R)),
        keycode_num_pad_0_(XKeysymToKeycode(display_, XK_KP_0)),
        keycode_num_pad_1_(XKeysymToKeycode(display_, XK_KP_1)),
        keycode_num_pad_2_(XKeysymToKeycode(display_, XK_KP_2)),
        keycode_num_pad_3_(XKeysymToKeycode(display_, XK_KP_3)),
        keycode_num_pad_4_(XKeysymToKeycode(display_, XK_KP_4)),
        keycode_num_pad_5_(XKeysymToKeycode(display_, XK_KP_5)),
        keycode_num_pad_6_(XKeysymToKeycode(display_, XK_KP_6)),
        keycode_num_pad_7_(XKeysymToKeycode(display_, XK_KP_7)),
        keycode_num_pad_8_(XKeysymToKeycode(display_, XK_KP_8)),
        keycode_num_pad_9_(XKeysymToKeycode(display_, XK_KP_9)),
        keycode_num_pad_begin_(XKeysymToKeycode(display_, XK_KP_Begin)),
        keycode_num_pad_decimal_(XKeysymToKeycode(display_, XK_KP_Decimal)),
        keycode_num_pad_delete_(XKeysymToKeycode(display_, XK_KP_Delete)),
        keycode_num_pad_down_(XKeysymToKeycode(display_, XK_KP_Down)),
        keycode_num_pad_end_(XKeysymToKeycode(display_, XK_KP_End)),
        keycode_num_pad_home_(XKeysymToKeycode(display_, XK_KP_Home)),
        keycode_num_pad_insert_(XKeysymToKeycode(display_, XK_KP_Insert)),
        keycode_num_pad_left_(XKeysymToKeycode(display_, XK_KP_Left)),
        keycode_num_pad_next_(XKeysymToKeycode(display_, XK_KP_Next)),
        keycode_num_pad_prior_(XKeysymToKeycode(display_, XK_KP_Prior)),
        keycode_num_pad_right_(XKeysymToKeycode(display_, XK_KP_Right)),
        keycode_num_pad_up_(XKeysymToKeycode(display_, XK_KP_Up)),
        keycode_super_l_(XKeysymToKeycode(display_, XK_Super_L)),
        keycode_super_r_(XKeysymToKeycode(display_, XK_Super_R)),
        keycode_void_symbol_(XKeysymToKeycode(display_, XK_VoidSymbol)),
        keycode_delete_(XKeysymToKeycode(display_, XK_Delete)),
        keycode_backspace_(XKeysymToKeycode(display_, XK_BackSpace)),
        keycode_up_(XKeysymToKeycode(display_, XK_Up)),
        keycode_down_(XKeysymToKeycode(display_, XK_Down)),
        keycode_prior_(XKeysymToKeycode(display_, XK_Prior)),
        keycode_next_(XKeysymToKeycode(display_, XK_Next)),
        keycode_home_(XKeysymToKeycode(display_, XK_Home)),
        keycode_end_(XKeysymToKeycode(display_, XK_End)) {
  }
  virtual ~EventRewriterTest() {}
  virtual void SetUp() {
    // Mocking user manager because the real one needs to be called on UI thread
    EXPECT_CALL(*user_manager_mock_.user_manager(), IsLoggedInAsGuest())
        .WillRepeatedly(testing::Return(false));
  }

 protected:
  Display* display_;
  const KeyCode keycode_a_;
  const KeyCode keycode_alt_l_;
  const KeyCode keycode_alt_r_;
  const KeyCode keycode_b_;
  const KeyCode keycode_caps_lock_;
  const KeyCode keycode_control_l_;
  const KeyCode keycode_control_r_;
  const KeyCode keycode_meta_l_;
  const KeyCode keycode_meta_r_;
  const KeyCode keycode_num_pad_0_;
  const KeyCode keycode_num_pad_1_;
  const KeyCode keycode_num_pad_2_;
  const KeyCode keycode_num_pad_3_;
  const KeyCode keycode_num_pad_4_;
  const KeyCode keycode_num_pad_5_;
  const KeyCode keycode_num_pad_6_;
  const KeyCode keycode_num_pad_7_;
  const KeyCode keycode_num_pad_8_;
  const KeyCode keycode_num_pad_9_;
  const KeyCode keycode_num_pad_begin_;
  const KeyCode keycode_num_pad_decimal_;
  const KeyCode keycode_num_pad_delete_;
  const KeyCode keycode_num_pad_down_;
  const KeyCode keycode_num_pad_end_;
  const KeyCode keycode_num_pad_home_;
  const KeyCode keycode_num_pad_insert_;
  const KeyCode keycode_num_pad_left_;
  const KeyCode keycode_num_pad_next_;
  const KeyCode keycode_num_pad_prior_;
  const KeyCode keycode_num_pad_right_;
  const KeyCode keycode_num_pad_up_;
  const KeyCode keycode_super_l_;
  const KeyCode keycode_super_r_;
  const KeyCode keycode_void_symbol_;
  const KeyCode keycode_delete_;
  const KeyCode keycode_backspace_;
  const KeyCode keycode_up_;
  const KeyCode keycode_down_;
  const KeyCode keycode_prior_;
  const KeyCode keycode_next_;
  const KeyCode keycode_home_;
  const KeyCode keycode_end_;
  chromeos::ScopedMockUserManagerEnabler user_manager_mock_;
};

}  // namespace
#else
class EventRewriterTest : public testing::Test {
 public:
  EventRewriterTest() {}
  virtual ~EventRewriterTest() {}
};
#endif

TEST_F(EventRewriterTest, TestGetDeviceType) {
  // This is the typical string which an Apple keyboard sends.
  EXPECT_EQ(EventRewriter::kDeviceAppleKeyboard,
            EventRewriter::GetDeviceType("Apple Inc. Apple Keyboard"));

  // Other cases we accept.
  EXPECT_EQ(EventRewriter::kDeviceAppleKeyboard,
            EventRewriter::GetDeviceType("Apple Keyboard"));
  EXPECT_EQ(EventRewriter::kDeviceAppleKeyboard,
            EventRewriter::GetDeviceType("apple keyboard"));
  EXPECT_EQ(EventRewriter::kDeviceAppleKeyboard,
            EventRewriter::GetDeviceType("apple keyboard."));
  EXPECT_EQ(EventRewriter::kDeviceAppleKeyboard,
            EventRewriter::GetDeviceType("apple.keyboard."));
  EXPECT_EQ(EventRewriter::kDeviceAppleKeyboard,
            EventRewriter::GetDeviceType(".apple.keyboard."));

  // Dell, Microsoft, Logitech, ... should be recognized as a kDeviceUnknown.
  EXPECT_EQ(EventRewriter::kDeviceUnknown,
            EventRewriter::GetDeviceType("Dell Dell USB Entry Keyboard"));
  EXPECT_EQ(EventRewriter::kDeviceUnknown,
            EventRewriter::GetDeviceType(
                "Microsoft Natural Ergonomic Keyboard"));
  EXPECT_EQ(EventRewriter::kDeviceUnknown,
            EventRewriter::GetDeviceType("CHESEN USB Keyboard"));

  // Some corner cases.
  EXPECT_EQ(EventRewriter::kDeviceUnknown, EventRewriter::GetDeviceType(""));
  EXPECT_EQ(EventRewriter::kDeviceUnknown,
            EventRewriter::GetDeviceType("."));
  EXPECT_EQ(EventRewriter::kDeviceUnknown,
            EventRewriter::GetDeviceType(". "));
  EXPECT_EQ(EventRewriter::kDeviceUnknown,
            EventRewriter::GetDeviceType(" ."));
  EXPECT_EQ(EventRewriter::kDeviceUnknown,
            EventRewriter::GetDeviceType("not-an-apple keyboard"));
}

TEST_F(EventRewriterTest, TestDeviceAddedOrRemoved) {
  EventRewriter rewriter;
  EXPECT_TRUE(rewriter.device_id_to_type_for_testing().empty());
  EXPECT_EQ(EventRewriter::kDeviceUnknown,
            rewriter.DeviceAddedForTesting(0, "PC Keyboard"));
  EXPECT_EQ(1U, rewriter.device_id_to_type_for_testing().size());
  EXPECT_EQ(EventRewriter::kDeviceAppleKeyboard,
            rewriter.DeviceAddedForTesting(1, "Apple Keyboard"));
  EXPECT_EQ(2U, rewriter.device_id_to_type_for_testing().size());
  // Try to reuse the first ID.
  EXPECT_EQ(EventRewriter::kDeviceAppleKeyboard,
            rewriter.DeviceAddedForTesting(0, "Apple Keyboard"));
  EXPECT_EQ(2U, rewriter.device_id_to_type_for_testing().size());
}

#if defined(OS_CHROMEOS)
TEST_F(EventRewriterTest, TestRewriteCommandToControl) {
  // First, test with a PC keyboard.
  TestingPrefService prefs;
  EventRewriter rewriter;
  rewriter.DeviceAddedForTesting(0, "PC Keyboard");
  rewriter.set_last_device_id_for_testing(0);
  rewriter.set_pref_service_for_testing(&prefs);

  // XK_a, Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod1Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod1Mask));

  // XK_a, Win modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod4Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod4Mask));

  // XK_a, Alt+Win modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod1Mask | Mod4Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod1Mask | Mod4Mask));

  // XK_Super_L (left Windows key), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_LWIN,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      Mod1Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      Mod1Mask));

  // XK_Super_R (right Windows key), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_RWIN,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_r_,
                                      Mod1Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_RWIN,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_r_,
                                      Mod1Mask));

  // An Apple keyboard reusing the ID, zero.
  rewriter.DeviceAddedForTesting(0, "Apple Keyboard");
  rewriter.set_last_device_id_for_testing(0);

  // XK_a, Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod1Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod1Mask));

  // XK_a, Win modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      ControlMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod4Mask));

  // XK_a, Alt+Win modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod1Mask | ControlMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod1Mask | Mod4Mask));

  // XK_Super_L (left Windows key), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      Mod1Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      Mod1Mask));

  // XK_Super_R (right Windows key), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_r_,
                                      Mod1Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_RWIN,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_r_,
                                      Mod1Mask));
}

// For crbug.com/133896.
TEST_F(EventRewriterTest, TestRewriteCommandToControlWithControlRemapped) {
  // Remap Control to Alt.
  TestingPrefService prefs;
  chromeos::Preferences::RegisterUserPrefs(&prefs);
  IntegerPrefMember control;
  control.Init(prefs::kLanguageXkbRemapControlKeyTo, &prefs, NULL);
  control.SetValue(chromeos::input_method::kAltKey);

  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.DeviceAddedForTesting(0, "PC Keyboard");
  rewriter.set_last_device_id_for_testing(0);

  // XK_Control_L (left Control key) should be remapped to Alt.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_MENU,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_alt_l_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CONTROL,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      0U));

  // An Apple keyboard reusing the ID, zero.
  rewriter.DeviceAddedForTesting(0, "Apple Keyboard");
  rewriter.set_last_device_id_for_testing(0);

  // XK_Super_L (left Command key) with  Alt modifier. The remapped Command key
  // should never be re-remapped to Alt.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      Mod1Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      Mod1Mask));

  // XK_Super_R (right Command key) with  Alt modifier. The remapped Command key
  // should never be re-remapped to Alt.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_r_,
                                      Mod1Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_RWIN,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_r_,
                                      Mod1Mask));
}

TEST_F(EventRewriterTest, TestRewriteNumPadKeys) {
  TestingPrefService prefs;
  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  // XK_KP_Insert (= NumPad 0 without Num Lock), no modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD0,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_0_,
                                      Mod2Mask,  // Num Lock
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_INSERT,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_insert_,
                                      0));

  // XK_KP_Insert (= NumPad 0 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD0,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_0_,
                                      Mod1Mask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_INSERT,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_insert_,
                                      Mod1Mask));

  // XK_KP_Delete (= NumPad . without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_DECIMAL,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_decimal_,
                                      Mod1Mask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_DELETE,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_delete_,
                                      Mod1Mask));

  // XK_KP_End (= NumPad 1 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD1,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_1_,
                                      Mod1Mask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_END,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_end_,
                                      Mod1Mask));

  // XK_KP_Down (= NumPad 2 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD2,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_2_,
                                      Mod1Mask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_DOWN,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_down_,
                                      Mod1Mask));

  // XK_KP_Next (= NumPad 3 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD3,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_3_,
                                      Mod1Mask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NEXT,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_next_,
                                      Mod1Mask));

  // XK_KP_Left (= NumPad 4 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD4,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_4_,
                                      Mod1Mask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LEFT,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_left_,
                                      Mod1Mask));

  // XK_KP_Begin (= NumPad 5 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD5,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_5_,
                                      Mod1Mask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CLEAR,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_begin_,
                                      Mod1Mask));

  // XK_KP_Right (= NumPad 6 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD6,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_6_,
                                      Mod1Mask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_RIGHT,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_right_,
                                      Mod1Mask));

  // XK_KP_Home (= NumPad 7 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD7,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_7_,
                                      Mod1Mask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_HOME,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_home_,
                                      Mod1Mask));

  // XK_KP_Up (= NumPad 8 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD8,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_8_,
                                      Mod1Mask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_UP,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_up_,
                                      Mod1Mask));

  // XK_KP_Prior (= NumPad 9 without Num Lock), Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD9,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_9_,
                                      Mod1Mask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_PRIOR,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_prior_,
                                      Mod1Mask));

  // XK_KP_0 (= NumPad 0 with Num Lock), Num Lock modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD0,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_0_,
                                      Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NUMPAD0,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_0_,
                                      Mod2Mask));

  // XK_KP_DECIMAL (= NumPad . with Num Lock), Num Lock modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_DECIMAL,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_decimal_,
                                      Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_DECIMAL,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_decimal_,
                                      Mod2Mask));

  // XK_KP_1 (= NumPad 1 with Num Lock), Num Lock modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD1,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_1_,
                                      Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NUMPAD1,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_1_,
                                      Mod2Mask));

  // XK_KP_2 (= NumPad 2 with Num Lock), Num Lock modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD2,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_2_,
                                      Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NUMPAD2,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_2_,
                                      Mod2Mask));

  // XK_KP_3 (= NumPad 3 with Num Lock), Num Lock modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD3,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_3_,
                                      Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NUMPAD3,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_3_,
                                      Mod2Mask));

  // XK_KP_4 (= NumPad 4 with Num Lock), Num Lock modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD4,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_4_,
                                      Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NUMPAD4,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_4_,
                                      Mod2Mask));

  // XK_KP_5 (= NumPad 5 with Num Lock), Num Lock modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD5,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_5_,
                                      Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NUMPAD5,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_5_,
                                      Mod2Mask));

  // XK_KP_6 (= NumPad 6 with Num Lock), Num Lock modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD6,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_6_,
                                      Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NUMPAD6,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_6_,
                                      Mod2Mask));

  // XK_KP_7 (= NumPad 7 with Num Lock), Num Lock modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD7,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_7_,
                                      Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NUMPAD7,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_7_,
                                      Mod2Mask));

  // XK_KP_8 (= NumPad 8 with Num Lock), Num Lock modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD8,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_8_,
                                      Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NUMPAD8,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_8_,
                                      Mod2Mask));

  // XK_KP_9 (= NumPad 9 with Num Lock), Num Lock modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD9,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_9_,
                                      Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NUMPAD9,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_9_,
                                      Mod2Mask));
}

// Tests if the rewriter can handle a Command + Num Pad event.
TEST_F(EventRewriterTest, TestRewriteNumPadKeysOnAppleKeyboard) {
  TestingPrefService prefs;
  EventRewriter rewriter;
  rewriter.DeviceAddedForTesting(0, "Apple Keyboard");
  rewriter.set_last_device_id_for_testing(0);
  rewriter.set_pref_service_for_testing(&prefs);

  // XK_KP_End (= NumPad 1 without Num Lock), Win modifier.
  // The result should be "Num Pad 1 with Control + Num Lock modifiers".
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD1,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_1_,
                                      ControlMask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_END,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_end_,
                                      Mod4Mask));

  // XK_KP_1 (= NumPad 1 without Num Lock), Win modifier.
  // The result should also be "Num Pad 1 with Control + Num Lock modifiers".
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NUMPAD1,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_1_,
                                      ControlMask | Mod2Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_NUMPAD1,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_num_pad_end_,
                                      Mod4Mask));
}

TEST_F(EventRewriterTest, TestRewriteModifiersNoRemap) {
  TestingPrefService prefs;
  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  // Press Search. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_LWIN,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      0U));

  // Press left Control. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CONTROL,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      0U));

  // Press right Control. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_r_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CONTROL,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_r_,
                                      0U));

  // Press left Alt. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_MENU,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_alt_l_,
                                      0,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_MENU,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_alt_l_,
                                      0U));

  // Press right Alt. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_MENU,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_alt_r_,
                                      0,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_MENU,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_alt_r_,
                                      0U));

  // Test KeyRelease event, just in case.
  // Release Search. Confirm the release event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_LWIN,
                                      0,
                                      ui::ET_KEY_RELEASED,
                                      keycode_super_l_,
                                      Mod4Mask,
                                      KeyRelease),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      0,
                                      ui::ET_KEY_RELEASED,
                                      keycode_super_l_,
                                      Mod4Mask));
}

TEST_F(EventRewriterTest, TestRewriteModifiersNoRemapMultipleKeys) {
  TestingPrefService prefs;
  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  // Press left Alt with Shift. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_MENU,
                                      ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_meta_l_,
                                      ShiftMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_MENU,
                                      ui::EF_SHIFT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_meta_l_,
                                      ShiftMask));

  // Press right Alt with Shift. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_MENU,
                                      ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_meta_r_,
                                      ShiftMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_MENU,
                                      ui::EF_SHIFT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_meta_r_,
                                      ShiftMask));

  // Press Search with Caps Lock mask. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_LWIN,
                                      ui::EF_CAPS_LOCK_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      LockMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_CAPS_LOCK_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      LockMask));

  // Release Search with Caps Lock mask. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_LWIN,
                                      ui::EF_CAPS_LOCK_DOWN,
                                      ui::ET_KEY_RELEASED,
                                      keycode_super_l_,
                                      LockMask | Mod4Mask,
                                      KeyRelease),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_CAPS_LOCK_DOWN,
                                      ui::ET_KEY_RELEASED,
                                      keycode_super_l_,
                                      LockMask | Mod4Mask));

  // Press Shift+Ctrl+Alt+Search+A. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_B,
                                      ui::EF_SHIFT_DOWN |
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_b_,
                                      ShiftMask | ControlMask | Mod1Mask |
                                      Mod4Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_B,
                                      ui::EF_SHIFT_DOWN |
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_b_,
                                      ShiftMask | ControlMask | Mod1Mask |
                                      Mod4Mask));
}

TEST_F(EventRewriterTest, TestRewriteModifiersDisableSome) {
  // Disable Search and Control keys.
  TestingPrefService prefs;
  chromeos::Preferences::RegisterUserPrefs(&prefs);
  IntegerPrefMember search;
  search.Init(prefs::kLanguageXkbRemapSearchKeyTo, &prefs, NULL);
  search.SetValue(chromeos::input_method::kVoidKey);
  IntegerPrefMember control;
  control.Init(prefs::kLanguageXkbRemapControlKeyTo, &prefs, NULL);
  control.SetValue(chromeos::input_method::kVoidKey);

  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  // Press left Alt with Shift. This key press shouldn't be affected by the
  // pref. Confirm the event is not rewritten.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_MENU,
                                      ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_meta_l_,
                                      ShiftMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_MENU,
                                      ui::EF_SHIFT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_meta_l_,
                                      ShiftMask));

  // Press Search. Confirm the event is now VKEY_UNKNOWN + XK_VoidSymbol.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_UNKNOWN,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_void_symbol_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      0U));

  // Press Control. Confirm the event is now VKEY_UNKNOWN + XK_VoidSymbol.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_UNKNOWN,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_void_symbol_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CONTROL,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      0U));

  // Press Control+Search. Confirm the event is now VKEY_UNKNOWN +
  // XK_VoidSymbol without any modifiers.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_UNKNOWN,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_void_symbol_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      ControlMask));

  // Press Control+Search+a. Confirm the event is now VKEY_A without any
  // modifiers.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      ControlMask | Mod4Mask));

  // Press Control+Search+Alt+a. Confirm the event is now VKEY_A only with
  // the Alt modifier.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod1Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      ControlMask | Mod1Mask | Mod4Mask));

  // Remap Alt to Control.
  IntegerPrefMember alt;
  alt.Init(prefs::kLanguageXkbRemapAltKeyTo, &prefs, NULL);
  alt.SetValue(chromeos::input_method::kControlKey);

  // Press left Alt. Confirm the event is now VKEY_CONTROL + XK_Control_L
  // even though the Control key itself is disabled.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_MENU,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_alt_l_,
                                      0U));

  // Press Alt+a. Confirm the event is now Control+a even though the Control
  // key itself is disabled.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_A,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      ControlMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_A,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_a_,
                                      Mod1Mask));
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToControl) {
  // Remap Search to Control.
  TestingPrefService prefs;
  chromeos::Preferences::RegisterUserPrefs(&prefs);
  IntegerPrefMember search;
  search.Init(prefs::kLanguageXkbRemapSearchKeyTo, &prefs, NULL);
  search.SetValue(chromeos::input_method::kControlKey);

  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  // Press Search. Confirm the event is now VKEY_CONTROL + XK_Control_L.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      0U));

  // Remap Alt to Control too.
  IntegerPrefMember alt;
  alt.Init(prefs::kLanguageXkbRemapAltKeyTo, &prefs, NULL);
  alt.SetValue(chromeos::input_method::kControlKey);

  // Press left Alt. Confirm the event is now VKEY_CONTROL + XK_Control_L.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_MENU,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_alt_l_,
                                      0U));

  // Press right Alt. Confirm the event is now VKEY_CONTROL + XK_Control_R.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_r_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_MENU,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_alt_r_,
                                      0U));

  // Press Alt+Search. Confirm the event is now VKEY_CONTROL + XK_Control_L.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      ControlMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      Mod1Mask));

  // Press Control+Alt+Search. Confirm the event is now VKEY_CONTROL +
  // XK_Control_L.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      ControlMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      ControlMask | Mod1Mask));

  // Press Shift+Control+Alt+Search. Confirm the event is now Control with
  // Shift and Control modifiers.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      ShiftMask | ControlMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_SHIFT_DOWN |
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      ShiftMask | ControlMask | Mod1Mask));

  // Press Shift+Control+Alt+Search+B. Confirm the event is now B with Shift
  // and Control modifiers.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_B,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_b_,
                                      ShiftMask | ControlMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_B,
                                      ui::EF_SHIFT_DOWN |
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_b_,
                                      ShiftMask | ControlMask | Mod1Mask));
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapMany) {
  // Remap Search to Alt.
  TestingPrefService prefs;
  chromeos::Preferences::RegisterUserPrefs(&prefs);
  IntegerPrefMember search;
  search.Init(prefs::kLanguageXkbRemapSearchKeyTo, &prefs, NULL);
  search.SetValue(chromeos::input_method::kAltKey);

  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  // Press Search. Confirm the event is now VKEY_MENU + XK_Alt_L.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_MENU,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_alt_l_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      0U));

  // Remap Alt to Control.
  IntegerPrefMember alt;
  alt.Init(prefs::kLanguageXkbRemapAltKeyTo, &prefs, NULL);
  alt.SetValue(chromeos::input_method::kControlKey);

  // Press left Alt. Confirm the event is now VKEY_CONTROL + XK_Control_L.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_MENU,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_alt_l_,
                                      0U));

  // Remap Control to Search.
  IntegerPrefMember control;
  control.Init(prefs::kLanguageXkbRemapControlKeyTo, &prefs, NULL);
  control.SetValue(chromeos::input_method::kSearchKey);

  // Press left Control. Confirm the event is now VKEY_LWIN.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_LWIN,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CONTROL,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      0U));

  // Then, press all of the three, Control+Alt+Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_MENU,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_alt_l_,
                                      ControlMask | Mod4Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      ControlMask | Mod1Mask));

  // Press Shift+Control+Alt+Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_MENU,
                                      (ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                       ui::EF_ALT_DOWN),
                                      ui::ET_KEY_PRESSED,
                                      keycode_alt_l_,
                                      ShiftMask | ControlMask | Mod4Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_SHIFT_DOWN |
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      ShiftMask | ControlMask | Mod1Mask));

  // Press Shift+Control+Alt+Search+B
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_B,
                                      ui::EF_SHIFT_DOWN |
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_b_,
                                      ShiftMask | ControlMask | Mod1Mask |
                                      Mod4Mask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_B,
                                      ui::EF_SHIFT_DOWN |
                                      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_b_,
                                      ShiftMask | ControlMask | Mod1Mask |
                                      Mod4Mask));
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToCapsLock) {
  // Remap Search to Caps Lock.
  TestingPrefService prefs;
  chromeos::Preferences::RegisterUserPrefs(&prefs);
  IntegerPrefMember search;
  search.Init(prefs::kLanguageXkbRemapSearchKeyTo, &prefs, NULL);
  search.SetValue(chromeos::input_method::kCapsLockKey);

  chromeos::input_method::MockXKeyboard xkeyboard;
  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);
  rewriter.set_xkeyboard_for_testing(&xkeyboard);
  EXPECT_FALSE(xkeyboard.caps_lock_is_enabled_);

  // Press Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CAPITAL,
                                      ui::EF_CAPS_LOCK_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_caps_lock_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      0U));
  // Confirm that the Caps Lock status is changed.
  EXPECT_TRUE(xkeyboard.caps_lock_is_enabled_);

  // Release Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CAPITAL,
                                      ui::EF_NONE,
                                      ui::ET_KEY_RELEASED,
                                      keycode_caps_lock_,
                                      LockMask,
                                      KeyRelease),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_CAPS_LOCK_DOWN,
                                      ui::ET_KEY_RELEASED,
                                      keycode_super_l_,
                                      Mod4Mask | LockMask));
  // Confirm that the Caps Lock status is not changed.
  EXPECT_TRUE(xkeyboard.caps_lock_is_enabled_);

  // Press Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CAPITAL,
                                      ui::EF_CAPS_LOCK_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_caps_lock_,
                                      LockMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_CAPS_LOCK_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_super_l_,
                                      LockMask));
  // Confirm that the Caps Lock status is changed.
  EXPECT_FALSE(xkeyboard.caps_lock_is_enabled_);

  // Release Search.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CAPITAL,
                                      ui::EF_NONE,
                                      ui::ET_KEY_RELEASED,
                                      keycode_caps_lock_,
                                      LockMask,
                                      KeyRelease),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_LWIN,
                                      ui::EF_CAPS_LOCK_DOWN,
                                      ui::ET_KEY_RELEASED,
                                      keycode_super_l_,
                                      Mod4Mask | LockMask));
  // Confirm that the Caps Lock status is not changed.
  EXPECT_FALSE(xkeyboard.caps_lock_is_enabled_);

  // Press Caps Lock (on an external keyboard).
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CAPITAL,
                                      ui::EF_CAPS_LOCK_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_caps_lock_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CAPITAL,
                                      ui::EF_NONE,
                                      ui::ET_KEY_PRESSED,
                                      keycode_caps_lock_,
                                      0U));

  // Confirm that calling RewriteForTesting() does not change the state of
  // |xkeyboard|. In this case, X Window system itself should change the
  // Caps Lock state, not ash::EventRewriter.
  EXPECT_FALSE(xkeyboard.caps_lock_is_enabled_);

  // Press Caps Lock (on an external keyboard).
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CAPITAL,
                                      ui::EF_NONE,
                                      ui::ET_KEY_RELEASED,
                                      keycode_caps_lock_,
                                      LockMask,
                                      KeyRelease),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_CAPITAL,
                                      ui::EF_CAPS_LOCK_DOWN,
                                      ui::ET_KEY_RELEASED,
                                      keycode_caps_lock_,
                                      LockMask));
  EXPECT_FALSE(xkeyboard.caps_lock_is_enabled_);
}

TEST_F(EventRewriterTest, TestRewriteBackspaceAndArrowKeys) {
  TestingPrefService prefs;
  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  // Alt+Backspace -> Delete
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_DELETE,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_delete_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_BACK,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_backspace_,
                                      Mod1Mask));

  // Ctrl+Alt+Backspace -> Ctrl+Delete
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_DELETE,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_delete_,
                                      ControlMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_BACK,
                                      ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_backspace_,
                                      Mod1Mask | ControlMask));

  // Alt+Up -> Prior
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_PRIOR,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_prior_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_UP,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_up_,
                                      Mod1Mask));

  // Alt+Down -> Next
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_NEXT,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_next_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_DOWN,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_down_,
                                      Mod1Mask));

  // Ctrl+Alt+Up -> Home
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_HOME,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_home_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_UP,
                                      ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_up_,
                                      Mod1Mask | ControlMask));

  // Ctrl+Alt+Down -> End
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_END,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_end_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_DOWN,
                                      ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_down_,
                                      Mod1Mask | ControlMask));

  // Shift+Ctrl+Alt+Down -> Shift+End
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_END,
                                      ui::EF_SHIFT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_end_,
                                      ShiftMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_DOWN,
                                      ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN |
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_down_,
                                      ShiftMask | Mod1Mask | ControlMask));
}

TEST_F(EventRewriterTest, TestRewriteBackspaceAndArrowKeysWithSearchRemapped) {
  // Remap Search to Control.
  TestingPrefService prefs;
  chromeos::Preferences::RegisterUserPrefs(&prefs);
  IntegerPrefMember search;
  search.Init(prefs::kLanguageXkbRemapSearchKeyTo, &prefs, NULL);
  search.SetValue(chromeos::input_method::kControlKey);

  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  // Alt+Search+Down -> End
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_END,
                                      0,
                                      ui::ET_KEY_PRESSED,
                                      keycode_end_,
                                      0U,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_DOWN,
                                      ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_down_,
                                      Mod1Mask | Mod4Mask));

  // Shift+Alt+Search+Down -> Shift+End
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_END,
                                      ui::EF_SHIFT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_end_,
                                      ShiftMask,
                                      KeyPress),
            GetRewrittenEventAsString(&rewriter,
                                      ui::VKEY_DOWN,
                                      ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_down_,
                                      ShiftMask | Mod1Mask | Mod4Mask));
}

TEST_F(EventRewriterTest, TestRewriteKeyEventSentByXSendEvent) {
  // Remap Control to Alt.
  TestingPrefService prefs;
  chromeos::Preferences::RegisterUserPrefs(&prefs);
  IntegerPrefMember control;
  control.Init(prefs::kLanguageXkbRemapControlKeyTo, &prefs, NULL);
  control.SetValue(chromeos::input_method::kAltKey);

  EventRewriter rewriter;
  rewriter.set_pref_service_for_testing(&prefs);

  // Send left control press.
  std::string rewritten_event;
  {
    XEvent xev;
    InitXKeyEvent(ui::VKEY_CONTROL, 0, ui::ET_KEY_PRESSED,
                  keycode_control_l_, 0U, &xev);
    xev.xkey.send_event = True;  // XSendEvent() always does this.
    ui::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    rewritten_event = StringPrintf(
        "ui_keycode=%d ui_flags=%d ui_type=%d "
        "x_keycode=%u x_state=%u x_type=%d",
        keyevent.key_code(), keyevent.flags(), keyevent.type(),
        xev.xkey.keycode, xev.xkey.state, xev.xkey.type);
  }

  // XK_Control_L (left Control key) should NOT be remapped to Alt if send_event
  // flag in the event is True.
  EXPECT_EQ(GetExpectedResultAsString(ui::VKEY_CONTROL,
                                      ui::EF_CONTROL_DOWN,
                                      ui::ET_KEY_PRESSED,
                                      keycode_control_l_,
                                      0U,
                                      KeyPress),
            rewritten_event);
}
#endif  // OS_CHROMEOS
