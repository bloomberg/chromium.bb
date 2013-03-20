// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/app_list_button.h"

#include <vector>

#include "ash/launcher/launcher_button_host.h"
#include "ash/launcher/launcher_types.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {
namespace internal {

namespace {

const int kAnimationDurationInMs = 600;
const float kAnimationOpacity[] = { 1.0f, 0.4f, 1.0f };
const int kBorderSize = 9;
}  // namespace

AppListButton::AppListButton(views::ButtonListener* listener,
                             LauncherButtonHost* host)
    : views::ImageButton(listener),
      host_(host) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetImage(
      views::CustomButton::STATE_NORMAL,
      rb.GetImageNamed(IDR_AURA_LAUNCHER_ICON_APPLIST).ToImageSkia());
  SetImage(
      views::CustomButton::STATE_HOVERED,
      rb.GetImageNamed(IDR_AURA_LAUNCHER_ICON_APPLIST_HOT).
          ToImageSkia());
  SetImage(
      views::CustomButton::STATE_PRESSED,
      rb.GetImageNamed(IDR_AURA_LAUNCHER_ICON_APPLIST_PUSHED).
          ToImageSkia());
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_AURA_APP_LIST_TITLE));
  SetSize(gfx::Size(kLauncherPreferredSize, kLauncherPreferredSize));
  SetImageAlignment(ImageButton::ALIGN_CENTER, ImageButton::ALIGN_TOP);
  set_border(views::Border::CreateEmptyBorder(kBorderSize,
                                              kBorderSize,
                                              kBorderSize,
                                              kBorderSize));
}

AppListButton::~AppListButton() {
}

void AppListButton::StartLoadingAnimation() {
  layer()->GetAnimator()->StopAnimating();

  scoped_ptr<ui::LayerAnimationSequence> opacity_sequence(
      new ui::LayerAnimationSequence());

  opacity_sequence->set_is_cyclic(true);

  for (size_t i = 0; i < arraysize(kAnimationOpacity); ++i) {
    opacity_sequence->AddElement(
        ui::LayerAnimationElement::CreateOpacityElement(
            kAnimationOpacity[i],
            base::TimeDelta::FromMilliseconds(kAnimationDurationInMs)));
  }

  ui::LayerAnimationElement::AnimatableProperties opacity_properties;
  opacity_properties.insert(ui::LayerAnimationElement::OPACITY);
  opacity_sequence->AddElement(
      ui::LayerAnimationElement::CreatePauseElement(
          opacity_properties,
          base::TimeDelta::FromMilliseconds(kAnimationDurationInMs)));

  // LayerAnimator takes ownership of the sequences.
  layer()->GetAnimator()->ScheduleAnimation(opacity_sequence.release());
}

void AppListButton::StopLoadingAnimation() {
  layer()->GetAnimator()->StopAnimating();

  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationInMs));
  layer()->SetOpacity(1.0f);
  layer()->SetTransform(gfx::Transform());
}

bool AppListButton::OnMousePressed(const ui::MouseEvent& event) {
  ImageButton::OnMousePressed(event);
  host_->PointerPressedOnButton(this, LauncherButtonHost::MOUSE, event);
  return true;
}

void AppListButton::OnMouseReleased(const ui::MouseEvent& event) {
  ImageButton::OnMouseReleased(event);
  host_->PointerReleasedOnButton(this, LauncherButtonHost::MOUSE, false);
}

void AppListButton::OnMouseCaptureLost() {
  host_->PointerReleasedOnButton(this, LauncherButtonHost::MOUSE, true);
  ImageButton::OnMouseCaptureLost();
}

bool AppListButton::OnMouseDragged(const ui::MouseEvent& event) {
  ImageButton::OnMouseDragged(event);
  host_->PointerDraggedOnButton(this, LauncherButtonHost::MOUSE, event);
  return true;
}

void AppListButton::OnMouseMoved(const ui::MouseEvent& event) {
  ImageButton::OnMouseMoved(event);
  host_->MouseMovedOverButton(this);
}

void AppListButton::OnMouseEntered(const ui::MouseEvent& event) {
  ImageButton::OnMouseEntered(event);
  host_->MouseEnteredButton(this);
}

void AppListButton::OnMouseExited(const ui::MouseEvent& event) {
  ImageButton::OnMouseExited(event);
  host_->MouseExitedButton(this);
}

void AppListButton::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = host_->GetAccessibleName(this);
}

}  // namespace internal
}  // namespace ash
