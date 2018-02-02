// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_dialog.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"

#include "third_party/WebKit/public/platform/WebMouseEvent.h"

using base::android::JavaParamRef;

namespace vr_shell {

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
void VrDialog::OnContentEnter(const gfx::PointF& normalized_hit_point) {}

void VrDialog::OnContentLeave() {}

void VrDialog::OnContentMove(const gfx::PointF& normalized_hit_point) {
  SendGestureToDialog(
      MakeMouseEvent(blink::WebInputEvent::kMouseMove, normalized_hit_point));
}

void VrDialog::OnContentDown(const gfx::PointF& normalized_hit_point) {
  SendGestureToDialog(
      MakeMouseEvent(blink::WebInputEvent::kMouseDown, normalized_hit_point));
}

void VrDialog::OnContentUp(const gfx::PointF& normalized_hit_point) {
  SendGestureToDialog(
      MakeMouseEvent(blink::WebInputEvent::kMouseUp, normalized_hit_point));
}

void VrDialog::OnContentFlingStart(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {}
void VrDialog::OnContentFlingCancel(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {}
void VrDialog::OnContentScrollBegin(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {}
void VrDialog::OnContentScrollUpdate(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {}
void VrDialog::OnContentScrollEnd(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {}

void VrDialog::SendGestureToDialog(
    std::unique_ptr<blink::WebInputEvent> event) {
  if (!event || !dialog_)
    return;
  // TODO(asimjour): support switching between dialogs.
  dialog_->ForwardDialogEvent(std::move(event));
}

void VrDialog::SetEventForwarder(vr::ContentInputForwarder* dialog) {
  dialog_ = dialog;
}

std::unique_ptr<blink::WebMouseEvent> VrDialog::MakeMouseEvent(
    blink::WebInputEvent::Type type,
    const gfx::PointF& normalized_web_content_location) {
  gfx::Point location(width_ * normalized_web_content_location.x(),
                      height_ * normalized_web_content_location.y());
  blink::WebInputEvent::Modifiers modifiers =
      blink::WebInputEvent::kNoModifiers;

  // timestamp is not used
  auto mouse_event = std::make_unique<blink::WebMouseEvent>(type, modifiers, 0);
  mouse_event->pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  mouse_event->button = blink::WebPointerProperties::Button::kLeft;
  mouse_event->SetPositionInWidget(location.x(), location.y());
  mouse_event->click_count = 1;
  return mouse_event;
}

}  //  namespace vr_shell
