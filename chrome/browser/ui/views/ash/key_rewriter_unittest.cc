// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/ui/views/ash/key_rewriter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/event.h"

#if defined(OS_CHROMEOS)
#include <X11/keysym.h>
#include <X11/Xlib.h>

#include "ui/base/x/x11_util.h"

namespace {

void InitXKeyEvent(ui::KeyboardCode ui_keycode,
                   int ui_flags,
                   unsigned int x_keycode,
                   unsigned int x_state,
                   XEvent* event) {
  ui::InitXKeyEventForTesting(ui::ET_KEY_PRESSED,
                              ui_keycode,
                              ui_flags,
                              event);
  event->xkey.keycode = x_keycode;
  event->xkey.state = x_state;
}

}  // namespace
#endif

TEST(KeyRewriterTest, TestGetDeviceType) {
  // This is the typical string which an Apple keyboard sends.
  EXPECT_EQ(KeyRewriter::kDeviceAppleKeyboard,
            KeyRewriter::GetDeviceType("Apple Inc. Apple Keyboard"));

  // Other cases we accept.
  EXPECT_EQ(KeyRewriter::kDeviceAppleKeyboard,
            KeyRewriter::GetDeviceType("Apple Keyboard"));
  EXPECT_EQ(KeyRewriter::kDeviceAppleKeyboard,
            KeyRewriter::GetDeviceType("apple keyboard"));
  EXPECT_EQ(KeyRewriter::kDeviceAppleKeyboard,
            KeyRewriter::GetDeviceType("apple keyboard."));
  EXPECT_EQ(KeyRewriter::kDeviceAppleKeyboard,
            KeyRewriter::GetDeviceType("apple.keyboard."));
  EXPECT_EQ(KeyRewriter::kDeviceAppleKeyboard,
            KeyRewriter::GetDeviceType(".apple.keyboard."));

  // Dell, Microsoft, Logitech, ... should be recognized as a kDeviceUnknown.
  EXPECT_EQ(KeyRewriter::kDeviceUnknown,
            KeyRewriter::GetDeviceType("Dell Dell USB Entry Keyboard"));
  EXPECT_EQ(KeyRewriter::kDeviceUnknown,
            KeyRewriter::GetDeviceType("Microsoft Natural Ergonomic Keyboard"));
  EXPECT_EQ(KeyRewriter::kDeviceUnknown,
            KeyRewriter::GetDeviceType("CHESEN USB Keyboard"));

  // Some corner cases.
  EXPECT_EQ(KeyRewriter::kDeviceUnknown, KeyRewriter::GetDeviceType(""));
  EXPECT_EQ(KeyRewriter::kDeviceUnknown,
            KeyRewriter::GetDeviceType("."));
  EXPECT_EQ(KeyRewriter::kDeviceUnknown,
            KeyRewriter::GetDeviceType(". "));
  EXPECT_EQ(KeyRewriter::kDeviceUnknown,
            KeyRewriter::GetDeviceType(" ."));
  EXPECT_EQ(KeyRewriter::kDeviceUnknown,
            KeyRewriter::GetDeviceType("not-an-apple keyboard"));
}

TEST(KeyRewriterTest, TestDeviceAddedOrRemoved) {
  KeyRewriter rewriter;
  EXPECT_TRUE(rewriter.device_id_to_type_for_testing().empty());
  EXPECT_EQ(KeyRewriter::kDeviceUnknown,
            rewriter.DeviceAddedForTesting(0, "PC Keyboard"));
  EXPECT_EQ(1U, rewriter.device_id_to_type_for_testing().size());
  EXPECT_EQ(KeyRewriter::kDeviceAppleKeyboard,
            rewriter.DeviceAddedForTesting(1, "Apple Keyboard"));
  EXPECT_EQ(2U, rewriter.device_id_to_type_for_testing().size());
  // Try to reuse the first ID.
  EXPECT_EQ(KeyRewriter::kDeviceAppleKeyboard,
            rewriter.DeviceAddedForTesting(0, "Apple Keyboard"));
  EXPECT_EQ(2U, rewriter.device_id_to_type_for_testing().size());
}

