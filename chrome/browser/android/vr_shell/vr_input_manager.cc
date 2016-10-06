// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_input_manager.h"

#include "base/task_runner_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

using blink::WebGestureEvent;
using blink::WebMouseEvent;
using blink::WebInputEvent;

namespace vr_shell {

VrInputManager::VrInputManager(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  dpi_scale_ = 1;
}

VrInputManager::~VrInputManager() {}

void VrInputManager::ProcessUpdatedGesture(VrGesture gesture) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&VrInputManager::SendGesture, this, gesture));
  } else {
    SendGesture(gesture);
  }
}

void VrInputManager::SendGesture(VrGesture gesture) {
  int64_t event_time = gesture.start_time;
  int64_t event_time_milliseconds = static_cast<int64_t>(event_time / 1000000);

  if (gesture.type == WebInputEvent::GestureScrollBegin ||
      gesture.type == WebInputEvent::GestureScrollUpdate ||
      gesture.type == WebInputEvent::GestureScrollEnd) {
    SendScrollEvent(event_time_milliseconds, 0.0f, 0.0f,
                    gesture.details.scroll.delta.x,
                    gesture.details.scroll.delta.y, gesture.type);
  } else if (gesture.type == WebInputEvent::GestureTap) {
    SendClickEvent(event_time_milliseconds, gesture.details.buttons.pos.x,
                   gesture.details.buttons.pos.y);
  } else if (gesture.type == WebInputEvent::MouseMove ||
             gesture.type == WebInputEvent::MouseEnter ||
             gesture.type == WebInputEvent::MouseLeave) {
    SendMouseEvent(event_time_milliseconds, gesture.details.move.delta.x,
                   gesture.details.move.delta.y, gesture.type);
  }
}

void VrInputManager::ScrollBegin(int64_t time_ms,
                                 float x,
                                 float y,
                                 float hintx,
                                 float hinty,
                                 bool target_viewport) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::GestureScrollBegin, time_ms, x, y);
  event.data.scrollBegin.deltaXHint = hintx / dpi_scale_;
  event.data.scrollBegin.deltaYHint = hinty / dpi_scale_;
  event.data.scrollBegin.targetViewport = target_viewport;

  ForwardGestureEvent(event);
}

void VrInputManager::ScrollEnd(int64_t time_ms) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::GestureScrollEnd, time_ms, 0, 0);
  ForwardGestureEvent(event);
}

void VrInputManager::ScrollBy(int64_t time_ms,
                              float x,
                              float y,
                              float dx,
                              float dy) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::GestureScrollUpdate, time_ms, x, y);
  event.data.scrollUpdate.deltaX = -dx / dpi_scale_;
  event.data.scrollUpdate.deltaY = -dy / dpi_scale_;

  ForwardGestureEvent(event);
}

void VrInputManager::SendScrollEvent(int64_t time_ms,
                                     float x,
                                     float y,
                                     float dx,
                                     float dy,
                                     int type) {
  float hinty = -dy;
  float hintx = -dx;
  bool target_viewport = false;
  switch (type) {
    case WebInputEvent::GestureScrollBegin: {
      WebGestureEvent event_start =
          MakeGestureEvent(WebInputEvent::GestureScrollBegin, time_ms, x, y);
      event_start.data.scrollBegin.deltaXHint = hintx / dpi_scale_;
      event_start.data.scrollBegin.deltaYHint = hinty / dpi_scale_;
      event_start.data.scrollBegin.targetViewport = target_viewport;
      ForwardGestureEvent(event_start);
      break;
    }
    case WebInputEvent::GestureScrollUpdate: {
      WebGestureEvent event =
          MakeGestureEvent(WebInputEvent::GestureScrollUpdate, time_ms, x, y);
      event.data.scrollUpdate.deltaX = -dx / dpi_scale_;
      event.data.scrollUpdate.deltaY = -dy / dpi_scale_;
      ForwardGestureEvent(event);
      break;
    }
    case WebInputEvent::GestureScrollEnd: {
      WebGestureEvent event_end =
          MakeGestureEvent(WebInputEvent::GestureScrollEnd, time_ms, 0, 0);
      ForwardGestureEvent(event_end);
      break;
    }
  }
}

