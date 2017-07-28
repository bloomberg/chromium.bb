// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/content_element.h"

#include "third_party/WebKit/public/platform/WebGestureEvent.h"

namespace vr {

ContentElement::ContentElement(ContentInputDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
  // TODO(vollick): the content element should draw itself through a new
  // function on the ui element renderer similar to that used by the textured
  // element, but allowing for external textures.
  set_fill(Fill::CONTENT);
  set_scrollable(true);
}

ContentElement::~ContentElement() = default;

void ContentElement::OnHoverEnter(const gfx::PointF& position) {
  delegate_->OnContentEnter(position);
}

void ContentElement::OnHoverLeave() {
  delegate_->OnContentLeave();
}

void ContentElement::OnMove(const gfx::PointF& position) {
  delegate_->OnContentMove(position);
}

void ContentElement::OnButtonDown(const gfx::PointF& position) {
  delegate_->OnContentDown(position);
}

void ContentElement::OnButtonUp(const gfx::PointF& position) {
  delegate_->OnContentUp(position);
}

void ContentElement::OnFlingStart(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& position) {
  delegate_->OnContentFlingStart(std::move(gesture), position);
}
void ContentElement::OnFlingCancel(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& position) {
  delegate_->OnContentFlingCancel(std::move(gesture), position);
}
void ContentElement::OnScrollBegin(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& position) {
  delegate_->OnContentScrollBegin(std::move(gesture), position);
}
void ContentElement::OnScrollUpdate(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& position) {
  delegate_->OnContentScrollUpdate(std::move(gesture), position);
}
void ContentElement::OnScrollEnd(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& position) {
  delegate_->OnContentScrollEnd(std::move(gesture), position);
}

}  // namespace vr
