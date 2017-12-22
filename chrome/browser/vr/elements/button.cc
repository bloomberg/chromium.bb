// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/button.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/invisible_hit_target.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/ui_scene_constants.h"

#include "ui/gfx/geometry/point_f.h"

namespace vr {

namespace {

constexpr float kHitPlaneScaleFactorHovered = 1.2f;
constexpr float kDefaultHoverOffsetDMM = 0.048f;

}  // namespace

Button::Button(base::RepeatingCallback<void()> click_handler)
    : click_handler_(click_handler), hover_offset_(kDefaultHoverOffsetDMM) {
  set_hit_testable(false);

  auto background = base::MakeUnique<Rect>();
  background->SetType(kTypeButtonBackground);
  background->set_bubble_events(true);
  background->set_contributes_to_parent_bounds(false);
  background->SetTransitionedProperties({TRANSFORM});
  background->set_hit_testable(false);
  background_ = background.get();
  AddChild(std::move(background));

  auto hit_plane = base::MakeUnique<InvisibleHitTarget>();
  hit_plane->SetType(kTypeButtonHitTarget);
  hit_plane->set_bubble_events(true);
  hit_plane->set_contributes_to_parent_bounds(false);
  hit_plane_ = hit_plane.get();
  AddChild(std::move(hit_plane));

  EventHandlers event_handlers;
  event_handlers.hover_enter =
      base::BindRepeating(&Button::HandleHoverEnter, base::Unretained(this));
  event_handlers.hover_move =
      base::BindRepeating(&Button::HandleHoverMove, base::Unretained(this));
  event_handlers.hover_leave =
      base::BindRepeating(&Button::HandleHoverLeave, base::Unretained(this));
  event_handlers.button_down =
      base::BindRepeating(&Button::HandleButtonDown, base::Unretained(this));
  event_handlers.button_up =
      base::BindRepeating(&Button::HandleButtonUp, base::Unretained(this));
  set_event_handlers(event_handlers);
}

Button::~Button() = default;

void Button::Render(UiElementRenderer* renderer,
                    const CameraModel& model) const {}

void Button::SetButtonColors(const ButtonColors& colors) {
  colors_ = colors;
  OnStateUpdated();
}

void Button::HandleHoverEnter() {
  hovered_ = true;
  OnStateUpdated();
}

void Button::HandleHoverMove(const gfx::PointF& position) {
  hovered_ = hit_plane_->LocalHitTest(position);
  OnStateUpdated();
}

void Button::HandleHoverLeave() {
  hovered_ = false;
  OnStateUpdated();
}

void Button::HandleButtonDown() {
  down_ = true;
  OnStateUpdated();
}

void Button::HandleButtonUp() {
  down_ = false;
  OnStateUpdated();
  if (hovered() && click_handler_)
    click_handler_.Run();
}

void Button::OnStateUpdated() {
  pressed_ = hovered_ ? down_ : false;

  if (hovered()) {
    background_->SetTranslate(0.0, 0.0, hover_offset_);
    hit_plane_->SetScale(kHitPlaneScaleFactorHovered,
                         kHitPlaneScaleFactorHovered, 1.0f);
  } else {
    background_->SetTranslate(0.0, 0.0, 0.0);
    hit_plane_->SetScale(1.0f, 1.0f, 1.0f);
  }
  background_->SetColor(colors_.GetBackgroundColor(hovered_, pressed_));
}

void Button::OnSetDrawPhase() {
  background_->SetDrawPhase(draw_phase());
  hit_plane_->SetDrawPhase(draw_phase());
}

void Button::OnSetName() {
  background_->set_owner_name_for_test(name());
  hit_plane_->set_owner_name_for_test(name());
}

void Button::OnSetSize(const gfx::SizeF& size) {
  background_->SetSize(size.width(), size.height());
  hit_plane_->SetSize(size.width(), size.height());
}

void Button::OnSetCornerRadii(const CornerRadii& radii) {
  background_->SetCornerRadii(radii);
  hit_plane_->SetCornerRadii(radii);
}

void Button::NotifyClientSizeAnimated(const gfx::SizeF& size,
                                      int target_property_id,
                                      cc::Animation* animation) {
  if (target_property_id == BOUNDS) {
    background_->SetSize(size.width(), size.height());
    hit_plane_->SetSize(size.width(), size.height());
  }
  UiElement::NotifyClientSizeAnimated(size, target_property_id, animation);
}

}  // namespace vr
