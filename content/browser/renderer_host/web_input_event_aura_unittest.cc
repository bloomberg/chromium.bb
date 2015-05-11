// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_input_event_aura.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

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
  {
    // Press Ctrl.
    ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL,
                       ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    // However, modifier bit for Control in |webkit_event| should be set.
    EXPECT_EQ(blink::WebInputEvent::ControlKey | blink::WebInputEvent::IsLeft,
              webkit_event.modifiers);
  }
  {
    // Release Ctrl.
    ui::KeyEvent event(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL,
                       ui::DomCode::CONTROL_LEFT, ui::EF_NONE);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    // However, modifier bit for Control in |webkit_event| shouldn't be set.
    EXPECT_EQ(blink::WebInputEvent::IsLeft, webkit_event.modifiers);
  }
#if defined(USE_X11)
  const int kLocationModifiers =
      blink::WebInputEvent::IsLeft | blink::WebInputEvent::IsRight;
  ui::ScopedXI2Event xev;
  {
    // Press Ctrl.
    xev.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, 0);
    ui::KeyEvent event(xev);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    // However, modifier bit for Control in |webkit_event| should be set.
    EXPECT_EQ(blink::WebInputEvent::ControlKey,
              webkit_event.modifiers & ~kLocationModifiers);
  }
  {
    // Release Ctrl.
    xev.InitKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL, ControlMask);
    ui::KeyEvent event(xev);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    // However, modifier bit for Control in |webkit_event| shouldn't be set.
    EXPECT_EQ(0, webkit_event.modifiers & ~kLocationModifiers);
  }
#endif
}

// Checks that MakeWebKeyboardEvent returns a correct windowsKeyCode.
#if defined(OS_CHROMEOS) || defined(THREAD_SANITIZER)
// Fails on Chrome OS and under ThreadSanitizer on Linux, see
// https://crbug.com/449103.
#define MAYBE_TestMakeWebKeyboardEventWindowsKeyCode \
    DISABLED_TestMakeWebKeyboardEventWindowsKeyCode
#else
#define MAYBE_TestMakeWebKeyboardEventWindowsKeyCode \
    TestMakeWebKeyboardEventWindowsKeyCode
#endif
TEST(WebInputEventAuraTest, MAYBE_TestMakeWebKeyboardEventWindowsKeyCode) {
#if defined(USE_X11)
  ui::ScopedXI2Event xev;
  {
    // Press left Ctrl.
    xev.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, 0);
    XEvent* xevent = xev;
    xevent->xkey.keycode =
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::CONTROL_LEFT);
    ui::KeyEvent event(xev);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    // ui::VKEY_LCONTROL, instead of ui::VKEY_CONTROL, should be filled.
    EXPECT_EQ(ui::VKEY_LCONTROL, webkit_event.windowsKeyCode);
  }
  {
    // Press right Ctrl.
    xev.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, 0);
    XEvent* xevent = xev;
    xevent->xkey.keycode = ui::KeycodeConverter::DomCodeToNativeKeycode(
        ui::DomCode::CONTROL_RIGHT);
    ui::KeyEvent event(xev);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    // ui::VKEY_RCONTROL, instead of ui::VKEY_CONTROL, should be filled.
    EXPECT_EQ(ui::VKEY_RCONTROL, webkit_event.windowsKeyCode);
  }
#elif defined(OS_WIN)
  // TODO(yusukes): Add tests for win_aura once keyboardEvent() in
  // third_party/WebKit/Source/web/win/WebInputEventFactory.cpp is modified
  // to return VKEY_[LR]XXX instead of VKEY_XXX.
  // https://bugs.webkit.org/show_bug.cgi?id=86694
#endif
  {
    // Press left Ctrl.
    ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL,
                       ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
                       ui::DomKey::CONTROL, 0, ui::EventTimeForNow());
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    // ui::VKEY_LCONTROL, instead of ui::VKEY_CONTROL, should be filled.
    EXPECT_EQ(ui::VKEY_LCONTROL, webkit_event.windowsKeyCode);
  }
  {
    // Press right Ctrl.
    ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL,
                       ui::DomCode::CONTROL_RIGHT, ui::EF_CONTROL_DOWN,
                       ui::DomKey::CONTROL, 0, ui::EventTimeForNow());
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    // ui::VKEY_RCONTROL, instead of ui::VKEY_CONTROL, should be filled.
    EXPECT_EQ(ui::VKEY_RCONTROL, webkit_event.windowsKeyCode);
  }
}

