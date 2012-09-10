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
    TabbedLauncherButton* host)
    : host_(host) {
  if (!browser_image_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    browser_image_ = rb.GetImageNamed(IDR_AURA_LAUNCHER_BROWSER).ToImageSkia();
    incognito_browser_image_ =
        rb.GetImageNamed(IDR_AURA_LAUNCHER_INCOGNITO_BROWSER).ToImageSkia();
    browser_panel_image_ =
        rb.GetImageNamed(IDR_AURA_LAUNCHER_BROWSER_PANEL).ToImageSkia();
    incognito_browser_panel_image_ =
        rb.GetImageNamed(
            IDR_AURA_LAUNCHER_INCOGNITO_BROWSER_PANEL).ToImageSkia();
  }
  set_icon_size(0);
  if (host->is_incognito() == STATE_NOT_INCOGNITO)
    LauncherButton::IconView::SetImage(*browser_image_);
  else
    LauncherButton::IconView::SetImage(*incognito_browser_image_);
}

TabbedLauncherButton::IconView::~IconView() {
}

void TabbedLauncherButton::IconView::AnimationEnded(
    const ui::Animation* animation) {
  AnimationProgressed(animation);
  animating_image_ = SkBitmap();
}

void TabbedLauncherButton::IconView::AnimationProgressed(
    const ui::Animation* animation) {
  if (animation_->current_part_index() == 1)
    SchedulePaint();
}

void TabbedLauncherButton::IconView::SetTabImage(const gfx::ImageSkia& image) {
  if (image.isNull()) {
    if (!image_.isNull()) {
      // Pause for 500ms, then ease out for 200ms.
      ui::MultiAnimation::Parts animation_parts;
      animation_parts.push_back(ui::MultiAnimation::Part(500, ui::Tween::ZERO));
      animation_parts.push_back(
          ui::MultiAnimation::Part(200, ui::Tween::EASE_OUT));
      animation_.reset(new ui::MultiAnimation(animation_parts));
      animation_->set_continuous(false);
      animation_->set_delegate(this);
      animation_->Start();
      animating_image_ = image_;
      image_ = image;
    }
  } else {
    animation_.reset();
    SchedulePaint();
    image_ = image;
  }
}

void TabbedLauncherButton::IconView::OnPaint(gfx::Canvas* canvas) {
  LauncherButton::IconView::OnPaint(canvas);

  // Only non incognito icons show the tab image.
  if (host_->is_incognito() != STATE_NOT_INCOGNITO)
    return;

  if ((animation_.get() && animation_->is_animating() &&
      animation_->current_part_index() == 1)) {
    int x = (width() - animating_image_.width()) / 2;
    int y = (height() - animating_image_.height()) / 2;
    canvas->SaveLayerAlpha(animation_->CurrentValueBetween(255, 0));
    canvas->DrawImageInt(animating_image_, x, y);
    canvas->Restore();
  } else {
    int x = (width() - image_.width()) / 2;
    int y = (height() - image_.height()) / 2;
    canvas->DrawImageInt(image_, x, y);
  }
}

// static
const gfx::ImageSkia* TabbedLauncherButton::IconView::browser_image_ = NULL;
const gfx::ImageSkia* TabbedLauncherButton::IconView::incognito_browser_image_ =
    NULL;
const gfx::ImageSkia* TabbedLauncherButton::IconView::browser_panel_image_ =
    NULL;
const gfx::ImageSkia*
    TabbedLauncherButton::IconView::incognito_browser_panel_image_ = NULL;

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

void TabbedLauncherButton::SetTabImage(const gfx::ImageSkia& image) {
  tabbed_icon_view()->SetTabImage(image);
}

void TabbedLauncherButton::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = host()->GetAccessibleName(this);
}

LauncherButton::IconView* TabbedLauncherButton::CreateIconView() {
  return new IconView(this);
}

}  // namespace internal
}  // namespace ash
