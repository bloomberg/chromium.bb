// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/keyboard_driven_event_rewriter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"

#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/Xlib.h>

namespace chromeos {

// Creates an XKeyEvent to initialize a ui::KeyEvent that is passed to
// KeyboardDrivenEventRewriter for processing.
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

class KeyboardDrivenEventRewriterTest : public testing::Test {
 public:
  KeyboardDrivenEventRewriterTest()
      : display_(ui::GetXDisplay()),
        keycode_a_(XKeysymToKeycode(display_, XK_a)),
        keycode_up_(XKeysymToKeycode(display_, XK_Up)),
        keycode_down_(XKeysymToKeycode(display_, XK_Down)),
        keycode_left_(XKeysymToKeycode(display_, XK_Left)),
        keycode_right_(XKeysymToKeycode(display_, XK_Right)),
        keycode_return_(XKeysymToKeycode(display_, XK_Return)) {
  }

  virtual ~KeyboardDrivenEventRewriterTest() {}

 protected:
  std::string GetRewrittenEventAsString(ui::KeyboardCode ui_keycode,
                                        int ui_flags,
                                        ui::EventType ui_type,
                                        KeyCode x_keycode,
                                        unsigned int x_state) {
    XEvent xev;
    InitXKeyEvent(ui_keycode, ui_flags, ui_type, x_keycode, x_state, &xev);
    ui::KeyEvent keyevent(&xev, false /* is_char */);
    rewriter_.RewriteForTesting(&keyevent);
    return base::StringPrintf(
        "ui_flags=%d x_state=%u", keyevent.flags(), xev.xkey.state);
  }

  std::string GetExpectedResultAsString(int ui_flags, unsigned int x_state) {
    return base::StringPrintf("ui_flags=%d x_state=%u", ui_flags, x_state);
  }

  Display* display_;
  const KeyCode keycode_a_;
  const KeyCode keycode_up_;
  const KeyCode keycode_down_;
  const KeyCode keycode_left_;
  const KeyCode keycode_right_;
  const KeyCode keycode_return_;

  KeyboardDrivenEventRewriter rewriter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardDrivenEventRewriterTest);
};

TEST_F(KeyboardDrivenEventRewriterTest, PassThrough) {
  struct {
    ui::KeyboardCode ui_keycode;
    int ui_flags;
    KeyCode x_keycode;
    unsigned int x_state;
  } kTests[] = {
    { ui::VKEY_A, ui::EF_NONE, keycode_a_, 0 },
    { ui::VKEY_A, ui::EF_CONTROL_DOWN, keycode_a_, ControlMask },
    { ui::VKEY_A, ui::EF_ALT_DOWN, keycode_a_, Mod1Mask },
    { ui::VKEY_A, ui::EF_SHIFT_DOWN, keycode_a_, ShiftMask },
    { ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        keycode_a_, ControlMask | Mod1Mask },
    { ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
        keycode_a_, ControlMask | Mod1Mask | ShiftMask },

    { ui::VKEY_LEFT, ui::EF_NONE, keycode_left_, 0 },
    { ui::VKEY_LEFT, ui::EF_CONTROL_DOWN, keycode_left_, ControlMask },
    { ui::VKEY_LEFT, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        keycode_left_, ControlMask | Mod1Mask },

    { ui::VKEY_RIGHT, ui::EF_NONE, keycode_right_, 0 },
    { ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN, keycode_right_, ControlMask },
    { ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        keycode_right_, ControlMask | Mod1Mask },

    { ui::VKEY_UP, ui::EF_NONE, keycode_up_, 0 },
    { ui::VKEY_UP, ui::EF_CONTROL_DOWN, keycode_up_, ControlMask },
    { ui::VKEY_UP, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        keycode_up_, ControlMask | Mod1Mask },

    { ui::VKEY_DOWN, ui::EF_NONE, keycode_down_, 0 },
    { ui::VKEY_DOWN, ui::EF_CONTROL_DOWN, keycode_down_, ControlMask },
    { ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        keycode_down_, ControlMask | Mod1Mask },

    { ui::VKEY_RETURN, ui::EF_NONE, keycode_return_, 0 },
    { ui::VKEY_RETURN, ui::EF_CONTROL_DOWN, keycode_return_, ControlMask },
    { ui::VKEY_RETURN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
        keycode_return_, ControlMask | Mod1Mask },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTests); ++i) {
    EXPECT_EQ(GetExpectedResultAsString(kTests[i].ui_flags,
                                        kTests[i].x_state),
              GetRewrittenEventAsString(kTests[i].ui_keycode,
                                        kTests[i].ui_flags,
                                        ui::ET_KEY_PRESSED,
                                        kTests[i].x_keycode,
                                        kTests[i].x_state))
    << "Test case " << i;
  }
}

TEST_F(KeyboardDrivenEventRewriterTest, Rewrite) {
  const int kModifierMask = ui::EF_SHIFT_DOWN;
  const unsigned int kXState = ShiftMask;

  struct {
    ui::KeyboardCode ui_keycode;
    int ui_flags;
    KeyCode x_keycode;
    unsigned int x_state;
  } kTests[] = {
    { ui::VKEY_LEFT, kModifierMask, keycode_left_, kXState },
    { ui::VKEY_RIGHT, kModifierMask, keycode_right_, kXState },
    { ui::VKEY_UP, kModifierMask, keycode_up_, kXState },
    { ui::VKEY_DOWN, kModifierMask, keycode_down_, kXState },
    { ui::VKEY_RETURN, kModifierMask, keycode_return_, kXState },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTests); ++i) {
    EXPECT_EQ(GetExpectedResultAsString(ui::EF_NONE, 0),
              GetRewrittenEventAsString(kTests[i].ui_keycode,
                                        kTests[i].ui_flags,
                                        ui::ET_KEY_PRESSED,
                                        kTests[i].x_keycode,
                                        kTests[i].x_state))
    << "Test case " << i;
  }
}

}  // namespace chromeos
