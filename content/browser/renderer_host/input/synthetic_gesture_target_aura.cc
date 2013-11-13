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
      root_window->GetDispatcher()->AsRootWindowHostDelegate();
  for (ScopedVector<ui::TouchEvent>::iterator iter = events.begin(),
      end = events.end(); iter != end; ++iter) {
    root_window_host_delegate->OnHostTouchEvent(*iter);
  }
}

SyntheticGestureParams::GestureSourceType
SyntheticGestureTargetAura::GetDefaultSyntheticGestureSourceType() const {
  // TODO(297960): Change this to MOUSE_INPUT when a native impl of
  // DispatchWebMouseWheelEventToPlatform is ready.
  return SyntheticGestureParams::TOUCH_INPUT;
}

bool SyntheticGestureTargetAura::SupportsSyntheticGestureSourceType(
    SyntheticGestureParams::GestureSourceType gesture_source_type) const {
  return gesture_source_type == SyntheticGestureParams::TOUCH_INPUT ||
      gesture_source_type == SyntheticGestureParams::MOUSE_INPUT;
}

}  // namespace content