void VrInputManager::SendMouseEvent(int64_t time_ms,
                                    float x,
                                    float y,
                                    WebInputEvent::Type type) {
  WebMouseEvent result;

  result.type = type;
  result.pointerType = blink::WebPointerProperties::PointerType::Mouse;
  result.x = x / dpi_scale_;
  result.y = y / dpi_scale_;
  result.windowX = x / dpi_scale_;
  result.windowY = y / dpi_scale_;
  result.timeStampSeconds = time_ms / 1000.0;
  result.clickCount = 1;
  result.modifiers = 0;
  result.button = WebMouseEvent::Button::NoButton;

  ForwardMouseEvent(result);
}

void VrInputManager::SendClickEvent(int64_t time_ms, float x, float y) {
  WebGestureEvent tap_down_event =
      MakeGestureEvent(WebInputEvent::GestureTapDown, time_ms, x, y);
  tap_down_event.data.tap.tapCount = 1;
  ForwardGestureEvent(tap_down_event);

  WebGestureEvent tap_event =
      MakeGestureEvent(WebInputEvent::GestureTap, time_ms, x, y);
  tap_event.data.tap.tapCount = 1;
  ForwardGestureEvent(tap_event);
}

void VrInputManager::PinchBegin(int64_t time_ms, float x, float y) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::GesturePinchBegin, time_ms, x, y);
  ForwardGestureEvent(event);
}

void VrInputManager::PinchEnd(int64_t time_ms) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::GesturePinchEnd, time_ms, 0, 0);
  ForwardGestureEvent(event);
}

void VrInputManager::PinchBy(int64_t time_ms,
                             float anchor_x,
                             float anchor_y,
                             float delta) {
  WebGestureEvent event = MakeGestureEvent(WebInputEvent::GesturePinchUpdate,
                                           time_ms, anchor_x, anchor_y);
  event.data.pinchUpdate.scale = delta;

  ForwardGestureEvent(event);
}

void VrInputManager::SendPinchEvent(int64_t time_ms,
                                    float x,
                                    float y,
                                    float dz,
                                    int type) {
  switch (type) {
    case WebInputEvent::GesturePinchBegin: {
      WebGestureEvent event_start =
          MakeGestureEvent(WebInputEvent::GesturePinchBegin, time_ms, x, y);
      ForwardGestureEvent(event_start);
      break;
    }
    case WebInputEvent::GesturePinchUpdate: {
      WebGestureEvent event =
          MakeGestureEvent(WebInputEvent::GesturePinchUpdate, time_ms, x, y);
      event.data.pinchUpdate.scale = dz;

      ForwardGestureEvent(event);
      break;
    }
    case WebInputEvent::GesturePinchEnd: {
      WebGestureEvent event_end =
          MakeGestureEvent(WebInputEvent::GesturePinchEnd, time_ms, 0, 0);
      ForwardGestureEvent(event_end);
      break;
    }
  }
}

WebGestureEvent VrInputManager::MakeGestureEvent(WebInputEvent::Type type,
                                                 int64_t time_ms,
                                                 float x,
                                                 float y) const {
  WebGestureEvent result;

  result.type = type;
  result.x = x / dpi_scale_;
  result.y = y / dpi_scale_;
  result.timeStampSeconds = time_ms / 1000.0;
  result.sourceDevice = blink::WebGestureDeviceTouchscreen;

  return result;
}

void VrInputManager::ForwardGestureEvent(const blink::WebGestureEvent& event) {
  content::RenderWidgetHost* rwh =
      web_contents_->GetRenderWidgetHostView()->GetRenderWidgetHost();
  if (rwh)
    rwh->ForwardGestureEvent(event);
}

void VrInputManager::ForwardMouseEvent(const blink::WebMouseEvent& event) {
  content::RenderWidgetHost* rwh =
      web_contents_->GetRenderWidgetHostView()->GetRenderWidgetHost();
  if (rwh)
    rwh->ForwardMouseEvent(event);
}

}  // namespace vr_shell
