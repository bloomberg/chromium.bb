// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_button.h"

#include <algorithm>

#include "ash/launcher/launcher_button_host.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/controls/image_view.h"

namespace {
const int kBarHeight = 3;
const int kBarSpacing = 5;
const int kIconHeight = 32;
const int kIconWidth = 48;
const int kHopSpacing = 2;
const int kActiveBarColor = 0xe6ffffff;
const int kInactiveBarColor = 0x80ffffff;
const int kHopUpMS = 200;
const int kHopDownMS = 200;
const int kAttentionThrobDurationMS = 2000;

bool ShouldHop(int state) {
  return state & ash::internal::LauncherButton::STATE_HOVERED ||
      state & ash::internal::LauncherButton::STATE_ACTIVE;
}

} // namespace

namespace ash {

namespace internal {

class LauncherButton::BarView : public views::ImageView,
                                public ui::AnimationDelegate {
 public:
  BarView() : ALLOW_THIS_IN_INITIALIZER_LIST(animation_(this)) {
    animation_.SetThrobDuration(kAttentionThrobDurationMS);
    animation_.SetTweenType(ui::Tween::SMOOTH_IN_OUT);
  }

  // View overrides.
  bool HitTest(const gfx::Point& l) const OVERRIDE {
    // Allow Mouse...() messages to go to the parent view.
    return false;
  }

  void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    if (animation_.is_animating()) {
      int alpha = animation_.CurrentValueBetween(0, 255);
      canvas->SaveLayerAlpha(alpha);
      views::ImageView::OnPaint(canvas);
      canvas->Restore();
    } else {
      views::ImageView::OnPaint(canvas);
    }
  }

  // ui::AnimationDelegate overrides.
  void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    SchedulePaint();
  }

  void ShowAttention(bool show) {
    if (show)
      animation_.StartThrobbing(-1);
    else
      animation_.Reset();
  }

 private:
  ui::ThrobAnimation animation_;

  DISALLOW_COPY_AND_ASSIGN(BarView);
};

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
      bar_(new BarView),
      state_(STATE_NORMAL) {
  set_accessibility_focusable(true);
  bar_->SetHorizontalAlignment(views::ImageView::CENTER);
  bar_->SetVerticalAlignment(views::ImageView::TRAILING);
  AddChildView(bar_);
}

LauncherButton::~LauncherButton() {
}

void LauncherButton::SetShadowedImage(const SkBitmap& bitmap) {
  const SkColor kShadowColor[] = {
    SkColorSetARGB(0x1A, 0, 0, 0),
    SkColorSetARGB(0x1A, 0, 0, 0),
    SkColorSetARGB(0x54, 0, 0, 0),
  };
  const gfx::Point kShadowOffset[] = {
    gfx::Point(0, 2),
    gfx::Point(0, 3),
    gfx::Point(0, 0),
  };
  const SkScalar kShadowRadius[] = {
    SkIntToScalar(0),
    SkIntToScalar(1),
    SkIntToScalar(1),
  };

  SkBitmap shadowed_bitmap = SkBitmapOperations::CreateDropShadow(
      bitmap,
      arraysize(kShadowColor) - 1,
      kShadowColor,
      kShadowOffset,
      kShadowRadius);
  icon_view_->SetImage(shadowed_bitmap);
}

void LauncherButton::SetImage(const SkBitmap& image) {
  if (image.empty()) {
    // TODO: need an empty image.
    icon_view_->SetImage(&image);
    return;
  }

  if (icon_view_->icon_size() == 0) {
    SetShadowedImage(image);
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
    SetShadowedImage(image);
    return;
  }

  SkBitmap resized_image = SkBitmapOperations::CreateResizedBitmap(
      image, gfx::Size(width, height));
  SetShadowedImage(resized_image);
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
    if (state & STATE_ATTENTION)
      bar_->ShowAttention(true);
  }
}

void LauncherButton::ClearState(State state) {
  if (state_ & state) {
    if (!ShouldHop(state) || ShouldHop(state_)) {
      ui::ScopedLayerAnimationSettings scoped_setter(
          icon_view_->layer()->GetAnimator());
      scoped_setter.SetTweenType(ui::Tween::LINEAR);
      scoped_setter.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kHopDownMS));
      state_ &= ~state;
      UpdateState();
    } else {
      state_ &= ~state;
      UpdateState();
    }
    if (state & STATE_ATTENTION)
      bar_->ShowAttention(false);
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
  CustomButton::OnMouseReleased(event);
  host_->MouseReleasedOnButton(this, false);
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
  bar_->SetBounds(0, 0, width(), height());
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
    bar_->SetVisible(false);
  } else {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    int bar_id;
    bar_->SetVisible(true);

    if (state_ & STATE_ACTIVE || state_ & STATE_ATTENTION)
      bar_id = IDR_AURA_LAUNCHER_UNDERLINE_ACTIVE;
    else if (state_ & STATE_HOVERED)
      bar_id = IDR_AURA_LAUNCHER_UNDERLINE_HOVER;
    else
      bar_id = IDR_AURA_LAUNCHER_UNDERLINE_RUNNING;

    bar_->SetImage(rb.GetImageNamed(bar_id).ToSkBitmap());
  }

  Layout();
  SchedulePaint();
}
}  // namespace internal
}  // namespace ash
