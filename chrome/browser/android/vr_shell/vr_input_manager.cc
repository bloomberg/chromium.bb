// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/vr_shell/vr_input_manager.h"

using blink::WebGestureEvent;
using blink::WebMouseEvent;
using blink::WebInputEvent;

namespace vr_shell {

VrInputManager::VrInputManager(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  dpi_scale_ = 1;
}

VrInputManager::~VrInputManager() {}

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

  SendGestureEvent(event);
}

void VrInputManager::ScrollEnd(int64_t time_ms) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::GestureScrollEnd, time_ms, 0, 0);
  SendGestureEvent(event);
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

  SendGestureEvent(event);
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
      SendGestureEvent(event_start);
      break;
    }
    case WebInputEvent::GestureScrollUpdate: {
      WebGestureEvent event =
          MakeGestureEvent(WebInputEvent::GestureScrollUpdate, time_ms, x, y);
      event.data.scrollUpdate.deltaX = -dx / dpi_scale_;
      event.data.scrollUpdate.deltaY = -dy / dpi_scale_;
      SendGestureEvent(event);
      break;
    }
    case WebInputEvent::GestureScrollEnd: {
      WebGestureEvent event_end =
          MakeGestureEvent(WebInputEvent::GestureScrollEnd, time_ms, 0, 0);
      SendGestureEvent(event_end);
      break;
    }
  }
}

void VrInputManager::SendMouseMoveEvent(int64_t time_ms,
                                        float x,
                                        float y,
                                        int type) {
  WebMouseEvent result;

  result.type = WebInputEvent::MouseMove;
  result.pointerType = blink::WebPointerProperties::PointerType::Mouse;
  result.x = x / dpi_scale_;
  result.y = y / dpi_scale_;
  result.windowX = x / dpi_scale_;
  result.windowY = y / dpi_scale_;
  result.timeStampSeconds = time_ms / 1000.0;
  result.clickCount = 1;
  result.modifiers = 0;

  if (type == 1) {
    result.type = WebInputEvent::MouseEnter;
  } else if (type == 2) {
    result.type = WebInputEvent::MouseLeave;
  }
  result.button = WebMouseEvent::Button::NoButton;

  SendMouseEvent(result);
}

void VrInputManager::SendClickEvent(int64_t time_ms, float x, float y) {
  WebGestureEvent tap_down_event =
      MakeGestureEvent(WebInputEvent::GestureTapDown, time_ms, x, y);
  tap_down_event.data.tap.tapCount = 1;
  SendGestureEvent(tap_down_event);

  WebGestureEvent tap_event =
      MakeGestureEvent(WebInputEvent::GestureTap, time_ms, x, y);
  tap_event.data.tap.tapCount = 1;
  SendGestureEvent(tap_event);
}

void VrInputManager::PinchBegin(int64_t time_ms, float x, float y) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::GesturePinchBegin, time_ms, x, y);
  SendGestureEvent(event);
}

void VrInputManager::PinchEnd(int64_t time_ms) {
  WebGestureEvent event =
      MakeGestureEvent(WebInputEvent::GesturePinchEnd, time_ms, 0, 0);
  SendGestureEvent(event);
}

void VrInputManager::PinchBy(int64_t time_ms,
                             float anchor_x,
                             float anchor_y,
                             float delta) {
  WebGestureEvent event = MakeGestureEvent(WebInputEvent::GesturePinchUpdate,
                                           time_ms, anchor_x, anchor_y);
  event.data.pinchUpdate.scale = delta;

  SendGestureEvent(event);
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
      SendGestureEvent(event_start);
      break;
    }
    case WebInputEvent::GesturePinchUpdate: {
      WebGestureEvent event =
          MakeGestureEvent(WebInputEvent::GesturePinchUpdate, time_ms, x, y);
      event.data.pinchUpdate.scale = dz;

      SendGestureEvent(event);
      break;
    }
    case WebInputEvent::GesturePinchEnd: {
      WebGestureEvent event_end =
          MakeGestureEvent(WebInputEvent::GesturePinchEnd, time_ms, 0, 0);
      SendGestureEvent(event_end);
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

void VrInputManager::SendGestureEvent(const blink::WebGestureEvent& event) {
  content::RenderWidgetHost* rwh =
      web_contents_->GetRenderWidgetHostView()->GetRenderWidgetHost();
  if (rwh)
    rwh->ForwardGestureEvent(event);
}

void VrInputManager::SendMouseEvent(const blink::WebMouseEvent& event) {
  content::RenderWidgetHost* rwh =
      web_contents_->GetRenderWidgetHostView()->GetRenderWidgetHost();
  if (rwh)
    rwh->ForwardMouseEvent(event);
}

}  // namespace vr_shell
