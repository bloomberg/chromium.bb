// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_target_aura.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/aura/event_injector.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

using blink::WebTouchEvent;
using blink::WebMouseWheelEvent;

namespace content {

SyntheticGestureTargetAura::SyntheticGestureTargetAura(
    RenderWidgetHostImpl* host)
    : SyntheticGestureTargetBase(host) {
  ScreenInfo screen_info;
  host->GetScreenInfo(&screen_info);
  device_scale_factor_ = screen_info.device_scale_factor;
}

void SyntheticGestureTargetAura::DispatchWebTouchEventToPlatform(
    const WebTouchEvent& web_touch,
    const ui::LatencyInfo& latency_info) {
  TouchEventWithLatencyInfo touch_with_latency(web_touch, latency_info);
  for (size_t i = 0; i < touch_with_latency.event.touches_length; i++) {
    touch_with_latency.event.touches[i].radius_x *= device_scale_factor_;
    touch_with_latency.event.touches[i].radius_y *= device_scale_factor_;
  }
  std::vector<std::unique_ptr<ui::TouchEvent>> events;
  bool conversion_success = MakeUITouchEventsFromWebTouchEvents(
      touch_with_latency, &events, LOCAL_COORDINATES);
  DCHECK(conversion_success);

  aura::Window* window = GetWindow();
  aura::WindowTreeHost* host = window->GetHost();
  aura::EventInjector injector;

  for (const auto& event : events) {
    event->ConvertLocationToTarget(window, host->window());
    // Apply the screen scale factor to the event location after it has been
    // transformed to the target.
    gfx::PointF device_location =
        gfx::ScalePoint(event->location_f(), device_scale_factor_);
    gfx::PointF device_root_location =
        gfx::ScalePoint(event->root_location_f(), device_scale_factor_);
    event->set_location_f(device_location);
    event->set_root_location_f(device_root_location);
    ui::EventDispatchDetails details = injector.Inject(host, event.get());
    if (details.dispatcher_destroyed)
      break;
  }
}

void SyntheticGestureTargetAura::DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo&) {
  if (web_wheel.phase == blink::WebMouseWheelEvent::kPhaseEnded) {
    DCHECK(
        !render_widget_host()->GetView()->IsRenderWidgetHostViewChildFrame() &&
        !render_widget_host()->GetView()->IsRenderWidgetHostViewGuest());
    // Send the pending wheel end event immediately.
    static_cast<RenderWidgetHostViewAura*>(render_widget_host()->GetView())
        ->event_handler()
        ->mouse_wheel_phase_handler()
        .DispatchPendingWheelEndEvent();
    return;
  }
  base::TimeTicks timestamp =
      ui::EventTimeStampFromSeconds(web_wheel.TimeStampSeconds());
  ui::MouseWheelEvent wheel_event(
      gfx::Vector2d(web_wheel.delta_x, web_wheel.delta_y), gfx::Point(),
      gfx::Point(), timestamp, ui::EF_NONE, ui::EF_NONE);
  gfx::PointF location(web_wheel.PositionInWidget().x * device_scale_factor_,
                       web_wheel.PositionInWidget().y * device_scale_factor_);
  wheel_event.set_location_f(location);
  wheel_event.set_root_location_f(location);

  aura::Window* window = GetWindow();
  wheel_event.ConvertLocationToTarget(window, window->GetRootWindow());
  aura::EventInjector injector;
  ui::EventDispatchDetails details =
      injector.Inject(window->GetHost(), &wheel_event);
  if (details.dispatcher_destroyed)
    return;
}

namespace {

ui::EventType
WebMouseEventTypeToEventType(blink::WebInputEvent::Type web_type) {
  switch (web_type) {
    case blink::WebInputEvent::kMouseDown:
      return ui::ET_MOUSE_PRESSED;

    case blink::WebInputEvent::kMouseUp:
      return ui::ET_MOUSE_RELEASED;

    case blink::WebInputEvent::kMouseMove:
      return ui::ET_MOUSE_MOVED;

    case blink::WebInputEvent::kMouseEnter:
      return ui::ET_MOUSE_ENTERED;

    case blink::WebInputEvent::kMouseLeave:
      return ui::ET_MOUSE_EXITED;

    case blink::WebInputEvent::kContextMenu:
      NOTREACHED() << "WebInputEvent::ContextMenu not supported by"
          "SyntheticGestureTargetAura";

    default:
      NOTREACHED();
  }

  return ui::ET_UNKNOWN;
}

int WebEventModifiersToEventFlags(int modifiers) {
  int flags = 0;

  if (modifiers & blink::WebInputEvent::kLeftButtonDown)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::kMiddleButtonDown)
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::kRightButtonDown)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::kBackButtonDown)
    flags |= ui::EF_BACK_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::kForwardButtonDown)
    flags |= ui::EF_FORWARD_MOUSE_BUTTON;

  return flags;
}

ui::EventPointerType WebMousePointerTypeToEventPointerType(
    blink::WebPointerProperties::PointerType type) {
  if (type == blink::WebPointerProperties::PointerType::kMouse)
    return ui::EventPointerType::POINTER_TYPE_MOUSE;
  if (type == blink::WebPointerProperties::PointerType::kPen)
    return ui::EventPointerType::POINTER_TYPE_PEN;
  NOTREACHED() << "Unexpected mouse event pointer type";
  return ui::EventPointerType::POINTER_TYPE_UNKNOWN;
}

}  // namespace

void SyntheticGestureTargetAura::DispatchWebMouseEventToPlatform(
    const blink::WebMouseEvent& web_mouse_event,
    const ui::LatencyInfo& latency_info) {
  ui::EventType event_type =
      WebMouseEventTypeToEventType(web_mouse_event.GetType());
  int flags = WebEventModifiersToEventFlags(web_mouse_event.GetModifiers());
  ui::PointerDetails pointer_details(
      WebMousePointerTypeToEventPointerType(web_mouse_event.pointer_type));
  ui::MouseEvent mouse_event(event_type, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), flags, flags,
                             pointer_details);
  gfx::PointF location(
      web_mouse_event.PositionInWidget().x * device_scale_factor_,
      web_mouse_event.PositionInWidget().y * device_scale_factor_);
  mouse_event.set_location_f(location);
  mouse_event.set_root_location_f(location);

  aura::Window* window = GetWindow();
  mouse_event.ConvertLocationToTarget(window, window->GetRootWindow());
  aura::EventInjector injector;
  ui::EventDispatchDetails details =
      injector.Inject(window->GetHost(), &mouse_event);
  if (details.dispatcher_destroyed)
    return;
}

SyntheticGestureParams::GestureSourceType
SyntheticGestureTargetAura::GetDefaultSyntheticGestureSourceType() const {
  return SyntheticGestureParams::TOUCH_INPUT;
}

float SyntheticGestureTargetAura::GetTouchSlopInDips() const {
  // - 1 because Aura considers a pointer to be moving if it has moved at least
  // 'max_touch_move_in_pixels_for_click' pixels.
  return ui::GestureConfiguration::GetInstance()
             ->max_touch_move_in_pixels_for_click() -
         1;
}

float SyntheticGestureTargetAura::GetMinScalingSpanInDips() const {
  return ui::GestureConfiguration::GetInstance()
      ->min_distance_for_pinch_scroll_in_pixels();
}

aura::Window* SyntheticGestureTargetAura::GetWindow() const {
  aura::Window* window = render_widget_host()->GetView()->GetNativeView();
  DCHECK(window);
  return window;
}

}  // namespace content
