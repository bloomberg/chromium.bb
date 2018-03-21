// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/disc_button.h"

#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/ui_scene_constants.h"

#include "ui/gfx/geometry/point_f.h"

namespace vr {

namespace {

constexpr float kDefaultIconScaleFactor = 0.5f;

}  // namespace

DiscButton::DiscButton(base::RepeatingCallback<void()> click_handler,
                       const gfx::VectorIcon& icon,
                       AudioDelegate* audio_delegate)
    : Button(click_handler), icon_scale_factor_(kDefaultIconScaleFactor) {
  auto vector_icon = std::make_unique<VectorIcon>(512);
  vector_icon->SetType(kTypeButtonForeground);
  vector_icon->SetIcon(icon);
  vector_icon->set_bubble_events(true);
  vector_icon->SetTransitionedProperties({TRANSFORM});
  foreground_ = vector_icon.get();

  // We now need to reparent the hit target so that it is a child of the
  // newly-created foreground.
  auto target = RemoveChild(hit_plane());
  vector_icon->AddChild(std::move(target));
  AddChild(std::move(vector_icon));

  Sounds sounds;
  sounds.hover_enter = kSoundButtonHover;
  sounds.button_down = kSoundBackButtonClick;
  SetSounds(sounds, audio_delegate);
}

DiscButton::~DiscButton() = default;

void DiscButton::OnStateUpdated() {
  Button::OnStateUpdated();

  if (hovered()) {
    foreground_->SetTranslate(0.0, 0.0, hover_offset());
  } else {
    foreground_->SetTranslate(0.0, 0.0, 0.0);
  }

  foreground_->SetColor(colors().GetForegroundColor(!enabled()));
}

void DiscButton::OnSetDrawPhase() {
  Button::OnSetDrawPhase();
  foreground_->SetDrawPhase(draw_phase());
}

void DiscButton::OnSetName() {
  Button::OnSetName();
  foreground_->set_owner_name_for_test(name());
}

void DiscButton::OnSetCornerRadii(const CornerRadii& radii) {
  Button::OnSetCornerRadii(radii);
  foreground_->SetCornerRadii(radii);
}

void DiscButton::NotifyClientSizeAnimated(const gfx::SizeF& size,
                                          int target_property_id,
                                          cc::KeyframeModel* animation) {
  Button::NotifyClientSizeAnimated(size, target_property_id, animation);
  if (target_property_id == BOUNDS) {
    background()->SetSize(size.width(), size.height());
    background()->set_corner_radius(size.width() * 0.5f);  // Creates a circle.
    foreground()->SetSize(size.width() * icon_scale_factor_,
                          size.height() * icon_scale_factor_);
    hit_plane()->SetSize(size.width(), size.height());
    hit_plane()->set_corner_radius(size.width() * 0.5f);  // Creates a circle.
  }
}

}  // namespace vr