#if defined(OS_CHROMEOS)
TEST(KeyRewriterTest, TestRewriteCommandToControl) {
  XEvent xev;

  const unsigned int kKeycodeA =
      XKeysymToKeycode(ui::GetXDisplay(), XK_a);
  const unsigned int kKeycodeSuperL =
      XKeysymToKeycode(ui::GetXDisplay(), XK_Super_L);
  const unsigned int kKeycodeSuperR =
      XKeysymToKeycode(ui::GetXDisplay(), XK_Super_R);
  const unsigned int kKeycodeControlL =
      XKeysymToKeycode(ui::GetXDisplay(), XK_Control_L);
  const unsigned int kKeycodeControlR =
      XKeysymToKeycode(ui::GetXDisplay(), XK_Control_R);

  // First, test with a PC keyboard.
  KeyRewriter rewriter;
  rewriter.DeviceAddedForTesting(0, "PC Keyboard");
  rewriter.set_last_device_id_for_testing(0);
  {
    // XK_a, Alt modifier.
    InitXKeyEvent(ui::VKEY_A, ui::EF_ALT_DOWN, kKeycodeA, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_A, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeA, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask), xkey.state);
  }
  {
    // XK_a, Win modifier.
    InitXKeyEvent(ui::VKEY_A, 0, kKeycodeA, Mod4Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_A, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeA, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod4Mask), xkey.state);
  }
  {
    // XK_a, Alt+Win modifier.
    InitXKeyEvent(ui::VKEY_A, ui::EF_ALT_DOWN,
                  kKeycodeA, Mod1Mask | Mod4Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_A, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeA, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | Mod4Mask), xkey.state);
  }
  {
    // XK_Super_L (left Windows key), Alt modifier.
    InitXKeyEvent(ui::VKEY_LWIN, ui::EF_ALT_DOWN,
                  kKeycodeSuperL, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_LWIN, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeSuperL, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask), xkey.state);
  }
  {
    // XK_Super_R (right Windows key), Alt modifier.
    InitXKeyEvent(ui::VKEY_RWIN, ui::EF_ALT_DOWN,
                  kKeycodeSuperR, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_RWIN, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeSuperR, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask), xkey.state);
  }

  // An Apple keyboard reuse the ID, zero.
  rewriter.DeviceAddedForTesting(0, "Apple Keyboard");
  rewriter.set_last_device_id_for_testing(0);
  {
    // XK_a, Alt modifier.
    InitXKeyEvent(ui::VKEY_A, ui::EF_ALT_DOWN, kKeycodeA, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_A, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeA, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask), xkey.state);
  }
  {
    // XK_a, Win modifier.
    InitXKeyEvent(ui::VKEY_A, 0, kKeycodeA, Mod4Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_A, keyevent.key_code());
    EXPECT_EQ(ui::EF_CONTROL_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeA, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(ControlMask), xkey.state);
  }
  {
    // XK_a, Alt+Win modifier.
    InitXKeyEvent(ui::VKEY_A, ui::EF_ALT_DOWN,
                  kKeycodeA, Mod1Mask | Mod4Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_A, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeA, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | ControlMask), xkey.state);
  }
  {
    // XK_Super_L (left Windows key), Alt modifier.
    InitXKeyEvent(ui::VKEY_LWIN, ui::EF_ALT_DOWN,
                  kKeycodeSuperL, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_CONTROL, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeControlL, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask), xkey.state);
  }
  {
    // XK_Super_R (right Windows key), Alt modifier.
    InitXKeyEvent(ui::VKEY_RWIN, ui::EF_ALT_DOWN,
                  kKeycodeSuperR, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_CONTROL, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeControlR, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask), xkey.state);
  }
}

