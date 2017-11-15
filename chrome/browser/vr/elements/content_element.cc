// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/content_element.h"

#include "chrome/browser/vr/ui_element_renderer.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

ContentElement::ContentElement(ContentInputDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
  set_scrollable(true);
}

ContentElement::~ContentElement() = default;

void ContentElement::Render(UiElementRenderer* renderer,
                            const CameraModel& model) const {
  if (!texture_id_)
    return;
  gfx::RectF copy_rect(0, 0, 1, 1);
  renderer->DrawTexturedQuad(texture_id_, texture_location_,
                             model.view_proj_matrix * world_space_transform(),
                             copy_rect, computed_opacity(), size(),
                             corner_radius());
}

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

void ContentElement::SetTexture(unsigned int texture_id,
                                UiElementRenderer::TextureLocation location) {
  texture_id_ = texture_id;
  texture_location_ = location;
}

}  // namespace vr
