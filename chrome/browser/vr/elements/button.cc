// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/button.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/ui_scene_constants.h"

#include "ui/gfx/geometry/point_f.h"

namespace vr {

namespace {

constexpr float kIconScaleFactor = 0.5f;
constexpr float kHitPlaneScaleFactorHovered = 1.2f;
constexpr float kDefaultHoverOffsetDMM = 0.048f;
}  // namespace

Button::Button(base::Callback<void()> click_handler,
               const gfx::VectorIcon& icon)
    : click_handler_(click_handler), hover_offset_(kDefaultHoverOffsetDMM) {
  set_hit_testable(false);

  auto background = base::MakeUnique<Rect>();
  background->set_type(kTypeButtonBackground);
  background->set_bubble_events(true);
  background->SetTransitionedProperties({TRANSFORM});
  background->set_hit_testable(false);
  background_ = background.get();
  AddChild(std::move(background));

  auto vector_icon = base::MakeUnique<VectorIcon>(512);
  vector_icon->set_type(kTypeButtonForeground);
  vector_icon->SetIcon(icon);
  vector_icon->set_bubble_events(true);
  vector_icon->SetTransitionedProperties({TRANSFORM});
  vector_icon->set_hit_testable(false);
  foreground_ = vector_icon.get();
  AddChild(std::move(vector_icon));

  auto hit_plane = base::MakeUnique<InvisibleHitTarget>();
  hit_plane->set_type(kTypeButtonHitTarget);
  hit_plane->set_bubble_events(true);
  hit_plane_ = hit_plane.get();
  foreground_->AddChild(std::move(hit_plane));

  EventHandlers event_handlers;
  event_handlers.hover_enter =
      base::Bind(&Button::HandleHoverEnter, base::Unretained(this));
  event_handlers.hover_move =
      base::Bind(&Button::HandleHoverMove, base::Unretained(this));
  event_handlers.hover_leave =
      base::Bind(&Button::HandleHoverLeave, base::Unretained(this));
  event_handlers.button_down =
      base::Bind(&Button::HandleButtonDown, base::Unretained(this));
  event_handlers.button_up =
      base::Bind(&Button::HandleButtonUp, base::Unretained(this));
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
  if (hovered_ && click_handler_)
    click_handler_.Run();
}

void Button::OnStateUpdated() {
  pressed_ = hovered_ ? down_ : false;

  if (hovered_) {
    background_->SetTranslate(0.0, 0.0, hover_offset_);
    foreground_->SetTranslate(0.0, 0.0, hover_offset_);
    hit_plane_->SetScale(kHitPlaneScaleFactorHovered,
                         kHitPlaneScaleFactorHovered, 1.0f);
  } else {
    background_->SetTranslate(0.0, 0.0, 0.0);
    foreground_->SetTranslate(0.0, 0.0, 0.0);
    hit_plane_->SetScale(1.0f, 1.0f, 1.0f);
  }

  background_->SetColor(colors_.GetBackgroundColor(hovered_, pressed_));
  foreground_->SetColor(colors_.GetForegroundColor(disabled_));
}

void Button::OnSetDrawPhase() {
  background_->set_draw_phase(draw_phase());
  foreground_->set_draw_phase(draw_phase());
  hit_plane_->set_draw_phase(draw_phase());
}

void Button::OnSetName() {
  background_->set_owner_name_for_test(name());
  foreground_->set_owner_name_for_test(name());
  hit_plane_->set_owner_name_for_test(name());
}

void Button::NotifyClientSizeAnimated(const gfx::SizeF& size,
                                      int target_property_id,
                                      cc::Animation* animation) {
  if (target_property_id == BOUNDS) {
    background_->SetSize(size.width(), size.height());
    background_->set_corner_radius(size.width() * 0.5f);
    foreground_->SetSize(size.width() * kIconScaleFactor,
                         size.height() * kIconScaleFactor);
    hit_plane_->SetSize(size.width(), size.height());
    hit_plane_->set_corner_radius(size.width() * 0.5f);
  }
  UiElement::NotifyClientSizeAnimated(size, target_property_id, animation);
}

}  // namespace vr
