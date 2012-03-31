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
    rewriter.RewriteCommandToControlForTesting(&keyevent);
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
    rewriter.RewriteCommandToControlForTesting(&keyevent);
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
    rewriter.RewriteCommandToControlForTesting(&keyevent);
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
    rewriter.RewriteCommandToControlForTesting(&keyevent);
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
    rewriter.RewriteCommandToControlForTesting(&keyevent);
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
    rewriter.RewriteCommandToControlForTesting(&keyevent);
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
    rewriter.RewriteCommandToControlForTesting(&keyevent);
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
    rewriter.RewriteCommandToControlForTesting(&keyevent);
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
    rewriter.RewriteCommandToControlForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_LCONTROL, keyevent.key_code());
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
    rewriter.RewriteCommandToControlForTesting(&keyevent);
    EXPECT_EQ(ui::VKEY_RCONTROL, keyevent.key_code());
    EXPECT_EQ(ui::EF_ALT_DOWN, keyevent.flags());
    const XKeyEvent& xkey = keyevent.native_event()->xkey;
    EXPECT_EQ(kKeycodeControlR, xkey.keycode);
    EXPECT_EQ(static_cast<unsigned int>(Mod1Mask), xkey.state);
  }
}
#endif  // OS_CHROMEOS
