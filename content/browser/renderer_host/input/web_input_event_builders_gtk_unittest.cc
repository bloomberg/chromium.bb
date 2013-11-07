// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_input_event_builders_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/keycodes/keyboard_code_conversion_gtk.h"

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using content::WebMouseEventBuilder;
using content::WebKeyboardEventBuilder;

namespace {

TEST(WebMouseEventBuilderTest, DoubleClick) {
  GdkEventButton first_click;
  first_click.type = GDK_BUTTON_PRESS;
  first_click.window = static_cast<GdkWindow*>(GINT_TO_POINTER(1));
  first_click.x = first_click.y = first_click.x_root = first_click.y_root = 100;
  first_click.state = 0;
  first_click.time = 0;
  first_click.button = 1;

  // Single click works.
  WebMouseEvent first_click_events =
      WebMouseEventBuilder::Build(&first_click);
  EXPECT_EQ(1, first_click_events.clickCount);

  // Make sure double click works.
  GdkEventButton second_click = first_click;
  second_click.time = first_click.time + 100;
  WebMouseEvent second_click_events =
      WebMouseEventBuilder::Build(&second_click);
  EXPECT_EQ(2, second_click_events.clickCount);

  // Reset the click count.
  first_click.time += 10000;
  first_click_events = WebMouseEventBuilder::Build(&first_click);
  EXPECT_EQ(1, first_click_events.clickCount);

  // Two clicks with a long gap in between aren't counted as a double click.
  second_click = first_click;
  second_click.time = first_click.time + 1000;
  second_click_events = WebMouseEventBuilder::Build(&second_click);
  EXPECT_EQ(1, second_click_events.clickCount);

  // Reset the click count.
  first_click.time += 10000;
  first_click_events = WebMouseEventBuilder::Build(&first_click);
  EXPECT_EQ(1, first_click_events.clickCount);

  // Two clicks far apart (horizontally) aren't counted as a double click.
  second_click = first_click;
  second_click.time = first_click.time + 1;
  second_click.x = first_click.x + 100;
  second_click_events = WebMouseEventBuilder::Build(&second_click);
  EXPECT_EQ(1, second_click_events.clickCount);

  // Reset the click count.
  first_click.time += 10000;
  first_click_events = WebMouseEventBuilder::Build(&first_click);
  EXPECT_EQ(1, first_click_events.clickCount);

  // Two clicks far apart (vertically) aren't counted as a double click.
  second_click = first_click;
  second_click.time = first_click.time + 1;
  second_click.x = first_click.y + 100;
  second_click_events = WebMouseEventBuilder::Build(&second_click);
  EXPECT_EQ(1, second_click_events.clickCount);

  // Reset the click count.
  first_click.time += 10000;
  first_click_events = WebMouseEventBuilder::Build(&first_click);
  EXPECT_EQ(1, first_click_events.clickCount);

  // Two clicks on different windows aren't a double click.
  second_click = first_click;
  second_click.time = first_click.time + 1;
  second_click.window = static_cast<GdkWindow*>(GINT_TO_POINTER(2));
  second_click_events = WebMouseEventBuilder::Build(&second_click);
  EXPECT_EQ(1, second_click_events.clickCount);
}

TEST(WebMouseEventBuilderTest, MouseUpClickCount) {
  GdkEventButton mouse_down;
  memset(&mouse_down, 0, sizeof(mouse_down));
  mouse_down.type = GDK_BUTTON_PRESS;
  mouse_down.window = static_cast<GdkWindow*>(GINT_TO_POINTER(1));
  mouse_down.x = mouse_down.y = mouse_down.x_root = mouse_down.y_root = 100;
  mouse_down.time = 0;
  mouse_down.button = 1;

  // Properly set the last click time, so that the internal state won't be
  // affected by previous tests.
  WebMouseEventBuilder::Build(&mouse_down);

  mouse_down.time += 10000;
  GdkEventButton mouse_up = mouse_down;
  mouse_up.type = GDK_BUTTON_RELEASE;
  WebMouseEvent mouse_down_event;
  WebMouseEvent mouse_up_event;

  // Click for three times.
  for (int i = 1; i < 4; ++i) {
    mouse_down.time += 100;
    mouse_down_event = WebMouseEventBuilder::Build(&mouse_down);
    EXPECT_EQ(i, mouse_down_event.clickCount);

    mouse_up.time = mouse_down.time + 50;
    mouse_up_event = WebMouseEventBuilder::Build(&mouse_up);
    EXPECT_EQ(i, mouse_up_event.clickCount);
  }

  // Reset the click count.
  mouse_down.time += 10000;
  mouse_down_event = WebMouseEventBuilder::Build(&mouse_down);
  EXPECT_EQ(1, mouse_down_event.clickCount);

  // Moving the cursor for a significant distance will reset the click count to
  // 0.
  GdkEventMotion mouse_move;
  memset(&mouse_move, 0, sizeof(mouse_move));
  mouse_move.type = GDK_MOTION_NOTIFY;
  mouse_move.window = mouse_down.window;
  mouse_move.time = mouse_down.time;
  mouse_move.x = mouse_move.y = mouse_move.x_root = mouse_move.y_root =
      mouse_down.x + 100;
  WebMouseEventBuilder::Build(&mouse_move);

  mouse_up.time = mouse_down.time + 50;
  mouse_up_event = WebMouseEventBuilder::Build(&mouse_up);
  EXPECT_EQ(0, mouse_up_event.clickCount);

  // Reset the click count.
  mouse_down.time += 10000;
  mouse_down_event = WebMouseEventBuilder::Build(&mouse_down);
  EXPECT_EQ(1, mouse_down_event.clickCount);

  // Moving the cursor with a significant delay will reset the click count to 0.
  mouse_move.time = mouse_down.time + 1000;
  mouse_move.x = mouse_move.y = mouse_move.x_root = mouse_move.y_root =
      mouse_down.x;
  WebMouseEventBuilder::Build(&mouse_move);

  mouse_up.time = mouse_move.time + 50;
  mouse_up_event = WebMouseEventBuilder::Build(&mouse_up);
  EXPECT_EQ(0, mouse_up_event.clickCount);
}

TEST(WebKeyboardEventBuilderTest, NumPadConversion) {
  // Construct a GDK input event for the numpad "5" key.
  char five[] = "5";
  GdkEventKey gdk_event;
  memset(&gdk_event, 0, sizeof(GdkEventKey));
  gdk_event.type = GDK_KEY_PRESS;
  gdk_event.keyval = GDK_KP_5;
  gdk_event.string = five;

  // Numpad flag should be set on the WebKeyboardEvent.
  WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(&gdk_event);
  EXPECT_TRUE(web_event.modifiers & WebInputEvent::IsKeyPad);
}

}  // anonymous namespace
