// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_button.h"

#include <algorithm>

#include "ash/launcher/launcher_button_host.h"
#include "grit/ui_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"

namespace {

// Size of the bar. This is along the opposite axis of the shelf. For example,
// if the shelf is aligned horizontally then this is the height of the bar.
const int kBarSize = 3;
const int kBarSpacing = 5;
const int kIconSize = 32;
const int kHopSpacing = 2;
const int kHopUpMS = 0;
const int kHopDownMS = 200;
const int kAttentionThrobDurationMS = 1000;

bool ShouldHop(int state) {
  return state & ash::internal::LauncherButton::STATE_HOVERED ||
      state & ash::internal::LauncherButton::STATE_ACTIVE ||
      state & ash::internal::LauncherButton::STATE_FOCUSED;
}

}  // namespace

namespace ash {

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// LauncherButton::BarView

class LauncherButton::BarView : public views::ImageView,
                                public ui::AnimationDelegate {
 public:
  BarView() : ALLOW_THIS_IN_INITIALIZER_LIST(animation_(this)) {
    animation_.SetThrobDuration(kAttentionThrobDurationMS);
    animation_.SetTweenType(ui::Tween::SMOOTH_IN_OUT);
  }

  // View overrides.
  bool HitTestRect(const gfx::Rect& rect) const OVERRIDE {
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
    if (show) {
      // It's less disruptive if we don't start the pulsing at 0.
      animation_.Reset(0.375);
      animation_.StartThrobbing(-1);
    } else {
      animation_.Reset(0.0);
    }
  }

 private:
  ui::ThrobAnimation animation_;

  DISALLOW_COPY_AND_ASSIGN(BarView);
};

////////////////////////////////////////////////////////////////////////////////
// LauncherButton::IconView

LauncherButton::IconView::IconView() : icon_size_(kIconSize) {
}

LauncherButton::IconView::~IconView() {
}

bool LauncherButton::IconView::HitTestRect(const gfx::Rect& rect) const {
  // Return false so that LauncherButton gets all the mouse events.
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// LauncherButton

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

  const gfx::ShadowValue kShadows[] = {
    gfx::ShadowValue(gfx::Point(0, 2), 0, SkColorSetARGB(0x1A, 0, 0, 0)),
    gfx::ShadowValue(gfx::Point(0, 3), 1, SkColorSetARGB(0x1A, 0, 0, 0)),
    gfx::ShadowValue(gfx::Point(0, 0), 1, SkColorSetARGB(0x54, 0, 0, 0)),
  };
  icon_shadows_.assign(kShadows, kShadows + arraysize(kShadows));

  AddChildView(bar_);
}

LauncherButton::~LauncherButton() {
}

void LauncherButton::SetShadowedImage(const gfx::ImageSkia& image) {
  icon_view_->SetImage(gfx::ImageSkiaOperations::CreateImageWithDropShadow(
      image, icon_shadows_));
}

void LauncherButton::SetImage(const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // TODO: need an empty image.
    icon_view_->SetImage(image);
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

  SetShadowedImage(gfx::ImageSkiaOperations::CreateResizedImage(image,
      skia::ImageOperations::RESIZE_BEST, gfx::Size(width, height)));
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

bool LauncherButton::OnMousePressed(const ui::MouseEvent& event) {
  CustomButton::OnMousePressed(event);
  host_->PointerPressedOnButton(this, LauncherButtonHost::MOUSE, event);
  return true;
}

void LauncherButton::OnMouseReleased(const ui::MouseEvent& event) {
  CustomButton::OnMouseReleased(event);
  host_->PointerReleasedOnButton(this, LauncherButtonHost::MOUSE, false);
}

void LauncherButton::OnMouseCaptureLost() {
  ClearState(STATE_HOVERED);
  host_->PointerReleasedOnButton(this, LauncherButtonHost::MOUSE, true);
  CustomButton::OnMouseCaptureLost();
}

bool LauncherButton::OnMouseDragged(const ui::MouseEvent& event) {
  CustomButton::OnMouseDragged(event);
  host_->PointerDraggedOnButton(this, LauncherButtonHost::MOUSE, event);
  return true;
}

void LauncherButton::OnMouseMoved(const ui::MouseEvent& event) {
  CustomButton::OnMouseMoved(event);
  host_->MouseMovedOverButton(this);
}

void LauncherButton::OnMouseEntered(const ui::MouseEvent& event) {
  AddState(STATE_HOVERED);
  CustomButton::OnMouseEntered(event);
  host_->MouseEnteredButton(this);
}

void LauncherButton::OnMouseExited(const ui::MouseEvent& event) {
  ClearState(STATE_HOVERED);
  CustomButton::OnMouseExited(event);
  host_->MouseExitedButton(this);
}

ui::EventResult LauncherButton::OnGestureEvent(
    const ui::GestureEvent& event) {
  switch (event.type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      AddState(STATE_HOVERED);
      return CustomButton::OnGestureEvent(event);
    case ui::ET_GESTURE_END:
      ClearState(STATE_HOVERED);
      return CustomButton::OnGestureEvent(event);
    case ui::ET_GESTURE_SCROLL_BEGIN:
      host_->PointerPressedOnButton(this, LauncherButtonHost::TOUCH, event);
      return ui::ER_CONSUMED;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      host_->PointerDraggedOnButton(this, LauncherButtonHost::TOUCH, event);
      return ui::ER_CONSUMED;
    case ui::ET_GESTURE_SCROLL_END:
      host_->PointerReleasedOnButton(this, LauncherButtonHost::TOUCH, false);
      return ui::ER_CONSUMED;
    default:
      return CustomButton::OnGestureEvent(event);
  }
}

void LauncherButton::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = host_->GetAccessibleName(this);
}

void LauncherButton::Layout() {
  gfx::Rect rect(GetContentsBounds());
  const gfx::Size& icon_size(icon_view_->GetPreferredSize());
  gfx::Rect icon_bounds = rect.Center(icon_size);

  int x_offset = 0, y_offset = 0;
  if (IsShelfHorizontal()) {
    y_offset += kBarSize / 2;
    if (ShouldHop(state_))
      y_offset += kHopSpacing;
  } else {
    x_offset += kBarSize / 2;
    if (ShouldHop(state_))
      x_offset += kHopSpacing;
    if (host_->GetShelfAlignment() == SHELF_ALIGNMENT_LEFT)
      x_offset = -x_offset;
  }

  // Offset to compensate for shadows.
  gfx::Insets icon_shadow_padding = gfx::ShadowValue::GetMargin(icon_shadows_);

  x_offset -= (icon_shadow_padding.left() - icon_shadow_padding.right()) / 2;
  y_offset -= (icon_shadow_padding.top() - icon_shadow_padding.bottom()) / 2;
  icon_bounds.Offset(-x_offset, -y_offset);

  icon_view_->SetBoundsRect(icon_bounds);
  bar_->SetBoundsRect(rect);
}

void LauncherButton::ChildPreferredSizeChanged(views::View* child) {
  Layout();
}

void LauncherButton::OnFocus() {
  AddState(STATE_FOCUSED);
  CustomButton::OnFocus();
}

void LauncherButton::OnBlur() {
  ClearState(STATE_FOCUSED);
  CustomButton::OnBlur();
}

void LauncherButton::Init() {
  icon_view_ = CreateIconView();

  // TODO: refactor the layers so each button doesn't require 2.
  icon_view_->SetPaintToLayer(true);
  icon_view_->SetFillsBoundsOpaquely(false);
  icon_view_->SetHorizontalAlignment(views::ImageView::CENTER);
  icon_view_->SetVerticalAlignment(views::ImageView::CENTER);

  AddChildView(icon_view_);
}

LauncherButton::IconView* LauncherButton::CreateIconView() {
  return new IconView;
}

bool LauncherButton::IsShelfHorizontal() const {
  return host_->GetShelfAlignment() == SHELF_ALIGNMENT_BOTTOM;
}

void LauncherButton::UpdateState() {
  if (state_ == STATE_NORMAL) {
    bar_->SetVisible(false);
  } else {
    int bar_id;
    if (IsShelfHorizontal()) {
      if (state_ & (STATE_HOVERED | STATE_FOCUSED | STATE_ATTENTION))
        bar_id = IDR_AURA_LAUNCHER_UNDERLINE_HOVER;
      else if (state_ & STATE_ACTIVE)
        bar_id = IDR_AURA_LAUNCHER_UNDERLINE_ACTIVE;
      else
        bar_id = IDR_AURA_LAUNCHER_UNDERLINE_RUNNING;
    } else {
      if (state_ & (STATE_HOVERED | STATE_FOCUSED | STATE_ATTENTION))
        bar_id = IDR_AURA_LAUNCHER_UNDERLINE_VERTICAL_HOVER;
      else if (state_ & STATE_ACTIVE)
        bar_id = IDR_AURA_LAUNCHER_UNDERLINE_VERTICAL_ACTIVE;
      else
        bar_id = IDR_AURA_LAUNCHER_UNDERLINE_VERTICAL_RUNNING;
    }

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    bar_->SetImage(rb.GetImageNamed(bar_id).ToImageSkia());
    bar_->SetVisible(true);
  }

  switch (host_->GetShelfAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
      bar_->SetHorizontalAlignment(views::ImageView::CENTER);
      bar_->SetVerticalAlignment(views::ImageView::TRAILING);
      break;
    case SHELF_ALIGNMENT_LEFT:
      bar_->SetHorizontalAlignment(
          base::i18n::IsRTL() ? views::ImageView::TRAILING :
                                views::ImageView::LEADING);
      bar_->SetVerticalAlignment(views::ImageView::CENTER);
      break;
    case SHELF_ALIGNMENT_RIGHT:
      bar_->SetHorizontalAlignment(
          base::i18n::IsRTL() ? views::ImageView::LEADING :
                                views::ImageView::TRAILING);
      bar_->SetVerticalAlignment(views::ImageView::CENTER);
      break;
  }

  Layout();
  SchedulePaint();
}

}  // namespace internal
}  // namespace ash
