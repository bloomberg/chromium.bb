// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "content/common/input_messages.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/event.h"
#include "ui/events/latency_info.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;

namespace content {
namespace {

// This value was determined experimentally. It was sufficient to not cause a
// fling on Android.
const int kPointerAssumedStoppedTimeMs = 50;

// SyntheticGestureTargetBase passes input events straight on to the renderer
// without going through a gesture recognition framework. There is thus no touch
// slop.
const int kTouchSlopInDips = 0;

}  // namespace

SyntheticGestureTargetBase::SyntheticGestureTargetBase(
    RenderWidgetHostImpl* host)
    : host_(host) {
  DCHECK(host);
}

SyntheticGestureTargetBase::~SyntheticGestureTargetBase() {
}

void SyntheticGestureTargetBase::DispatchInputEventToPlatform(
    const WebInputEvent& event) {
  TRACE_EVENT1("benchmark",
               "SyntheticGestureTarget::DispatchInputEventToPlatform",
               "type", WebInputEventTraits::GetName(event.type));

  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);

  if (WebInputEvent::isTouchEventType(event.type)) {
    DCHECK(SupportsSyntheticGestureSourceType(
            SyntheticGestureParams::TOUCH_INPUT));

    const WebTouchEvent& web_touch =
        static_cast<const WebTouchEvent&>(event);
    DispatchWebTouchEventToPlatform(web_touch, latency_info);
  } else if (event.type == WebInputEvent::MouseWheel) {
    DCHECK(SupportsSyntheticGestureSourceType(
            SyntheticGestureParams::MOUSE_INPUT));

    const WebMouseWheelEvent& web_wheel =
        static_cast<const WebMouseWheelEvent&>(event);
    DispatchWebMouseWheelEventToPlatform(web_wheel, latency_info);
  } else if (WebInputEvent::isMouseEventType(event.type)) {
    DCHECK(SupportsSyntheticGestureSourceType(
            SyntheticGestureParams::MOUSE_INPUT));

    const WebMouseEvent& web_mouse =
        static_cast<const WebMouseEvent&>(event);
    DispatchWebMouseEventToPlatform(web_mouse, latency_info);
  } else {
    NOTREACHED();
  }
}

void SyntheticGestureTargetBase::DispatchWebTouchEventToPlatform(
      const blink::WebTouchEvent& web_touch,
      const ui::LatencyInfo& latency_info) {
  host_->ForwardTouchEventWithLatencyInfo(web_touch, latency_info);
}

void SyntheticGestureTargetBase::DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo& latency_info) {
  host_->ForwardWheelEventWithLatencyInfo(web_wheel, latency_info);
}

void SyntheticGestureTargetBase::DispatchWebMouseEventToPlatform(
      const blink::WebMouseEvent& web_mouse,
      const ui::LatencyInfo& latency_info) {
  host_->ForwardMouseEventWithLatencyInfo(web_mouse, latency_info);
}

void SyntheticGestureTargetBase::OnSyntheticGestureCompleted(
    SyntheticGesture::Result result) {
  host_->Send(new InputMsg_SyntheticGestureCompleted(host_->GetRoutingID()));
}

void SyntheticGestureTargetBase::SetNeedsFlush() {
  host_->SetNeedsFlush();
}

SyntheticGestureParams::GestureSourceType
SyntheticGestureTargetBase::GetDefaultSyntheticGestureSourceType() const {
  return SyntheticGestureParams::MOUSE_INPUT;
}

bool SyntheticGestureTargetBase::SupportsSyntheticGestureSourceType(
    SyntheticGestureParams::GestureSourceType gesture_source_type) const {
  return gesture_source_type == SyntheticGestureParams::MOUSE_INPUT ||
      gesture_source_type == SyntheticGestureParams::TOUCH_INPUT;
}

base::TimeDelta SyntheticGestureTargetBase::PointerAssumedStoppedTime()
    const {
  return base::TimeDelta::FromMilliseconds(kPointerAssumedStoppedTimeMs);
}

int SyntheticGestureTargetBase::GetTouchSlopInDips() const {
  return kTouchSlopInDips;
}

}  // namespace content
