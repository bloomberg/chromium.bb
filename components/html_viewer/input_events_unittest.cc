// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "components/html_viewer/blink_input_events_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/events_test_utils.h"

using ui::EventTimeForNow;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;

namespace mojo {
namespace {

TEST(InputEventLibTest, MouseEventConversion) {
  scoped_ptr<ui::Event> mouseev(
      new ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(1, 2),
                         gfx::Point(3, 4), EventTimeForNow(), 0, 0));

  mus::mojom::EventPtr mojo_event(mus::mojom::Event::From(*mouseev));

  EXPECT_EQ(mus::mojom::EVENT_TYPE_POINTER_DOWN, mojo_event->action);
  EXPECT_EQ(mus::mojom::POINTER_KIND_MOUSE, mojo_event->pointer_data->kind);

  scoped_ptr<blink::WebInputEvent> webevent =
      mojo_event.To<scoped_ptr<blink::WebInputEvent>>();

  ASSERT_TRUE(webevent);
  EXPECT_EQ(WebInputEvent::MouseDown, webevent->type);

  scoped_ptr<WebMouseEvent> web_mouse_event(
      static_cast<WebMouseEvent*>(webevent.release()));

  EXPECT_EQ(1, web_mouse_event->x);
  EXPECT_EQ(2, web_mouse_event->y);
  EXPECT_EQ(3, web_mouse_event->globalX);
  EXPECT_EQ(4, web_mouse_event->globalY);
}

TEST(InputEventLibTest, MouseWheelEventConversionNonPrecise) {
  scoped_ptr<ui::Event> original_wheel(new ui::MouseWheelEvent(
      gfx::Vector2d(-1 * ui::MouseWheelEvent::kWheelDelta,
                    -2 * ui::MouseWheelEvent::kWheelDelta),
      gfx::Point(1, 2), gfx::Point(3, 4), EventTimeForNow(), 0, 0));

  mus::mojom::EventPtr mojo_event(mus::mojom::Event::From(*original_wheel));

  EXPECT_EQ(mus::mojom::EVENT_TYPE_WHEEL, mojo_event->action);

  // Exercise the blink converter.
  scoped_ptr<blink::WebInputEvent> webevent =
      mojo_event.To<scoped_ptr<blink::WebInputEvent>>();

  ASSERT_TRUE(webevent);
  EXPECT_EQ(WebInputEvent::MouseWheel, webevent->type);

  scoped_ptr<WebMouseWheelEvent> web_wheel(
      static_cast<WebMouseWheelEvent*>(webevent.release()));

  EXPECT_EQ(1, web_wheel->x);
  EXPECT_EQ(2, web_wheel->y);
  EXPECT_EQ(3, web_wheel->globalX);
  EXPECT_EQ(4, web_wheel->globalY);

  EXPECT_EQ(-1.0 * ui::MouseWheelEvent::kWheelDelta, web_wheel->deltaX);
  EXPECT_EQ(-2.0 * ui::MouseWheelEvent::kWheelDelta, web_wheel->deltaY);

  EXPECT_EQ(-1.0, web_wheel->wheelTicksX);
  EXPECT_EQ(-2.0, web_wheel->wheelTicksY);

  EXPECT_FALSE(web_wheel->scrollByPage);
  EXPECT_FALSE(web_wheel->hasPreciseScrollingDeltas);
  EXPECT_TRUE(web_wheel->canScroll);

  // Exercise the round-trip converter.
  mus::mojom::EventPtr mojo_other_event(
      mus::mojom::Event::From(*original_wheel));
  scoped_ptr<ui::Event> new_event =
      mojo_other_event.To<scoped_ptr<ui::Event>>();
  EXPECT_EQ(ui::ET_MOUSEWHEEL, new_event->type());

  scoped_ptr<ui::MouseWheelEvent> ui_wheel(
      static_cast<ui::MouseWheelEvent*>(new_event.release()));

  EXPECT_EQ(-1 * ui::MouseWheelEvent::kWheelDelta, ui_wheel->x_offset());
  EXPECT_EQ(-2 * ui::MouseWheelEvent::kWheelDelta, ui_wheel->y_offset());
}

}  // namespace
}  // namespace mojo
