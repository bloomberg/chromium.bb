// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/tabbed_launcher_button.h"

#include <algorithm>

#include "ash/launcher/launcher_button_host.h"
#include "ash/launcher/launcher_types.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/multi_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/insets.h"

namespace ash {
namespace internal {

TabbedLauncherButton::IconView::IconView(
    TabbedLauncherButton* host,
    TabbedLauncherButton::IncognitoState is_incognito)
    : host_(host),
      show_image_(true) {
  if (!browser_image_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    browser_image_ = new SkBitmap(
        *rb.GetImageNamed(IDR_AURA_LAUNCHER_BROWSER).ToSkBitmap());
    incognito_browser_image_ = new SkBitmap(
        *rb.GetImageNamed(IDR_AURA_LAUNCHER_INCOGNITO_BROWSER).ToSkBitmap());
    browser_panel_image_ = new SkBitmap(
        *rb.GetImageNamed(IDR_AURA_LAUNCHER_BROWSER_PANEL).ToSkBitmap());
    incognito_browser_panel_image_ = new SkBitmap(
        *rb.GetImageNamed(
            IDR_AURA_LAUNCHER_INCOGNITO_BROWSER_PANEL).ToSkBitmap());
  }
  set_icon_size(0);
  if (is_incognito == STATE_NOT_INCOGNITO)
    LauncherButton::IconView::SetImage(*browser_image_);
  else
    LauncherButton::IconView::SetImage(*incognito_browser_image_);
}

TabbedLauncherButton::IconView::~IconView() {
}

void TabbedLauncherButton::IconView::AnimationEnded(
    const ui::Animation* animation) {
  AnimationProgressed(animation);
  // Hide the image when the animation is done. We'll show it again the next
  // time SetImages is invoked.
  show_image_ = false;
}

void TabbedLauncherButton::IconView::AnimationProgressed(
    const ui::Animation* animation) {
  if (animation_->current_part_index() == 1)
    SchedulePaint();
}

void TabbedLauncherButton::IconView::PrepareForImageChange() {
  if (!show_image_ || (animation_.get() && animation_->is_animating()))
    return;

  // Pause for 500ms, then ease out for 200ms.
  ui::MultiAnimation::Parts animation_parts;
  animation_parts.push_back(ui::MultiAnimation::Part(500, ui::Tween::ZERO));
  animation_parts.push_back(ui::MultiAnimation::Part(200, ui::Tween::EASE_OUT));
  animation_.reset(new ui::MultiAnimation(animation_parts));
  animation_->set_continuous(false);
  animation_->set_delegate(this);
  animation_->Start();
}

void TabbedLauncherButton::IconView::SetTabImage(const SkBitmap& image) {
  animation_.reset();
  show_image_ = true;
  image_ = image;
  SchedulePaint();
}

void TabbedLauncherButton::IconView::OnPaint(gfx::Canvas* canvas) {
  LauncherButton::IconView::OnPaint(canvas);

  if (image_.empty() || !show_image_)
    return;

  bool save_layer = (animation_.get() && animation_->is_animating() &&
                     animation_->current_part_index() == 1);
  if (save_layer)
    canvas->SaveLayerAlpha(animation_->CurrentValueBetween(255, 0));

  int x = (width() - image_.width()) / 2;
  int y = (height() - image_.height()) / 2;
  canvas->DrawBitmapInt(image_, x, y);

  if (save_layer)
    canvas->Restore();
}

// static
SkBitmap* TabbedLauncherButton::IconView::browser_image_ = NULL;
SkBitmap* TabbedLauncherButton::IconView::incognito_browser_image_ = NULL;
SkBitmap* TabbedLauncherButton::IconView::browser_panel_image_ = NULL;
SkBitmap* TabbedLauncherButton::IconView::incognito_browser_panel_image_ = NULL;

TabbedLauncherButton* TabbedLauncherButton::Create(
    views::ButtonListener* listener,
    LauncherButtonHost* host,
    IncognitoState is_incognito) {
  TabbedLauncherButton* button =
      new TabbedLauncherButton(listener, host, is_incognito);
  button->Init();
  return button;
}

TabbedLauncherButton::TabbedLauncherButton(views::ButtonListener* listener,
                                           LauncherButtonHost* host,
                                           IncognitoState is_incognito)
    : LauncherButton(listener, host),
      is_incognito_(is_incognito) {
  set_accessibility_focusable(true);
}

TabbedLauncherButton::~TabbedLauncherButton() {
}

void TabbedLauncherButton::PrepareForImageChange() {
  tabbed_icon_view()->PrepareForImageChange();
}

void TabbedLauncherButton::SetTabImage(const SkBitmap& image) {
  tabbed_icon_view()->SetTabImage(image);
}

void TabbedLauncherButton::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = host()->GetAccessibleName(this);
}

LauncherButton::IconView* TabbedLauncherButton::CreateIconView() {
  return new IconView(this, is_incognito_);
}

}  // namespace internal
}  // namespace ash