TEST(KeyRewriterTest, TestRewriteNumPadKeys) {
  XEvent xev;
  Display* display = ui::GetXDisplay();

  const unsigned int kKeycodeNumPad0 = XKeysymToKeycode(display, XK_KP_0);
  const unsigned int kKeycodeNumPadDecimal =
      XKeysymToKeycode(display, XK_KP_Decimal);
  const unsigned int kKeycodeNumPad1 = XKeysymToKeycode(display, XK_KP_1);
  const unsigned int kKeycodeNumPad2 = XKeysymToKeycode(display, XK_KP_2);
  const unsigned int kKeycodeNumPad3 = XKeysymToKeycode(display, XK_KP_3);
  const unsigned int kKeycodeNumPad4 = XKeysymToKeycode(display, XK_KP_4);
  const unsigned int kKeycodeNumPad5 = XKeysymToKeycode(display, XK_KP_5);
  const unsigned int kKeycodeNumPad6 = XKeysymToKeycode(display, XK_KP_6);
  const unsigned int kKeycodeNumPad7 = XKeysymToKeycode(display, XK_KP_7);
  const unsigned int kKeycodeNumPad8 = XKeysymToKeycode(display, XK_KP_8);
  const unsigned int kKeycodeNumPad9 = XKeysymToKeycode(display, XK_KP_9);

  const unsigned int kKeycodeNumPadInsert =
      XKeysymToKeycode(display, XK_KP_Insert);
  const unsigned int kKeycodeNumPadDelete =
      XKeysymToKeycode(display, XK_KP_Delete);
  const unsigned int kKeycodeNumPadEnd = XKeysymToKeycode(display, XK_KP_End);
  const unsigned int kKeycodeNumPadDown = XKeysymToKeycode(display, XK_KP_Down);
  const unsigned int kKeycodeNumPadNext = XKeysymToKeycode(display, XK_KP_Next);
  const unsigned int kKeycodeNumPadLeft = XKeysymToKeycode(display, XK_KP_Left);
  const unsigned int kKeycodeNumPadBegin =
      XKeysymToKeycode(display, XK_KP_Begin);
  const unsigned int kKeycodeNumPadRight =
      XKeysymToKeycode(display, XK_KP_Right);
  const unsigned int kKeycodeNumPadHome = XKeysymToKeycode(display, XK_KP_Home);
  const unsigned int kKeycodeNumPadUp = XKeysymToKeycode(display, XK_KP_Up);
  const unsigned int kKeycodeNumPadPrior =
      XKeysymToKeycode(display, XK_KP_Prior);

  KeyRewriter rewriter;
  {
    // XK_KP_Insert (= NumPad 0 without Num Lock), no modifier.
    InitXKeyEvent(ui::VKEY_INSERT, 0, kKeycodeNumPadInsert, 0, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD0, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad0, xkey.keycode);
    // Mod2Mask means Num Lock.
    EXPECT_EQ(static_cast<unsigned int>(Mod2Mask), xkey.state);
  }
  {
    // XK_KP_Insert (= NumPad 0 without Num Lock), Alt modifier.
    InitXKeyEvent(ui::VKEY_INSERT, ui::EF_ALT_DOWN,
                  kKeycodeNumPadInsert, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD0, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad0, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | Mod2Mask), xkey.state);
  }
  {
    // XK_KP_Delete (= NumPad . without Num Lock), Alt modifier.
    InitXKeyEvent(ui::VKEY_DELETE, ui::EF_ALT_DOWN,
                  kKeycodeNumPadDelete, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_DECIMAL, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPadDecimal, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | Mod2Mask), xkey.state);
  }
  {
    // XK_KP_End (= NumPad 1 without Num Lock), Alt modifier.
    InitXKeyEvent(ui::VKEY_END, ui::EF_ALT_DOWN,
                  kKeycodeNumPadEnd, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD1, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad1, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | Mod2Mask), xkey.state);
  }
  {
    // XK_KP_Down (= NumPad 2 without Num Lock), Alt modifier.
    InitXKeyEvent(ui::VKEY_DOWN, ui::EF_ALT_DOWN,
                  kKeycodeNumPadDown, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD2, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad2, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | Mod2Mask), xkey.state);
  }
  {
    // XK_KP_Next (= NumPad 3 without Num Lock), Alt modifier.
    InitXKeyEvent(ui::VKEY_NEXT, ui::EF_ALT_DOWN,
                  kKeycodeNumPadNext, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD3, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad3, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | Mod2Mask), xkey.state);
  }
  {
    // XK_KP_Left (= NumPad 4 without Num Lock), Alt modifier.
    InitXKeyEvent(ui::VKEY_LEFT, ui::EF_ALT_DOWN,
                  kKeycodeNumPadLeft, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD4, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad4, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | Mod2Mask), xkey.state);
  }
  {
    // XK_KP_Begin (= NumPad 5 without Num Lock), Alt modifier.
    InitXKeyEvent(ui::VKEY_CLEAR, ui::EF_ALT_DOWN,
                  kKeycodeNumPadBegin, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD5, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad5, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | Mod2Mask), xkey.state);
  }
  {
    // XK_KP_Right (= NumPad 6 without Num Lock), Alt modifier.
    InitXKeyEvent(ui::VKEY_RIGHT, ui::EF_ALT_DOWN,
                  kKeycodeNumPadRight, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD6, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad6, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | Mod2Mask), xkey.state);
  }
  {
    // XK_KP_Home (= NumPad 7 without Num Lock), Alt modifier.
    InitXKeyEvent(ui::VKEY_HOME, ui::EF_ALT_DOWN,
                  kKeycodeNumPadHome, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD7, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad7, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | Mod2Mask), xkey.state);
  }
  {
    // XK_KP_Up (= NumPad 8 without Num Lock), Alt modifier.
    InitXKeyEvent(ui::VKEY_UP, ui::EF_ALT_DOWN,
                  kKeycodeNumPadUp, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD8, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad8, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | Mod2Mask), xkey.state);
  }
  {
    // XK_KP_Prior (= NumPad 9 without Num Lock), Alt modifier.
    InitXKeyEvent(ui::VKEY_PRIOR, ui::EF_ALT_DOWN,
                  kKeycodeNumPadPrior, Mod1Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD9, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad9, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask | Mod2Mask), xkey.state);
  }
  {
    // XK_KP_0 (= NumPad 0 with Num Lock), Num Lock modifier.
    InitXKeyEvent(ui::VKEY_NUMPAD0, 0, kKeycodeNumPad0, Mod2Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD0, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad0, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod2Mask), xkey.state);
  }
  {
    // XK_KP_DECIMAL (= NumPad . with Num Lock), Num Lock modifier.
    InitXKeyEvent(ui::VKEY_DECIMAL, 0, kKeycodeNumPadDecimal, Mod2Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_DECIMAL, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPadDecimal, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod2Mask), xkey.state);
  }
  {
    // XK_KP_1 (= NumPad 1 with Num Lock), Num Lock modifier.
    InitXKeyEvent(ui::VKEY_NUMPAD1, 0, kKeycodeNumPad1, Mod2Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD1, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad1, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod2Mask), xkey.state);
  }
  {
    // XK_KP_2 (= NumPad 2 with Num Lock), Num Lock modifier.
    InitXKeyEvent(ui::VKEY_NUMPAD2, 0, kKeycodeNumPad2, Mod2Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD2, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad2, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod2Mask), xkey.state);
  }
  {
    // XK_KP_3 (= NumPad 3 with Num Lock), Num Lock modifier.
    InitXKeyEvent(ui::VKEY_NUMPAD3, 0, kKeycodeNumPad3, Mod2Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD3, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad3, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod2Mask), xkey.state);
  }
  {
    // XK_KP_4 (= NumPad 4 with Num Lock), Num Lock modifier.
    InitXKeyEvent(ui::VKEY_NUMPAD4, 0, kKeycodeNumPad4, Mod2Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD4, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad4, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod2Mask), xkey.state);
  }
  {
    // XK_KP_5 (= NumPad 5 with Num Lock), Num Lock modifier.
    InitXKeyEvent(ui::VKEY_NUMPAD5, 0, kKeycodeNumPad5, Mod2Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD5, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad5, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod2Mask), xkey.state);
  }
  {
    // XK_KP_6 (= NumPad 6 with Num Lock), Num Lock modifier.
    InitXKeyEvent(ui::VKEY_NUMPAD6, 0, kKeycodeNumPad6, Mod2Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD6, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad6, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod2Mask), xkey.state);
  }
  {
    // XK_KP_7 (= NumPad 7 with Num Lock), Num Lock modifier.
    InitXKeyEvent(ui::VKEY_NUMPAD7, 0, kKeycodeNumPad7, Mod2Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD7, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad7, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod2Mask), xkey.state);
  }
  {
    // XK_KP_8 (= NumPad 8 with Num Lock), Num Lock modifier.
    InitXKeyEvent(ui::VKEY_NUMPAD8, 0, kKeycodeNumPad8, Mod2Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD8, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad8, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod2Mask), xkey.state);
  }
  {
    // XK_KP_9 (= NumPad 9 with Num Lock), Num Lock modifier.
    InitXKeyEvent(ui::VKEY_NUMPAD9, 0, kKeycodeNumPad9, Mod2Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_NUMPAD9, keyevent.key_code());
    EXPECT_EQ(0, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad9, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod2Mask), xkey.state);
  }
}

