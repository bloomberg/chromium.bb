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

constexpr float kIconScaleFactor = 0.5;
constexpr float kHitPlaneScaleFactorHovered = 1.2;
constexpr float kBackgroundZOffset = -kTextureOffset;
constexpr float kForegroundZOffset = kTextureOffset;
constexpr float kBackgroundZOffsetHover = kTextureOffset;
constexpr float kForegroundZOffsetHover = 4 * kTextureOffset;

}  // namespace

Button::Button(base::Callback<void()> click_handler,
               int draw_phase,
               float width,
               float height,
               const gfx::VectorIcon& icon)
    : click_handler_(click_handler) {
  set_draw_phase(draw_phase);
  set_hit_testable(false);
  SetSize(width, height);

  auto background = base::MakeUnique<Rect>();
  background->set_name(kNone);
  background->set_draw_phase(draw_phase);
  background->set_bubble_events(true);
  background->SetSize(width, height);
  background->SetTransitionedProperties({TRANSFORM});
  background->set_corner_radius(width / 2);
  background->set_hit_testable(false);
  background->SetTranslate(0.0, 0.0, kBackgroundZOffset);
  background_ = background.get();
  AddChild(std::move(background));

  auto vector_icon = base::MakeUnique<VectorIcon>(512);
  vector_icon->set_name(kNone);
  vector_icon->SetIcon(icon);
  vector_icon->set_draw_phase(draw_phase);
  vector_icon->set_bubble_events(true);
  vector_icon->SetSize(width * kIconScaleFactor, height * kIconScaleFactor);
  vector_icon->SetTransitionedProperties({TRANSFORM});
  vector_icon->set_hit_testable(false);
  vector_icon->SetTranslate(0.0, 0.0, kForegroundZOffset);
  foreground_ = vector_icon.get();
  AddChild(std::move(vector_icon));

  auto hit_plane = base::MakeUnique<InvisibleHitTarget>();
  hit_plane->set_name(kNone);
  hit_plane->set_draw_phase(draw_phase);
  hit_plane->set_bubble_events(true);
  hit_plane->SetSize(width, height);
  hit_plane->set_corner_radius(width / 2);
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
    background_->SetTranslate(0.0, 0.0, kBackgroundZOffsetHover);
    foreground_->SetTranslate(0.0, 0.0, kForegroundZOffsetHover);
    hit_plane_->SetScale(kHitPlaneScaleFactorHovered,
                         kHitPlaneScaleFactorHovered, 1.0f);
  } else {
    background_->SetTranslate(0.0, 0.0, kBackgroundZOffset);
    foreground_->SetTranslate(0.0, 0.0, kForegroundZOffset);
    hit_plane_->SetScale(1.0f, 1.0f, 1.0f);
  }

  if (pressed_) {
    background_->SetColor(colors_.background_press);
  } else if (hovered_) {
    background_->SetColor(colors_.background_hover);
  } else {
    background_->SetColor(colors_.background);
  }
  foreground_->SetColor(colors_.foreground);
}

}  // namespace vr
