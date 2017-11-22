// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/content_element.h"

#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/vr_gl_util.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

namespace {

// If the screen space bounds or the aspect ratio of the content quad change
// beyond these thresholds we propagate the new content bounds so that the
// content's resolution can be adjusted.
static constexpr float kContentBoundsPropagationThreshold = 0.2f;
// Changes of the aspect ratio lead to a
// distorted content much more quickly. Thus, have a smaller threshold here.
static constexpr float kContentAspectRatioPropagationThreshold = 0.01f;

}  // namespace

ContentElement::ContentElement(
    ContentInputDelegate* delegate,
    ContentElement::ScreenBoundsChangedCallback bounds_changed_callback)
    : delegate_(delegate), bounds_changed_callback_(bounds_changed_callback) {
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

void ContentElement::SetTextureId(unsigned int texture_id) {
  texture_id_ = texture_id;
}

void ContentElement::SetTextureLocation(
    UiElementRenderer::TextureLocation location) {
  texture_location_ = location;
}

void ContentElement::SetProjectionMatrix(const gfx::Transform& matrix) {
  projection_matrix_ = matrix;
}

bool ContentElement::OnBeginFrame(const base::TimeTicks& time,
                                  const gfx::Vector3dF& look_at) {
  // Determine if the projected size of the content quad changed more than a
  // given threshold. If so, propagate this info so that the content's
  // resolution and size can be adjusted. For the calculation, we cannot take
  // the content quad's actual size (main_content_->size()) if this property
  // is animated. If we took the actual size during an animation we would
  // surpass the threshold with differing projected sizes and aspect ratios
  // (depending on the animation's timing). The differing values may cause
  // visual artefacts if, for instance, the fullscreen aspect ratio is not 16:9.
  // As a workaround, take the final size of the content quad after the
  // animation as the basis for the calculation.
  gfx::SizeF target_size = GetTargetSize();
  // We take the target transform in case the content quad's parent's translate
  // is animated. This approach only works with the current scene hierarchy and
  // set of animated properties.
  gfx::Transform target_transform = ComputeTargetWorldSpaceTransform();
  gfx::SizeF screen_size =
      CalculateScreenSize(projection_matrix_, target_transform, target_size);

  float aspect_ratio = target_size.width() / target_size.height();
  gfx::SizeF screen_bounds;
  if (screen_size.width() < screen_size.height() * aspect_ratio) {
    screen_bounds.set_width(screen_size.height() * aspect_ratio);
    screen_bounds.set_height(screen_size.height());
  } else {
    screen_bounds.set_width(screen_size.width());
    screen_bounds.set_height(screen_size.width() / aspect_ratio);
  }

  if (std::abs(screen_bounds.width() - last_content_screen_bounds_.width()) >
          kContentBoundsPropagationThreshold ||
      std::abs(screen_bounds.height() - last_content_screen_bounds_.height()) >
          kContentBoundsPropagationThreshold ||
      std::abs(aspect_ratio - last_content_aspect_ratio_) >
          kContentAspectRatioPropagationThreshold) {
    bounds_changed_callback_.Run(screen_bounds);
    last_content_screen_bounds_.set_width(screen_bounds.width());
    last_content_screen_bounds_.set_height(screen_bounds.height());
    last_content_aspect_ratio_ = aspect_ratio;
    return true;
  }
  return false;
}

}  // namespace vr
