// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_dialog.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/android/vr/vr_shell_delegate.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"

using base::android::JavaParamRef;

namespace vr {

VrDialog::VrDialog(int width, int height) {
  width_ = width;
  height_ = height;
}

VrDialog::~VrDialog() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
}

void VrDialog::SetSize(int width, int height) {
  width_ = width;
  height_ = height;
}

void VrDialog::SendGestureToTarget(
    std::unique_ptr<blink::WebInputEvent> event) {
  if (!event || !dialog_)
    return;
  // TODO(asimjour): support switching between dialogs.
  dialog_->ForwardDialogEvent(std::move(event));
}

void VrDialog::SetEventForwarder(ContentInputForwarder* dialog) {
  dialog_ = dialog;
}

std::unique_ptr<blink::WebMouseEvent> VrDialog::MakeMouseEvent(
    blink::WebInputEvent::Type type,
    const gfx::PointF& normalized_web_content_location) {
  gfx::Point location(width_ * normalized_web_content_location.x(),
                      height_ * normalized_web_content_location.y());
  blink::WebInputEvent::Modifiers modifiers =
      blink::WebInputEvent::kNoModifiers;

  auto mouse_event = std::make_unique<blink::WebMouseEvent>(
      type, modifiers, base::TimeTicks::Now());
  mouse_event->pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  mouse_event->button = blink::WebPointerProperties::Button::kLeft;
  mouse_event->SetPositionInWidget(location.x(), location.y());
  mouse_event->click_count = 1;
  return mouse_event;
}

void VrDialog::UpdateGesture(const gfx::PointF& normalized_content_hit_point,
                             blink::WebGestureEvent& gesture) {
  gesture.SetPositionInWidget(
      ScalePoint(normalized_content_hit_point, width_, height_));
}

}  //  namespace vr
