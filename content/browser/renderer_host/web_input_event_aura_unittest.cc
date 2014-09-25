// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_input_event_aura.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

#if defined(USE_X11)
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/gfx/x/x11_types.h"
#endif

namespace content {

// Checks that MakeWebKeyboardEvent makes a DOM3 spec compliant key event.
// crbug.com/127142
TEST(WebInputEventAuraTest, TestMakeWebKeyboardEvent) {
#if defined(USE_X11)
  ui::ScopedXI2Event xev;
  {
    // Press Ctrl.
    xev.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, 0);
    ui::KeyEvent event(xev);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(&event);
    // However, modifier bit for Control in |webkit_event| should be set.
    EXPECT_EQ(webkit_event.modifiers, blink::WebInputEvent::ControlKey);
  }
  {
    // Release Ctrl.
    xev.InitKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL, ControlMask);
    ui::KeyEvent event(xev);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(&event);
    // However, modifier bit for Control in |webkit_event| shouldn't be set.
    EXPECT_EQ(webkit_event.modifiers, 0);
  }
#endif
}

// Checks that MakeWebKeyboardEvent returns a correct windowsKeyCode.
TEST(WebInputEventAuraTest, TestMakeWebKeyboardEventWindowsKeyCode) {
#if defined(USE_X11)
  ui::ScopedXI2Event xev;
  {
    // Press left Ctrl.
    xev.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, 0);
    XEvent* xevent = xev;
    xevent->xkey.keycode = XKeysymToKeycode(gfx::GetXDisplay(), XK_Control_L);
    ui::KeyEvent event(xev);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(&event);
    // ui::VKEY_LCONTROL, instead of ui::VKEY_CONTROL, should be filled.
    EXPECT_EQ(ui::VKEY_LCONTROL, webkit_event.windowsKeyCode);
  }
  {
    // Press right Ctrl.
    xev.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, 0);
    XEvent* xevent = xev;
    xevent->xkey.keycode = XKeysymToKeycode(gfx::GetXDisplay(), XK_Control_R);
    ui::KeyEvent event(xev);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(&event);
    // ui::VKEY_RCONTROL, instead of ui::VKEY_CONTROL, should be filled.
    EXPECT_EQ(ui::VKEY_RCONTROL, webkit_event.windowsKeyCode);
  }
#elif defined(OS_WIN)
  // TODO(yusukes): Add tests for win_aura once keyboardEvent() in
  // third_party/WebKit/Source/web/win/WebInputEventFactory.cpp is modified
  // to return VKEY_[LR]XXX instead of VKEY_XXX.
  // https://bugs.webkit.org/show_bug.cgi?id=86694
#endif
}

// Checks that MakeWebKeyboardEvent fills a correct keypard modifier.
TEST(WebInputEventAuraTest, TestMakeWebKeyboardEventKeyPadKeyCode) {
#if defined(USE_X11)
  struct TestCase {
    ui::KeyboardCode ui_keycode;  // The virtual key code.
    uint32 x_keycode;  // The platform key code.
    bool expected_result;  // true if the event has "isKeyPad" modifier.
  } kTesCases[] = {
    {ui::VKEY_0, XK_0, false},
    {ui::VKEY_1, XK_1, false},
    {ui::VKEY_2, XK_2, false},
    {ui::VKEY_3, XK_3, false},
    {ui::VKEY_4, XK_4, false},
    {ui::VKEY_5, XK_5, false},
    {ui::VKEY_6, XK_6, false},
    {ui::VKEY_7, XK_7, false},
    {ui::VKEY_8, XK_8, false},
    {ui::VKEY_9, XK_9, false},

    {ui::VKEY_NUMPAD0, XK_KP_0, true},
    {ui::VKEY_NUMPAD1, XK_KP_1, true},
    {ui::VKEY_NUMPAD2, XK_KP_2, true},
    {ui::VKEY_NUMPAD3, XK_KP_3, true},
    {ui::VKEY_NUMPAD4, XK_KP_4, true},
    {ui::VKEY_NUMPAD5, XK_KP_5, true},
    {ui::VKEY_NUMPAD6, XK_KP_6, true},
    {ui::VKEY_NUMPAD7, XK_KP_7, true},
    {ui::VKEY_NUMPAD8, XK_KP_8, true},
    {ui::VKEY_NUMPAD9, XK_KP_9, true},

    {ui::VKEY_MULTIPLY, XK_KP_Multiply, true},
    {ui::VKEY_SUBTRACT, XK_KP_Subtract, true},
    {ui::VKEY_ADD, XK_KP_Add, true},
    {ui::VKEY_DIVIDE, XK_KP_Divide, true},
    {ui::VKEY_DECIMAL, XK_KP_Decimal, true},
    {ui::VKEY_DELETE, XK_KP_Delete, true},
    {ui::VKEY_INSERT, XK_KP_Insert, true},
    {ui::VKEY_END, XK_KP_End, true},
    {ui::VKEY_DOWN, XK_KP_Down, true},
    {ui::VKEY_NEXT, XK_KP_Page_Down, true},
    {ui::VKEY_LEFT, XK_KP_Left, true},
    {ui::VKEY_CLEAR, XK_KP_Begin, true},
    {ui::VKEY_RIGHT, XK_KP_Right, true},
    {ui::VKEY_HOME, XK_KP_Home, true},
    {ui::VKEY_UP, XK_KP_Up, true},
    {ui::VKEY_PRIOR, XK_KP_Page_Up, true},
  };
  ui::ScopedXI2Event xev;
  for (size_t i = 0; i < arraysize(kTesCases); ++i) {
    const TestCase& test_case = kTesCases[i];

    xev.InitKeyEvent(ui::ET_KEY_PRESSED, test_case.ui_keycode, 0);
    XEvent* xevent = xev;
    xevent->xkey.keycode = XKeysymToKeycode(gfx::GetXDisplay(),
                                            test_case.x_keycode);
    ui::KeyEvent event(xev);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(&event);
    EXPECT_EQ(test_case.expected_result,
              (webkit_event.modifiers & blink::WebInputEvent::IsKeyPad) != 0)
        << "Failed in " << i << "th test case: "
        << "{ui_keycode:" << test_case.ui_keycode
        << ", x_keycode:" << test_case.x_keycode
        << "}, expect: " << test_case.expected_result;
  }
#endif
}

}  // namespace content