// Checks that MakeWebKeyboardEvent fills a correct keypad modifier.
TEST(WebInputEventAuraTest, TestMakeWebKeyboardEventKeyPadKeyCode) {
#if defined(USE_X11)
#define XK(x) XK_##x
#else
#define XK(x) 0
#endif
  struct TestCase {
    ui::DomCode dom_code;         // The physical key (location).
    ui::KeyboardCode ui_keycode;  // The virtual key code.
    uint32 x_keysym;              // The X11 keysym.
    bool expected_result;         // true if the event has "isKeyPad" modifier.
  } kTesCases[] = {
      {ui::DomCode::DIGIT0, ui::VKEY_0, XK(0), false},
      {ui::DomCode::DIGIT1, ui::VKEY_1, XK(1), false},
      {ui::DomCode::DIGIT2, ui::VKEY_2, XK(2), false},
      {ui::DomCode::DIGIT3, ui::VKEY_3, XK(3), false},
      {ui::DomCode::DIGIT4, ui::VKEY_4, XK(4), false},
      {ui::DomCode::DIGIT5, ui::VKEY_5, XK(5), false},
      {ui::DomCode::DIGIT6, ui::VKEY_6, XK(6), false},
      {ui::DomCode::DIGIT7, ui::VKEY_7, XK(7), false},
      {ui::DomCode::DIGIT8, ui::VKEY_8, XK(8), false},
      {ui::DomCode::DIGIT9, ui::VKEY_9, XK(9), false},

      {ui::DomCode::NUMPAD0, ui::VKEY_NUMPAD0, XK(KP_0), true},
      {ui::DomCode::NUMPAD1, ui::VKEY_NUMPAD1, XK(KP_1), true},
      {ui::DomCode::NUMPAD2, ui::VKEY_NUMPAD2, XK(KP_2), true},
      {ui::DomCode::NUMPAD3, ui::VKEY_NUMPAD3, XK(KP_3), true},
      {ui::DomCode::NUMPAD4, ui::VKEY_NUMPAD4, XK(KP_4), true},
      {ui::DomCode::NUMPAD5, ui::VKEY_NUMPAD5, XK(KP_5), true},
      {ui::DomCode::NUMPAD6, ui::VKEY_NUMPAD6, XK(KP_6), true},
      {ui::DomCode::NUMPAD7, ui::VKEY_NUMPAD7, XK(KP_7), true},
      {ui::DomCode::NUMPAD8, ui::VKEY_NUMPAD8, XK(KP_8), true},
      {ui::DomCode::NUMPAD9, ui::VKEY_NUMPAD9, XK(KP_9), true},

      {ui::DomCode::NUMPAD_MULTIPLY, ui::VKEY_MULTIPLY, XK(KP_Multiply), true},
      {ui::DomCode::NUMPAD_SUBTRACT, ui::VKEY_SUBTRACT, XK(KP_Subtract), true},
      {ui::DomCode::NUMPAD_ADD, ui::VKEY_ADD, XK(KP_Add), true},
      {ui::DomCode::NUMPAD_DIVIDE, ui::VKEY_DIVIDE, XK(KP_Divide), true},
      {ui::DomCode::NUMPAD_DECIMAL, ui::VKEY_DECIMAL, XK(KP_Decimal), true},
      {ui::DomCode::NUMPAD_DECIMAL, ui::VKEY_DELETE, XK(KP_Delete), true},
      {ui::DomCode::NUMPAD0, ui::VKEY_INSERT, XK(KP_Insert), true},
      {ui::DomCode::NUMPAD1, ui::VKEY_END, XK(KP_End), true},
      {ui::DomCode::NUMPAD2, ui::VKEY_DOWN, XK(KP_Down), true},
      {ui::DomCode::NUMPAD3, ui::VKEY_NEXT, XK(KP_Page_Down), true},
      {ui::DomCode::NUMPAD4, ui::VKEY_LEFT, XK(KP_Left), true},
      {ui::DomCode::NUMPAD5, ui::VKEY_CLEAR, XK(KP_Begin), true},
      {ui::DomCode::NUMPAD6, ui::VKEY_RIGHT, XK(KP_Right), true},
      {ui::DomCode::NUMPAD7, ui::VKEY_HOME, XK(KP_Home), true},
      {ui::DomCode::NUMPAD8, ui::VKEY_UP, XK(KP_Up), true},
      {ui::DomCode::NUMPAD9, ui::VKEY_PRIOR, XK(KP_Page_Up), true},
  };
  for (const auto& test_case : kTesCases) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, test_case.ui_keycode,
                       test_case.dom_code, ui::EF_NONE);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    EXPECT_EQ(test_case.expected_result,
              (webkit_event.modifiers & blink::WebInputEvent::IsKeyPad) != 0)
        << "Failed in "
        << "{dom_code:"
        << ui::KeycodeConverter::DomCodeToCodeString(test_case.dom_code)
        << ", ui_keycode:" << test_case.ui_keycode
        << "}, expect: " << test_case.expected_result;
  }
#if defined(USE_X11)
  ui::ScopedXI2Event xev;
  for (size_t i = 0; i < arraysize(kTesCases); ++i) {
    const TestCase& test_case = kTesCases[i];

    xev.InitKeyEvent(ui::ET_KEY_PRESSED, test_case.ui_keycode, ui::EF_NONE);
    XEvent* xevent = xev;
    xevent->xkey.keycode =
        XKeysymToKeycode(gfx::GetXDisplay(), test_case.x_keysym);
    if (!xevent->xkey.keycode)
      continue;
    ui::KeyEvent event(xev);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(event);
    EXPECT_EQ(test_case.expected_result,
              (webkit_event.modifiers & blink::WebInputEvent::IsKeyPad) != 0)
        << "Failed in " << i << "th test case: "
        << "{dom_code:"
        << ui::KeycodeConverter::DomCodeToCodeString(test_case.dom_code)
        << ", ui_keycode:" << test_case.ui_keycode
        << ", x_keysym:" << test_case.x_keysym
        << "}, expect: " << test_case.expected_result;
  }
#endif
}

}  // namespace content
