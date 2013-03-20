// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_button.h"

#include <algorithm>

#include "ash/launcher/launcher_button_host.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "grit/ash_resources.h"
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
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/controls/image_view.h"

namespace {

// Size of the bar. This is along the opposite axis of the shelf. For example,
// if the shelf is aligned horizontally then this is the height of the bar.
const int kBarSize = 3;
const int kBarSpacing = 5;
const int kIconSize = 32;
const int kHopSpacing = 2;
const int kIconPad = 8;
const int kHopUpMS = 0;
const int kHopDownMS = 200;
const int kAttentionThrobDurationMS = 800;

bool ShouldHop(int state) {
  return state & ash::internal::LauncherButton::STATE_HOVERED ||
      state & ash::internal::LauncherButton::STATE_ACTIVE ||
      state & ash::internal::LauncherButton::STATE_FOCUSED;
}

// Simple AnimationDelegate that owns a single ThrobAnimation instance to
// keep all Draw Attention animations in sync.
class LauncherButtonAnimation : public ui::AnimationDelegate {
 public:
  class Observer {
   public:
    virtual void AnimationProgressed() = 0;

   protected:
    virtual ~Observer() {}
  };

  static LauncherButtonAnimation* GetInstance() {
    static LauncherButtonAnimation* s_instance = new LauncherButtonAnimation();
    return s_instance;
  }

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
    if (observers_.size() == 0)
      animation_.Stop();
  }

  int GetAlpha() {
    return GetThrobAnimation().CurrentValueBetween(0, 255);
  }

  double GetAnimation() {
    return GetThrobAnimation().GetCurrentValue();
  }

 private:
  LauncherButtonAnimation()
    : ALLOW_THIS_IN_INITIALIZER_LIST(animation_(this)) {
    animation_.SetThrobDuration(kAttentionThrobDurationMS);
    animation_.SetTweenType(ui::Tween::SMOOTH_IN_OUT);
  }

  virtual ~LauncherButtonAnimation() {
  }

  ui::ThrobAnimation& GetThrobAnimation() {
    if (!animation_.is_animating()) {
      animation_.Reset();
      animation_.StartThrobbing(-1 /*throb indefinitely*/);
    }
    return animation_;
  }

  // ui::AnimationDelegate
  void AnimationProgressed(const ui::Animation* animation) {
    if (animation != &animation_)
      return;
    if (!animation_.is_animating())
      return;
    FOR_EACH_OBSERVER(Observer, observers_, AnimationProgressed());
  }

  ui::ThrobAnimation animation_;
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(LauncherButtonAnimation);
};

}  // namespace

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// LauncherButton::BarView