// Tests if the rewriter can handle a Command + Num Pad event.
TEST(KeyRewriterTest, TestRewriteNumPadKeysOnAppleKeyboard) {
  XEvent xev;
  Display* display = ui::GetXDisplay();

  const unsigned int kKeycodeNumPadEnd = XKeysymToKeycode(display, XK_KP_End);
  const unsigned int kKeycodeNumPad1 = XKeysymToKeycode(display, XK_KP_1);

  KeyRewriter rewriter;
  rewriter.DeviceAddedForTesting(0, "Apple Keyboard");
  rewriter.set_last_device_id_for_testing(0);
  {
    // XK_KP_End (= NumPad 1 without Num Lock), Win modifier.
    InitXKeyEvent(ui::VKEY_END, 0, kKeycodeNumPadEnd, Mod4Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    // The result should be "Num Pad 1 with Control + Num Lock modifiers".
    EXPECT_EQ(ui::VKEY_NUMPAD1, keyevent.key_code());
    EXPECT_EQ(ui::EF_CONTROL_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad1, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(ControlMask | Mod2Mask), xkey.state);
  }
  {
    // XK_KP_1 (= NumPad 1 without Num Lock), Win modifier.
    InitXKeyEvent(ui::VKEY_NUMPAD1, 0, kKeycodeNumPadEnd, Mod4Mask, &xev);
    aura::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter.RewriteForTesting(&keyevent);
    // The result should also be "Num Pad 1 with Control + Num Lock modifiers".
    EXPECT_EQ(ui::VKEY_NUMPAD1, keyevent.key_code());
    EXPECT_EQ(ui::EF_CONTROL_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeNumPad1, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(ControlMask | Mod2Mask), xkey.state);
  }
}

#endif  // OS_CHROMEOS
