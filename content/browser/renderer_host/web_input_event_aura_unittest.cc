// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_input_event_aura.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/event.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/base/x/x11_util.h"
#endif

namespace content {

// Checks that MakeWebKeyboardEvent makes a DOM3 spec compliant key event.
// crbug.com/127142
TEST(WebInputEventAuraTest, TestMakeWebKeyboardEvent) {
#if defined(USE_X11)
  XEvent xev;
  {
    // Press Ctrl.
    ui::InitXKeyEventForTesting(ui::ET_KEY_PRESSED,
                                ui::VKEY_CONTROL,
                                0,  // X does not set ControlMask for KeyPress.
                                &xev);
    aura::KeyEvent event(&xev, false /* is_char */);
    WebKit::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(&event);
    // However, modifier bit for Control in |webkit_event| should be set.
    EXPECT_EQ(webkit_event.modifiers, WebKit::WebInputEvent::ControlKey);
  }
  {
    // Release Ctrl.
    ui::InitXKeyEventForTesting(ui::ET_KEY_RELEASED,
                                ui::VKEY_CONTROL,
                                ControlMask,  // X sets the mask for KeyRelease.
                                &xev);
    aura::KeyEvent event(&xev, false /* is_char */);
    WebKit::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(&event);
    // However, modifier bit for Control in |webkit_event| shouldn't be set.
    EXPECT_EQ(webkit_event.modifiers, 0);
  }
#endif
}

}  // namespace content
