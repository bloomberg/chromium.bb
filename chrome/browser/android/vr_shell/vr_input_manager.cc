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

void VrInputManager::ProcessUpdatedGesture(const WebInputEvent& event) {
  base::Closure task;
  if (WebInputEvent::isMouseEventType(event.type)) {
    task = base::Bind(&VrInputManager::SendMouseEvent, this,
                      static_cast<const blink::WebMouseEvent&>(event));
  } else {
    task = base::Bind(&VrInputManager::SendGesture, this,
                      static_cast<const blink::WebGestureEvent&>(event));
  }
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     task);
  } else {
    if (WebInputEvent::isMouseEventType(event.type))
      SendMouseEvent(static_cast<const blink::WebMouseEvent&>(event));
    else if ((WebInputEvent::isGestureEventType(event.type)))
      SendGesture(static_cast<const blink::WebGestureEvent&>(event));
  }
  return;
}

void VrInputManager::SendGesture(const WebGestureEvent& gesture) {
  if (gesture.type != WebGestureEvent::GestureTapDown) {
    ForwardGestureEvent(gesture);
  } else {
    ForwardGestureEvent(gesture);

    // Generate and forward Tap
    WebGestureEvent tap_event = MakeGestureEvent(
        WebInputEvent::GestureTap, gesture.timeStampSeconds,
        gesture.data.tapDown.width, gesture.data.tapDown.height);
    tap_event.data.tap.tapCount = 1;
    ForwardGestureEvent(tap_event);
  }
}

void VrInputManager::SendMouseEvent(const WebMouseEvent& mouse_event) {
  ForwardMouseEvent(mouse_event);
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
  result.sourceDevice = blink::WebGestureDeviceTouchpad;

  return result;
}

void VrInputManager::ForwardGestureEvent(
    const blink::WebGestureEvent& gesture) {
  content::RenderWidgetHost* rwh =
      web_contents_->GetRenderWidgetHostView()->GetRenderWidgetHost();
  if (rwh)
    rwh->ForwardGestureEvent(gesture);
}

void VrInputManager::ForwardMouseEvent(
    const blink::WebMouseEvent& mouse_event) {
  content::RenderWidgetHost* rwh =
      web_contents_->GetRenderWidgetHostView()->GetRenderWidgetHost();
  if (rwh)
    rwh->ForwardMouseEvent(mouse_event);
}

}  // namespace vr_shell
