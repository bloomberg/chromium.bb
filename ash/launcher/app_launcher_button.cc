// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/app_launcher_button.h"

#include <algorithm>

#include "ash/launcher/launcher_button_host.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/image_view.h"

namespace {
const int kBarHeight = 4;
const int kBarSpacing = 4;
const int kImageSize = 32;
const int kHopSpacing = 2;
const int kActiveBarColor = 0xe6ffffff;
const int kInactiveBarColor = 0x80ffffff;
const int kHopUpMS = 130;
const int kHopDownMS = 260;

// Used to allow Mouse...() messages to go to the parent view.
class MouseIgnoredImageView : public views::ImageView {
  bool HitTest(const gfx::Point& l) const OVERRIDE {
    return false;
  }
};

class MouseIgnoredView : public views::View {
 public:
  bool HitTest(const gfx::Point& l) const OVERRIDE {
    return false;
  }
};

bool ShouldHop(int state) {
  return state & ash::internal::AppLauncherButton::APP_HOVERED ||
      state & ash::internal::AppLauncherButton::APP_ACTIVE;
}

} // namespace

namespace ash {

namespace internal {

AppLauncherButton::AppLauncherButton(views::ButtonListener* listener,
                                     LauncherButtonHost* host)
    : CustomButton(listener),
      host_(host),
      image_view_(NULL),
      bar_(NULL),
      state_(APP_NORMAL) {
  set_accessibility_focusable(true);
  image_view_ = new MouseIgnoredImageView();
  // TODO: refactor the layers so each button doesn't require 2.
  image_view_->SetPaintToLayer(true);
  image_view_->SetFillsBoundsOpaquely(false);
  image_view_->SetSize(gfx::Size(kImageSize, kImageSize));
  image_view_->SetHorizontalAlignment(views::ImageView::CENTER);
  image_view_->SetVerticalAlignment(views::ImageView::CENTER);
  AddChildView(image_view_);

  bar_ = new MouseIgnoredView();
}

AppLauncherButton::~AppLauncherButton() {
}

void AppLauncherButton::SetAppImage(const SkBitmap& image) {
  if (image.empty()) {
    // TODO: need an empty image.
    image_view_->SetImageSize(gfx::Size());
    image_view_->SetImage(&image);
    return;
  }
  // Resize the image maintaining our aspect ratio.
  int pref = kImageSize;
  float aspect_ratio =
      static_cast<float>(image.width()) / static_cast<float>(image.height());
  int height = pref;
  int width = static_cast<int>(aspect_ratio * height);
  image_view_->SetImageSize(gfx::Size(width, height));
  if (width > pref) {
    width = pref;
    height = static_cast<int>(width / aspect_ratio);
  }
  if (width == image.width() && height == image.height()) {
    image_view_->SetImage(&image);
    return;
  }
  gfx::CanvasSkia canvas(gfx::Size(width, height), false);
  canvas.DrawBitmapInt(image, 0, 0, image.width(), image.height(),
                       0, 0, width, height, false);
  SkBitmap resized_image(canvas.ExtractBitmap());
  image_view_->SetImage(&resized_image);
}

void AppLauncherButton::AddAppState(AppState state) {
  if (!(state_ & state)) {
    if (ShouldHop(state) || !ShouldHop(state_)) {
      ui::ScopedLayerAnimationSettings scoped_setter(
          image_view_->layer()->GetAnimator());
      scoped_setter.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kHopUpMS));
      state_ |= state;
      UpdateAppState();
    } else {
      state_ |= state;
      UpdateAppState();
    }
  }
}

void AppLauncherButton::ClearAppState(AppState state) {
  if (state_ & state) {
    if (!ShouldHop(state) || ShouldHop(state_)) {
      ui::ScopedLayerAnimationSettings scoped_setter(
          image_view_->layer()->GetAnimator());
      scoped_setter.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kHopDownMS));
      state_ &= ~state;
      UpdateAppState();
    } else {
      state_ &= ~state;
      UpdateAppState();
    }
  }
}

bool AppLauncherButton::OnMousePressed(const views::MouseEvent& event) {
  CustomButton::OnMousePressed(event);
  host_->MousePressedOnButton(this, event);
  return true;
}

void AppLauncherButton::OnMouseReleased(const views::MouseEvent& event) {
  host_->MouseReleasedOnButton(this, false);
  CustomButton::OnMouseReleased(event);
}

void AppLauncherButton::OnMouseCaptureLost() {
  ClearAppState(APP_HOVERED);
  host_->MouseReleasedOnButton(this, true);
  CustomButton::OnMouseCaptureLost();
}

bool AppLauncherButton::OnMouseDragged(const views::MouseEvent& event) {
  CustomButton::OnMouseDragged(event);
  host_->MouseDraggedOnButton(this, event);
  return true;
}

void AppLauncherButton::OnMouseEntered(const views::MouseEvent& event) {
  AddAppState(APP_HOVERED);
  CustomButton::OnMouseEntered(event);
  host_->MouseExitedButton(this);
}

void AppLauncherButton::OnMouseExited(const views::MouseEvent& event) {
  ClearAppState(APP_HOVERED);
  CustomButton::OnMouseExited(event);
  host_->MouseExitedButton(this);
}

void AppLauncherButton::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = host_->GetAccessibleName(this);
}

void AppLauncherButton::Layout() {
  int image_x = (width() - image_view_->width()) / 2;
  int image_y = height() - (kImageSize + kBarHeight + kBarSpacing);

  if (ShouldHop(state_))
    image_y -= kHopSpacing;

  image_view_->SetPosition(gfx::Point(image_x, image_y));
  bar_->SetBounds(0, height() - kBarHeight, width(), kBarHeight);
}

void AppLauncherButton::UpdateAppState() {
  if (state_ == APP_NORMAL) {
    if (bar_->parent())
      RemoveChildView(bar_);
  } else {
    if (!bar_->parent())
      AddChildView(bar_);
    if (state_ & APP_HOVERED || state_ & APP_ACTIVE) {
      bar_->set_background(views::Background::CreateSolidBackground(
          kActiveBarColor));
    } else if (state_ & APP_RUNNING) {
      bar_->set_background(views::Background::CreateSolidBackground(
          kInactiveBarColor));
    }
  }

  Layout();
  SchedulePaint();
}
}  // namespace internal
}  // namespace ash
