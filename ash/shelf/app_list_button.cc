// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_button.h"

#include <vector>

#include "ash/ash_constants.h"
#include "ash/shelf/shelf_button_host.h"
#include "ash/shelf/shelf_constants.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/painter.h"

namespace ash {

const int kAnimationDurationInMs = 600;
const float kAnimationOpacity[] = { 1.0f, 0.4f, 1.0f };

AppListButton::AppListButton(views::ButtonListener* listener,
                             ShelfButtonHost* host)
    : views::ImageButton(listener),
      host_(host) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SetImage(views::CustomButton::STATE_NORMAL,
           rb.GetImageNamed(IDR_ASH_SHELF_ICON_APPLIST).ToImageSkia());
  SetImage(views::CustomButton::STATE_HOVERED,
           rb.GetImageNamed(IDR_ASH_SHELF_ICON_APPLIST_HOT).ToImageSkia());
  SetImage(views::CustomButton::STATE_PRESSED,
           rb.GetImageNamed(IDR_ASH_SHELF_ICON_APPLIST_PUSHED).ToImageSkia());
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_AURA_APP_LIST_TITLE));
  SetSize(gfx::Size(kShelfPreferredSize, kShelfPreferredSize));
  SetImageAlignment(ImageButton::ALIGN_CENTER, ImageButton::ALIGN_TOP);
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
                      kFocusBorderColor, gfx::Insets(1, 1, 1, 1)));
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

  opacity_sequence->AddElement(
      ui::LayerAnimationElement::CreatePauseElement(
          ui::LayerAnimationElement::OPACITY,
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
  host_->PointerPressedOnButton(this, ShelfButtonHost::MOUSE, event);
  return true;
}

void AppListButton::OnMouseReleased(const ui::MouseEvent& event) {
  ImageButton::OnMouseReleased(event);
  host_->PointerReleasedOnButton(this, ShelfButtonHost::MOUSE, false);
}

void AppListButton::OnMouseCaptureLost() {
  host_->PointerReleasedOnButton(this, ShelfButtonHost::MOUSE, true);
  ImageButton::OnMouseCaptureLost();
}

bool AppListButton::OnMouseDragged(const ui::MouseEvent& event) {
  ImageButton::OnMouseDragged(event);
  host_->PointerDraggedOnButton(this, ShelfButtonHost::MOUSE, event);
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

void AppListButton::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_BUTTON;
  state->name = host_->GetAccessibleName(this);
}

}  // namespace ash
