// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_button.h"

#include <algorithm>

#include "ash/launcher/launcher_button_host.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/image_view.h"

namespace {
const int kBarHeight = 3;
const int kBarSpacing = 5;
const int kIconHeight = 32;
const int kIconWidth = 48;
const int kHopSpacing = 2;
const int kActiveBarColor = 0xe6ffffff;
const int kInactiveBarColor = 0x80ffffff;
const int kHopUpMS = 130;
const int kHopDownMS = 260;

// Used to allow Mouse...() messages to go to the parent view.
class MouseIgnoredView : public views::View {
 public:
  bool HitTest(const gfx::Point& l) const OVERRIDE {
    return false;
  }
};

bool ShouldHop(int state) {
  return state & ash::internal::LauncherButton::STATE_HOVERED ||
      state & ash::internal::LauncherButton::STATE_ACTIVE;
}

} // namespace

namespace ash {

namespace internal {

LauncherButton::IconView::IconView() : icon_size_(kIconHeight) {
}

LauncherButton::IconView::~IconView() {
}

bool LauncherButton::IconView::HitTest(const gfx::Point& l) const {
  return false;
}

LauncherButton* LauncherButton::Create(views::ButtonListener* listener,
                                       LauncherButtonHost* host) {
  LauncherButton* button = new LauncherButton(listener, host);
  button->Init();
  return button;
}

LauncherButton::LauncherButton(views::ButtonListener* listener,
                               LauncherButtonHost* host)
    : CustomButton(listener),
      host_(host),
      icon_view_(NULL),
      bar_(new MouseIgnoredView),
      state_(STATE_NORMAL) {
  set_accessibility_focusable(true);
}

LauncherButton::~LauncherButton() {
  if (!bar_->parent())
    delete bar_;
}

void LauncherButton::SetImage(const SkBitmap& image) {
  if (image.empty()) {
    // TODO: need an empty image.
    icon_view_->SetImage(&image);
    return;
  }

  if (icon_view_->icon_size() == 0) {
    icon_view_->SetImage(&image);
    return;
  }

  // Resize the image maintaining our aspect ratio.
  int pref = icon_view_->icon_size();
  float aspect_ratio =
      static_cast<float>(image.width()) / static_cast<float>(image.height());
  int height = pref;
  int width = static_cast<int>(aspect_ratio * height);
  if (width > pref) {
    width = pref;
    height = static_cast<int>(width / aspect_ratio);
  }
  if (width == image.width() && height == image.height()) {
    icon_view_->SetImage(&image);
    return;
  }
  gfx::Canvas canvas(gfx::Size(width, height), false);
  canvas.DrawBitmapInt(image, 0, 0, image.width(), image.height(),
                       0, 0, width, height, false);
  SkBitmap resized_image(canvas.ExtractBitmap());
  icon_view_->SetImage(&resized_image);
}

void LauncherButton::AddState(State state) {
  if (!(state_ & state)) {
    if (ShouldHop(state) || !ShouldHop(state_)) {
      ui::ScopedLayerAnimationSettings scoped_setter(
          icon_view_->layer()->GetAnimator());
      scoped_setter.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kHopUpMS));
      state_ |= state;
      UpdateState();
    } else {
      state_ |= state;
      UpdateState();
    }
  }
}

void LauncherButton::ClearState(State state) {
  if (state_ & state) {
    if (!ShouldHop(state) || ShouldHop(state_)) {
      ui::ScopedLayerAnimationSettings scoped_setter(
          icon_view_->layer()->GetAnimator());
      scoped_setter.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kHopDownMS));
      state_ &= ~state;
      UpdateState();
    } else {
      state_ &= ~state;
      UpdateState();
    }
  }
}

gfx::Rect LauncherButton::GetIconBounds() const {
  return icon_view_->bounds();
}

bool LauncherButton::OnMousePressed(const views::MouseEvent& event) {
  CustomButton::OnMousePressed(event);
  host_->MousePressedOnButton(this, event);
  return true;
}

void LauncherButton::OnMouseReleased(const views::MouseEvent& event) {
  host_->MouseReleasedOnButton(this, false);
  CustomButton::OnMouseReleased(event);
}

void LauncherButton::OnMouseCaptureLost() {
  ClearState(STATE_HOVERED);
  host_->MouseReleasedOnButton(this, true);
  CustomButton::OnMouseCaptureLost();
}

bool LauncherButton::OnMouseDragged(const views::MouseEvent& event) {
  CustomButton::OnMouseDragged(event);
  host_->MouseDraggedOnButton(this, event);
  return true;
}

void LauncherButton::OnMouseEntered(const views::MouseEvent& event) {
  AddState(STATE_HOVERED);
  CustomButton::OnMouseEntered(event);
  host_->MouseExitedButton(this);
}

void LauncherButton::OnMouseExited(const views::MouseEvent& event) {
  ClearState(STATE_HOVERED);
  CustomButton::OnMouseExited(event);
  host_->MouseExitedButton(this);
}

void LauncherButton::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = host_->GetAccessibleName(this);
}

void LauncherButton::Layout() {
  int image_x = (width() - icon_view_->width()) / 2;
  int image_y = height() - (icon_view_->height() + kBarHeight + kBarSpacing);

  if (ShouldHop(state_))
    image_y -= kHopSpacing;

  icon_view_->SetPosition(gfx::Point(image_x, image_y));
  bar_->SetBounds(0, height() - kBarHeight, width(), kBarHeight);
}

bool LauncherButton::GetTooltipText(
    const gfx::Point& p, string16* tooltip) const {
  DCHECK(tooltip);
  tooltip->assign(host_->GetAccessibleName(this));
  return true;
}

void LauncherButton::Init() {
  icon_view_ = CreateIconView();
  // TODO: refactor the layers so each button doesn't require 2.
  icon_view_->SetPaintToLayer(true);
  icon_view_->SetFillsBoundsOpaquely(false);
  icon_view_->SetSize(gfx::Size(kIconWidth, kIconHeight));
  icon_view_->SetHorizontalAlignment(views::ImageView::CENTER);
  icon_view_->SetVerticalAlignment(views::ImageView::CENTER);
  AddChildView(icon_view_);
}

LauncherButton::IconView* LauncherButton::CreateIconView() {
  return new IconView;
}

void LauncherButton::UpdateState() {
  if (state_ == STATE_NORMAL) {
    if (bar_->parent())
      RemoveChildView(bar_);
  } else {
    if (!bar_->parent())
      AddChildView(bar_);
    if (state_ & STATE_HOVERED || state_ & STATE_ACTIVE) {
      bar_->set_background(views::Background::CreateSolidBackground(
          kActiveBarColor));
    } else if (state_ & STATE_RUNNING) {
      bar_->set_background(views::Background::CreateSolidBackground(
          kInactiveBarColor));
    }
  }

  Layout();
  SchedulePaint();
}
}  // namespace internal
}  // namespace ash
