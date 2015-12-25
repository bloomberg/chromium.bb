// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/display.h"
#include "ui/gfx/switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "content/browser/renderer_host/input/web_input_event_builders_win.h"
#endif

using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;

namespace content {

#if defined(OS_WIN)
// This test validates that Pixel to DIP conversion occurs as needed in the
// WebMouseEventBuilder::Build function.
TEST(WebInputEventBuilderTest, TestMouseEventScale) {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  gfx::Display::ResetForceDeviceScaleFactorForTesting();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");

  // Synthesize a mouse move with x = 300 and y = 200.
  WebMouseEvent mouse_move =
      WebMouseEventBuilder::Build(::GetDesktopWindow(), WM_MOUSEMOVE, 0,
                                  MAKELPARAM(300, 200), 100);

  // The WebMouseEvent.x, WebMouseEvent.y, WebMouseEvent.windowX and
  // WebMouseEvent.windowY fields should be in pixels on return and hence
  // should be the same value as the x and y coordinates passed in to the
  // WebMouseEventBuilder::Build function.
  EXPECT_EQ(300, mouse_move.x);
  EXPECT_EQ(200, mouse_move.y);

  EXPECT_EQ(300, mouse_move.windowX);
  EXPECT_EQ(200, mouse_move.windowY);

  // WebMouseEvent.globalX and WebMouseEvent.globalY are calculated in DIPs.
  EXPECT_EQ(150, mouse_move.globalX);
  EXPECT_EQ(100, mouse_move.globalY);

  command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");
  gfx::Display::ResetForceDeviceScaleFactorForTesting();
}

// Test that hasPreciseScrollingDeltas is true for wheel events that generate
// high precision scroll deltas.
TEST(WebInputEventBuilderTest, TestPreciseScrollingDelta) {
  LPARAM lparam = MAKELPARAM(300, 200);

  // wheel_delta is equal to WHEEL_DELTA, scroll up
  WebMouseWheelEvent evt =
      WebMouseWheelEventBuilder::Build(::GetDesktopWindow(), WM_MOUSEWHEEL,
                                       MAKEWPARAM(0, 120), lparam, 100);
  EXPECT_EQ(1, evt.wheelTicksY);
  EXPECT_EQ(100, evt.deltaY);
  EXPECT_FALSE(evt.hasPreciseScrollingDeltas);

  // wheel_delta is a multiple of WHEEL_DELTA, scroll up
  evt = WebMouseWheelEventBuilder::Build(::GetDesktopWindow(), WM_MOUSEWHEEL,
                                         MAKEWPARAM(0, 360), lparam, 100);
  EXPECT_EQ(3, evt.wheelTicksY);
  EXPECT_EQ(300, evt.deltaY);
  EXPECT_FALSE(evt.hasPreciseScrollingDeltas);

  // wheel_delta is a multiple of WHEEL_DELTA, scroll down
  evt = WebMouseWheelEventBuilder::Build(::GetDesktopWindow(), WM_MOUSEWHEEL,
                                         MAKEWPARAM(0, -360), lparam, 100);
  EXPECT_EQ(-3, evt.wheelTicksY);
  EXPECT_EQ(-300, evt.deltaY);
  EXPECT_FALSE(evt.hasPreciseScrollingDeltas);

  // high precision scroll, wheel_delta not a multiple of WHEEL_DELTA
  evt = WebMouseWheelEventBuilder::Build(::GetDesktopWindow(), WM_MOUSEWHEEL,
                                         MAKEWPARAM(0, 3), lparam, 100);
  EXPECT_FLOAT_EQ(0.025f, evt.wheelTicksY);
  EXPECT_FLOAT_EQ(2.5f, evt.deltaY);
  EXPECT_TRUE(evt.hasPreciseScrollingDeltas);
}
#endif

}  // namespace content