class LauncherButton::BarView : public views::ImageView,
                                public LauncherButtonAnimation::Observer {
 public:
  BarView(LauncherButton* host)
      : host_(host),
        show_attention_(false) {
  }

  ~BarView() {
    if (show_attention_)
      LauncherButtonAnimation::GetInstance()->RemoveObserver(this);
  }

  // View
  virtual bool HitTestRect(const gfx::Rect& rect) const OVERRIDE {
    // Allow Mouse...() messages to go to the parent view.
    return false;
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    if (show_attention_) {
      int alpha = LauncherButtonAnimation::GetInstance()->GetAlpha();
      canvas->SaveLayerAlpha(alpha);
      views::ImageView::OnPaint(canvas);
      canvas->Restore();
    } else {
      views::ImageView::OnPaint(canvas);
    }
  }

  // LauncherButtonAnimation::Observer
  virtual void AnimationProgressed() OVERRIDE {
    UpdateBounds();
    SchedulePaint();
  }

  void SetBarBoundsRect(const gfx::Rect& bounds) {
    base_bounds_ = bounds;
    UpdateBounds();
  }

  void ShowAttention(bool show) {
    if (show_attention_ != show) {
      show_attention_ = show;
      if (show_attention_)
        LauncherButtonAnimation::GetInstance()->AddObserver(this);
      else
        LauncherButtonAnimation::GetInstance()->RemoveObserver(this);
    }
    UpdateBounds();
  }

 private:
  void UpdateBounds() {
    gfx::Rect bounds = base_bounds_;
    if (show_attention_) {
      // Scale from .35 to 1.0 of the total width (which is wider than the
      // visible width of the image, so the animation "rests" briefly at full
      // visible width.
      double animation = LauncherButtonAnimation::GetInstance()->GetAnimation();
      double scale = (.35 + .65 * animation);
      if (host_->shelf_layout_manager()->GetAlignment() ==
          SHELF_ALIGNMENT_BOTTOM) {
        bounds.set_width(base_bounds_.width() * scale);
        int x_offset = (base_bounds_.width() - bounds.width()) / 2;
        bounds.set_x(base_bounds_.x() + x_offset);
      } else {
        bounds.set_height(base_bounds_.height() * scale);
        int y_offset = (base_bounds_.height() - bounds.height()) / 2;
        bounds.set_y(base_bounds_.y() + y_offset);
      }
    }
    SetBoundsRect(bounds);
  }

  LauncherButton* host_;
  bool show_attention_;
  gfx::Rect base_bounds_;

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

LauncherButton* LauncherButton::Create(
    views::ButtonListener* listener,
    LauncherButtonHost* host,
    ShelfLayoutManager* shelf_layout_manager) {
  LauncherButton* button =
      new LauncherButton(listener, host, shelf_layout_manager);
  button->Init();
  return button;
}

LauncherButton::LauncherButton(views::ButtonListener* listener,
                               LauncherButtonHost* host,
                               ShelfLayoutManager* shelf_layout_manager)
    : CustomButton(listener),
      host_(host),
      icon_view_(NULL),
      bar_(new BarView(this)),
      state_(STATE_NORMAL),
      shelf_layout_manager_(shelf_layout_manager),
      destroyed_flag_(NULL) {
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
  if (destroyed_flag_)
    *destroyed_flag_ = true;
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
    }
    state_ |= state;
    UpdateState();
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
    }
    state_ &= ~state;
    UpdateState();
    if (state & STATE_ATTENTION)
      bar_->ShowAttention(false);
  }
}

gfx::Rect LauncherButton::GetIconBounds() const {
  return icon_view_->bounds();
}

void LauncherButton::ShowContextMenu(const gfx::Point& p,
                                     bool is_mouse_gesture) {
  if (!context_menu_controller())
    return;

  bool destroyed = false;
  destroyed_flag_ = &destroyed;

  CustomButton::ShowContextMenu(p, is_mouse_gesture);

  if (!destroyed) {
    destroyed_flag_ = NULL;
    // The menu will not propagate mouse events while its shown. To address,
    // the hover state gets cleared once the menu was shown (and this was not
    // destroyed).
    ClearState(STATE_HOVERED);
  }
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

void LauncherButton::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = host_->GetAccessibleName(this);
}

void LauncherButton::Layout() {
  const gfx::Rect button_bounds(GetContentsBounds());

  int x_offset = shelf_layout_manager_->SelectValueForShelfAlignment(
      0, kIconPad, kIconPad, 0);
  int y_offset = shelf_layout_manager_->SelectValueForShelfAlignment(
      kIconPad, 0, 0, kIconPad);

  int icon_width = std::min(kIconSize, button_bounds.width() - x_offset);
  int icon_height = std::min(kIconSize, button_bounds.height() - y_offset);

  x_offset = std::max(x_offset, (button_bounds.width() - icon_width) / 2);
  y_offset = std::max(y_offset, (button_bounds.height() - icon_height) / 2);

  if (ShouldHop(state_)) {
    x_offset += shelf_layout_manager_->SelectValueForShelfAlignment(
        0, kHopSpacing, -kHopSpacing, 0);
    y_offset += shelf_layout_manager_->SelectValueForShelfAlignment(
        -kHopSpacing, 0, 0, kHopSpacing);
  }

  gfx::Rect icon_bounds(
        button_bounds.x() + x_offset,
        button_bounds.y() + y_offset,
        icon_width,
        icon_height);
  icon_view_->SetBoundsRect(icon_bounds);
  bar_->SetBarBoundsRect(GetContentsBounds());
  UpdateState();
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

void LauncherButton::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      AddState(STATE_HOVERED);
      return CustomButton::OnGestureEvent(event);
    case ui::ET_GESTURE_END:
      ClearState(STATE_HOVERED);
      return CustomButton::OnGestureEvent(event);
    case ui::ET_GESTURE_SCROLL_BEGIN:
      host_->PointerPressedOnButton(this, LauncherButtonHost::TOUCH, *event);
      event->SetHandled();
      return;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      host_->PointerDraggedOnButton(this, LauncherButtonHost::TOUCH, *event);
      event->SetHandled();
      return;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      host_->PointerReleasedOnButton(this, LauncherButtonHost::TOUCH, false);
      event->SetHandled();
      return;
    default:
      return CustomButton::OnGestureEvent(event);
  }
}

