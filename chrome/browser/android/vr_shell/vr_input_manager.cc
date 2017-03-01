// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_input_manager.h"

#include <memory>

#include "base/task_runner_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/keycodes/dom/dom_key.h"

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
    : web_contents_(web_contents) {}

VrInputManager::~VrInputManager() = default;

void VrInputManager::ProcessUpdatedGesture(
    std::unique_ptr<blink::WebInputEvent> event) {
  if (WebInputEvent::isMouseEventType(event->type()))
    ForwardMouseEvent(static_cast<const blink::WebMouseEvent&>(*event));
  else if (WebInputEvent::isGestureEventType(event->type()))
    SendGesture(static_cast<const blink::WebGestureEvent&>(*event));
}

void VrInputManager::GenerateKeyboardEvent(int char_value, int modifiers) {
  content::NativeWebKeyboardEvent event(
      blink::WebInputEvent::Type::KeyDown, modifiers,
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF());
  event.domKey = ui::DomKey::FromCharacter(char_value);
  event.nativeKeyCode = char_value;
  event.windowsKeyCode = char_value;
  ForwardKeyboardEvent(event);

  event.setType(blink::WebInputEvent::Type::Char);
  event.text[0] = char_value;
  event.unmodifiedText[0] = char_value;
  event.domCode = char_value;
  ForwardKeyboardEvent(event);

  event.setType(blink::WebInputEvent::Type::KeyUp);
  ForwardKeyboardEvent(event);
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

void VrInputManager::ForwardKeyboardEvent(
    const content::NativeWebKeyboardEvent& keyboard_event) {
  content::RenderWidgetHostView* rwhv =
      web_contents_->GetRenderWidgetHostView();
  if (rwhv == nullptr)
    return;
  content::RenderWidgetHost* rwh = rwhv->GetRenderWidgetHost();
  if (rwh)
    rwh->ForwardKeyboardEvent(keyboard_event);
}

}  // namespace vr_shell
