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

namespace {
WebGestureEvent MakeGestureEvent(WebInputEvent::Type type,
                                 double time,
                                 float x,
                                 float y) {
  WebGestureEvent result(type, WebInputEvent::NoModifiers, time);
  result.x = x;
  result.y = y;
  result.sourceDevice = blink::WebGestureDeviceTouchpad;
  return result;
}
}  // namespace

VrInputManager::VrInputManager(content::WebContents* web_contents)
    : web_contents_(web_contents),
      weak_ptr_factory_(this) {
}

VrInputManager::~VrInputManager() = default;

base::WeakPtr<VrInputManager> VrInputManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void VrInputManager::ProcessUpdatedGesture(
    std::unique_ptr<blink::WebInputEvent> event) {
  if (WebInputEvent::isMouseEventType(event->type())) {
    ForwardMouseEvent(static_cast<const blink::WebMouseEvent&>(*event));
  } else {
    SendGesture(static_cast<const blink::WebGestureEvent&>(*event));
  }
}

void VrInputManager::SendGesture(const WebGestureEvent& gesture) {
  if (gesture.type() == WebGestureEvent::GestureTapDown) {
    ForwardGestureEvent(gesture);

    // Generate and forward Tap
    WebGestureEvent tap_event =
        MakeGestureEvent(WebInputEvent::GestureTap, gesture.timeStampSeconds(),
                         gesture.x, gesture.y);
    tap_event.data.tap.tapCount = 1;
    ForwardGestureEvent(tap_event);
  } else {
    ForwardGestureEvent(gesture);
  }
}

void VrInputManager::ForwardGestureEvent(
    const blink::WebGestureEvent& gesture) {
  if (!web_contents_->GetRenderWidgetHostView())
    return;
  content::RenderWidgetHost* rwh =
      web_contents_->GetRenderWidgetHostView()->GetRenderWidgetHost();
  if (rwh)
    rwh->ForwardGestureEvent(gesture);
}

void VrInputManager::ForwardMouseEvent(
    const blink::WebMouseEvent& mouse_event) {
  if (!web_contents_->GetRenderWidgetHostView())
      return;
  content::RenderWidgetHost* rwh =
      web_contents_->GetRenderWidgetHostView()->GetRenderWidgetHost();
  if (rwh)
    rwh->ForwardMouseEvent(mouse_event);
}

}  // namespace vr_shell