void LauncherButton::Init() {
  icon_view_ = CreateIconView();

  // TODO: refactor the layers so each button doesn't require 2.
  icon_view_->SetPaintToLayer(true);
  icon_view_->SetFillsBoundsOpaquely(false);
  icon_view_->SetHorizontalAlignment(views::ImageView::CENTER);
  icon_view_->SetVerticalAlignment(views::ImageView::LEADING);

  AddChildView(icon_view_);
}

LauncherButton::IconView* LauncherButton::CreateIconView() {
  return new IconView;
}

bool LauncherButton::IsShelfHorizontal() const {
  return shelf_layout_manager_->IsHorizontalAlignment();
}

void LauncherButton::UpdateState() {
  if (state_ == STATE_NORMAL) {
    bar_->SetVisible(false);
  } else {
    int bar_id;
    if (state_ & (STATE_ACTIVE | STATE_ATTENTION)) {
      bar_id = IDR_AURA_LAUNCHER_UNDERLINE_ACTIVE;
    } else if (state_ & (STATE_HOVERED | STATE_FOCUSED)) {
      bar_id = IDR_AURA_LAUNCHER_UNDERLINE_HOVER;
    } else {
      bar_id = IDR_AURA_LAUNCHER_UNDERLINE_RUNNING;
    }
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const gfx::ImageSkia* image = rb.GetImageNamed(bar_id).ToImageSkia();
    if(SHELF_ALIGNMENT_BOTTOM == shelf_layout_manager_->GetAlignment())
      bar_->SetImage(*image);
    else
      bar_->SetImage(gfx::ImageSkiaOperations::CreateRotatedImage(*image,
          shelf_layout_manager_->SelectValueForShelfAlignment(
              SkBitmapOperations::ROTATION_90_CW,
              SkBitmapOperations::ROTATION_90_CW,
              SkBitmapOperations::ROTATION_270_CW,
              SkBitmapOperations::ROTATION_180_CW)));
    bar_->SetVisible(true);
  }
  icon_view_->SetHorizontalAlignment(
      shelf_layout_manager_->PrimaryAxisValue(views::ImageView::CENTER,
                                              views::ImageView::LEADING));
  icon_view_->SetVerticalAlignment(
      shelf_layout_manager_->PrimaryAxisValue(views::ImageView::LEADING,
                                              views::ImageView::CENTER));
  bar_->SetHorizontalAlignment(
      shelf_layout_manager_->SelectValueForShelfAlignment(
          views::ImageView::CENTER,
          views::ImageView::LEADING,
          views::ImageView::TRAILING,
          views::ImageView::CENTER));
  bar_->SetVerticalAlignment(
      shelf_layout_manager_->SelectValueForShelfAlignment(
          views::ImageView::TRAILING,
          views::ImageView::CENTER,
          views::ImageView::CENTER,
          views::ImageView::LEADING));
  bar_->SchedulePaint();
  SchedulePaint();
}

}  // namespace internal
}  // namespace ash
