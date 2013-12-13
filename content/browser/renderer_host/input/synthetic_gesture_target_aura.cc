// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_target_aura.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "content/common/input/input_event.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/events/gestures/gesture_configuration.h"

using blink::WebTouchEvent;
using blink::WebMouseWheelEvent;

namespace content {

SyntheticGestureTargetAura::SyntheticGestureTargetAura(
    RenderWidgetHostImpl* host)
    : SyntheticGestureTargetBase(host) {
}

void SyntheticGestureTargetAura::DispatchWebTouchEventToPlatform(
    const WebTouchEvent& web_touch,
    const ui::LatencyInfo& latency_info) {
  aura::Window* window = render_widget_host()->GetView()->GetNativeView();
  aura::Window* root_window = window->GetRootWindow();
  aura::client::ScreenPositionClient* position_client =
      aura::client::GetScreenPositionClient(root_window);
  DCHECK(position_client);

  TouchEventWithLatencyInfo touch_with_latency(web_touch, latency_info);

  // SyntheticGesture may skip calculating screenPosition, so we will fill it
  // in here. "screenPosition" is converted from "position".
  const size_t num_touches = touch_with_latency.event.touchesLength;
  for (size_t i = 0; i < num_touches; ++ i) {
    blink::WebTouchPoint* point = &touch_with_latency.event.touches[i];
    gfx::Point position(point->position.x, point->position.y);
    position_client->ConvertPointToScreen(window, &position);
    point->screenPosition.x = position.x();
    point->screenPosition.y = position.y();
  }

  ScopedVector<ui::TouchEvent> events;
  bool conversion_success = MakeUITouchEventsFromWebTouchEvents(
      touch_with_latency, &events, SCREEN_COORDINATES);
  DCHECK(conversion_success);

  aura::RootWindowHostDelegate* root_window_host_delegate =
      GetRootWindowHostDelegate();
  for (ScopedVector<ui::TouchEvent>::iterator iter = events.begin(),
      end = events.end(); iter != end; ++iter) {
    root_window_host_delegate->OnHostTouchEvent(*iter);
  }
}

void SyntheticGestureTargetAura::DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo&) {
  gfx::Point location(web_wheel.x, web_wheel.y);
  ui::MouseEvent mouse_event(
      ui::ET_MOUSEWHEEL, location, location, ui::EF_NONE);
  ui::MouseWheelEvent wheel_event(
      mouse_event, web_wheel.deltaX, web_wheel.deltaY);

  GetRootWindowHostDelegate()->OnHostMouseEvent(&wheel_event);
}

namespace {

ui::EventType
WebMouseEventTypeToEventType(blink::WebInputEvent::Type web_type) {
  switch (web_type) {
    case blink::WebInputEvent::MouseDown:
      return ui::ET_MOUSE_PRESSED;

    case blink::WebInputEvent::MouseUp:
      return ui::ET_MOUSE_RELEASED;

    case blink::WebInputEvent::MouseMove:
      return ui::ET_MOUSE_MOVED;

    case blink::WebInputEvent::MouseEnter:
      return ui::ET_MOUSE_ENTERED;

    case blink::WebInputEvent::MouseLeave:
      return ui::ET_MOUSE_EXITED;

    case blink::WebInputEvent::ContextMenu:
      NOTREACHED() << "WebInputEvent::ContextMenu not supported by"
          "SyntheticGestureTargetAura";

    default:
      NOTREACHED();
  }

  return ui::ET_UNKNOWN;
}

int WebMouseEventButtonToFlags(blink::WebMouseEvent::Button button) {
  switch (button) {
    case blink::WebMouseEvent::ButtonLeft:
      return ui::EF_LEFT_MOUSE_BUTTON;

    case blink::WebMouseEvent::ButtonMiddle:
      return ui::EF_MIDDLE_MOUSE_BUTTON;

    case blink::WebMouseEvent::ButtonRight:
      return ui::EF_RIGHT_MOUSE_BUTTON;

    default:
      NOTREACHED();
  }

  return 0;
}

}  // namespace

void SyntheticGestureTargetAura::DispatchWebMouseEventToPlatform(
      const blink::WebMouseEvent& web_mouse,
      const ui::LatencyInfo& latency_info) {
  gfx::Point location(web_mouse.x, web_mouse.y);
  ui::EventType event_type = WebMouseEventTypeToEventType(web_mouse.type);
  int flags = WebMouseEventButtonToFlags(web_mouse.button);
  ui::MouseEvent mouse_event(event_type, location, location, flags);

  GetRootWindowHostDelegate()->OnHostMouseEvent(&mouse_event);
}

SyntheticGestureParams::GestureSourceType
SyntheticGestureTargetAura::GetDefaultSyntheticGestureSourceType() const {
  return SyntheticGestureParams::MOUSE_INPUT;
}

bool SyntheticGestureTargetAura::SupportsSyntheticGestureSourceType(
    SyntheticGestureParams::GestureSourceType gesture_source_type) const {
  return gesture_source_type == SyntheticGestureParams::TOUCH_INPUT ||
      gesture_source_type == SyntheticGestureParams::MOUSE_INPUT;
}

int SyntheticGestureTargetAura::GetTouchSlopInDips() const {
  // - 1 because Aura considers a pointer to be moving if it has moved at least
  // 'max_touch_move_in_pixels_for_click' pixels.
  return ui::GestureConfiguration::max_touch_move_in_pixels_for_click() - 1;
}

aura::RootWindowHostDelegate*
SyntheticGestureTargetAura::GetRootWindowHostDelegate() const {
  aura::Window* window = render_widget_host()->GetView()->GetNativeView();
  aura::Window* root_window = window->GetRootWindow();
  aura::RootWindowHostDelegate* root_window_host_delegate =
      root_window->GetDispatcher()->AsRootWindowHostDelegate();
  DCHECK(root_window_host_delegate);
  return root_window_host_delegate;
}


}  // namespace content
