// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_button.h"

#include <algorithm>

#include "ash/ash_constants.h"
#include "ash/ash_switches.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "base/time/time.h"
#include "grit/ash_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/controls/image_view.h"

namespace {

// Size of the bar. This is along the opposite axis of the shelf. For example,
// if the shelf is aligned horizontally then this is the height of the bar.
const int kBarSize = 3;
const int kIconSize = 32;
const int kIconPad = 5;
const int kIconPadVertical = 6;
const int kAttentionThrobDurationMS = 800;
const int kMaxAnimationSeconds = 10;

// Simple AnimationDelegate that owns a single ThrobAnimation instance to
// keep all Draw Attention animations in sync.
class ShelfButtonAnimation : public gfx::AnimationDelegate {
 public:
  class Observer {
   public:
    virtual void AnimationProgressed() = 0;

   protected:
    virtual ~Observer() {}
  };

  static ShelfButtonAnimation* GetInstance() {
    static ShelfButtonAnimation* s_instance = new ShelfButtonAnimation();
    return s_instance;
  }

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
    if (!observers_.might_have_observers())
      animation_.Stop();
  }

  int GetAlpha() {
    return GetThrobAnimation().CurrentValueBetween(0, 255);
  }

  double GetAnimation() {
    return GetThrobAnimation().GetCurrentValue();
  }

 private:
  ShelfButtonAnimation()
      : animation_(this) {
    animation_.SetThrobDuration(kAttentionThrobDurationMS);
    animation_.SetTweenType(gfx::Tween::SMOOTH_IN_OUT);
  }

  ~ShelfButtonAnimation() override {}

  gfx::ThrobAnimation& GetThrobAnimation() {
    if (!animation_.is_animating()) {
      animation_.Reset();
      animation_.StartThrobbing(-1 /*throb indefinitely*/);
    }
    return animation_;
  }

  // gfx::AnimationDelegate
  void AnimationProgressed(const gfx::Animation* animation) override {
    if (animation != &animation_)
      return;
    if (!animation_.is_animating())
      return;
    FOR_EACH_OBSERVER(Observer, observers_, AnimationProgressed());
  }

  gfx::ThrobAnimation animation_;
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ShelfButtonAnimation);
};

}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// ShelfButton::BarView

class ShelfButton::BarView : public views::ImageView,
                             public ShelfButtonAnimation::Observer {
 public:
  BarView(Shelf* shelf)
      : shelf_(shelf),
        show_attention_(false),
        animation_end_time_(base::TimeTicks()),
        animating_(false) {
    // Make sure the events reach the parent view for handling.
    set_interactive(false);
  }

  ~BarView() override {
    if (show_attention_)
      ShelfButtonAnimation::GetInstance()->RemoveObserver(this);
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    if (show_attention_) {
      int alpha =
          animating_ ? ShelfButtonAnimation::GetInstance()->GetAlpha() : 255;
      canvas->SaveLayerAlpha(alpha);
      views::ImageView::OnPaint(canvas);
      canvas->Restore();
    } else {
      views::ImageView::OnPaint(canvas);
    }
  }

  // ShelfButtonAnimation::Observer
  void AnimationProgressed() override {
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
      if (show_attention_) {
        animating_ = true;
        animation_end_time_ = base::TimeTicks::Now() +
            base::TimeDelta::FromSeconds(kMaxAnimationSeconds);
        ShelfButtonAnimation::GetInstance()->AddObserver(this);
      } else {
        animating_ = false;
        ShelfButtonAnimation::GetInstance()->RemoveObserver(this);
      }
    }
    UpdateBounds();
  }

 private:
  void UpdateBounds() {
    gfx::Rect bounds = base_bounds_;
    if (show_attention_) {
      // Scale from .35 to 1.0 of the total width (which is wider than the
      // visible width of the image), so the animation "rests" briefly at full
      // visible width.  Cap bounds length at kIconSize to prevent visual
      // flutter while centering bar within further expanding bounds.
      double animation = animating_ ?
          ShelfButtonAnimation::GetInstance()->GetAnimation() : 1.0;
      double scale = .35 + .65 * animation;
      if (shelf_->IsHorizontalAlignment()) {
        int width = base_bounds_.width() * scale;
        bounds.set_width(std::min(width, kIconSize));
        int x_offset = (base_bounds_.width() - bounds.width()) / 2;
        bounds.set_x(base_bounds_.x() + x_offset);
        UpdateAnimating(bounds.width() == kIconSize);
      } else {
        int height = base_bounds_.height() * scale;
        bounds.set_height(std::min(height, kIconSize));
        int y_offset = (base_bounds_.height() - bounds.height()) / 2;
        bounds.set_y(base_bounds_.y() + y_offset);
        UpdateAnimating(bounds.height() == kIconSize);
      }
    }
    SetBoundsRect(bounds);
  }

  void UpdateAnimating(bool max_length) {
    if (!max_length)
      return;
    if (base::TimeTicks::Now() > animation_end_time_) {
      animating_ = false;
      ShelfButtonAnimation::GetInstance()->RemoveObserver(this);
    }
  }

  Shelf* shelf_;
  bool show_attention_;
  base::TimeTicks animation_end_time_;  // For attention throbbing underline.
  bool animating_;  // Is time-limited attention animation running?
  gfx::Rect base_bounds_;

  DISALLOW_COPY_AND_ASSIGN(BarView);
};

////////////////////////////////////////////////////////////////////////////////
// ShelfButton

// static
const char ShelfButton::kViewClassName[] = "ash/ShelfButton";

ShelfButton::ShelfButton(ShelfView* shelf_view)
    : CustomButton(shelf_view),
      shelf_view_(shelf_view),
      icon_view_(new views::ImageView()),
      bar_(new BarView(shelf_view->shelf())),
      state_(STATE_NORMAL),
      destroyed_flag_(nullptr) {
  SetAccessibilityFocusable(true);

  const gfx::ShadowValue kShadows[] = {
      gfx::ShadowValue(gfx::Vector2d(0, 2), 0, SkColorSetARGB(0x1A, 0, 0, 0)),
      gfx::ShadowValue(gfx::Vector2d(0, 3), 1, SkColorSetARGB(0x1A, 0, 0, 0)),
      gfx::ShadowValue(gfx::Vector2d(0, 0), 1, SkColorSetARGB(0x54, 0, 0, 0)),
  };
  icon_shadows_.assign(kShadows, kShadows + arraysize(kShadows));

  // TODO: refactor the layers so each button doesn't require 2.
  icon_view_->SetPaintToLayer(true);
  icon_view_->layer()->SetFillsBoundsOpaquely(false);
  icon_view_->SetHorizontalAlignment(views::ImageView::CENTER);
  icon_view_->SetVerticalAlignment(views::ImageView::LEADING);
  // Do not make this interactive, so that events are sent to ShelfView.
  icon_view_->set_interactive(false);

  AddChildView(bar_);
  AddChildView(icon_view_);
}

ShelfButton::~ShelfButton() {
  if (destroyed_flag_)
    *destroyed_flag_ = true;
}

void ShelfButton::SetShadowedImage(const gfx::ImageSkia& image) {
  icon_view_->SetImage(gfx::ImageSkiaOperations::CreateImageWithDropShadow(
      image, icon_shadows_));
}

void ShelfButton::SetImage(const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // TODO: need an empty image.
    icon_view_->SetImage(image);
    return;
  }

  // Resize the image maintaining our aspect ratio.
  float aspect_ratio =
      static_cast<float>(image.width()) / static_cast<float>(image.height());
  int height = kIconSize;
  int width = static_cast<int>(aspect_ratio * height);
  if (width > kIconSize) {
    width = kIconSize;
    height = static_cast<int>(width / aspect_ratio);
  }

  if (width == image.width() && height == image.height()) {
    SetShadowedImage(image);
    return;
  }

  SetShadowedImage(gfx::ImageSkiaOperations::CreateResizedImage(image,
      skia::ImageOperations::RESIZE_BEST, gfx::Size(width, height)));
}

const gfx::ImageSkia& ShelfButton::GetImage() const {
  return icon_view_->GetImage();
}

void ShelfButton::AddState(State state) {
  if (!(state_ & state)) {
    state_ |= state;
    Layout();
    if (state & STATE_ATTENTION)
      bar_->ShowAttention(true);
  }
}

void ShelfButton::ClearState(State state) {
  if (state_ & state) {
    state_ &= ~state;
    Layout();
    if (state & STATE_ATTENTION)
      bar_->ShowAttention(false);
  }
}

gfx::Rect ShelfButton::GetIconBounds() const {
  return icon_view_->bounds();
}

void ShelfButton::ShowContextMenu(const gfx::Point& p,
                                  ui::MenuSourceType source_type) {
  if (!context_menu_controller())
    return;

  bool destroyed = false;
  destroyed_flag_ = &destroyed;

  CustomButton::ShowContextMenu(p, source_type);

  if (!destroyed) {
    destroyed_flag_ = nullptr;
    // The menu will not propagate mouse events while its shown. To address,
    // the hover state gets cleared once the menu was shown (and this was not
    // destroyed).
    ClearState(STATE_HOVERED);
  }
}

const char* ShelfButton::GetClassName() const {
  return kViewClassName;
}

bool ShelfButton::OnMousePressed(const ui::MouseEvent& event) {
  CustomButton::OnMousePressed(event);
  shelf_view_->PointerPressedOnButton(this, ShelfView::MOUSE, event);
  return true;
}

void ShelfButton::OnMouseReleased(const ui::MouseEvent& event) {
  CustomButton::OnMouseReleased(event);
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, false);
}

void ShelfButton::OnMouseCaptureLost() {
  ClearState(STATE_HOVERED);
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, true);
  CustomButton::OnMouseCaptureLost();
}

bool ShelfButton::OnMouseDragged(const ui::MouseEvent& event) {
  CustomButton::OnMouseDragged(event);
  shelf_view_->PointerDraggedOnButton(this, ShelfView::MOUSE, event);
  return true;
}

void ShelfButton::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_BUTTON;
  state->name = shelf_view_->GetTitleForView(this);
}

void ShelfButton::Layout() {
  const gfx::Rect button_bounds(GetContentsBounds());
  Shelf* shelf = shelf_view_->shelf();
  int icon_pad = shelf->PrimaryAxisValue(kIconPad, kIconPadVertical);
  int x_offset = shelf->PrimaryAxisValue(0, icon_pad);
  int y_offset = shelf->PrimaryAxisValue(icon_pad, 0);

  int icon_width = std::min(kIconSize, button_bounds.width() - x_offset);
  int icon_height = std::min(kIconSize, button_bounds.height() - y_offset);

  // If on the left or top 'invert' the inset so the constant gap is on
  // the interior (towards the center of display) edge of the shelf.
  if (SHELF_ALIGNMENT_LEFT == shelf->alignment())
    x_offset = button_bounds.width() - (kIconSize + icon_pad);

  // Center icon with respect to the secondary axis, and ensure
  // that the icon doesn't occlude the bar highlight.
  if (shelf->IsHorizontalAlignment()) {
    x_offset = std::max(0, button_bounds.width() - icon_width) / 2;
    if (y_offset + icon_height + kBarSize > button_bounds.height())
      icon_height = button_bounds.height() - (y_offset + kBarSize);
  } else {
    y_offset = std::max(0, button_bounds.height() - icon_height) / 2;
    if (x_offset + icon_width + kBarSize > button_bounds.width())
      icon_width = button_bounds.width() - (x_offset + kBarSize);
  }

  // Expand bounds to include shadows.
  gfx::Insets insets_shadows = gfx::ShadowValue::GetMargin(icon_shadows_);
  // Adjust offsets to center icon, not icon + shadow.
  x_offset += (insets_shadows.left() - insets_shadows.right()) / 2;
  y_offset += (insets_shadows.top() - insets_shadows.bottom()) / 2;
  gfx::Rect icon_view_bounds =
      gfx::Rect(button_bounds.x() + x_offset, button_bounds.y() + y_offset,
                icon_width, icon_height);
  icon_view_bounds.Inset(insets_shadows);
  icon_view_->SetBoundsRect(icon_view_bounds);

  // Icon size has been incorrect when running
  // PanelLayoutManagerTest.PanelAlignmentSecondDisplay on valgrind bot, see
  // http://crbug.com/234854.
  DCHECK_LE(icon_width, kIconSize);
  DCHECK_LE(icon_height, kIconSize);

  bar_->SetBarBoundsRect(button_bounds);

  UpdateState();
}

void ShelfButton::ChildPreferredSizeChanged(views::View* child) {
  Layout();
}

void ShelfButton::OnFocus() {
  AddState(STATE_FOCUSED);
  CustomButton::OnFocus();
}

void ShelfButton::OnBlur() {
  ClearState(STATE_FOCUSED);
  CustomButton::OnBlur();
}

void ShelfButton::OnPaint(gfx::Canvas* canvas) {
  CustomButton::OnPaint(canvas);
  if (HasFocus()) {
    gfx::Rect paint_bounds(GetLocalBounds());
    paint_bounds.Inset(1, 1, 1, 1);
    canvas->DrawSolidFocusRect(paint_bounds, kFocusBorderColor);
  }
}

void ShelfButton::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      AddState(STATE_HOVERED);
      return CustomButton::OnGestureEvent(event);
    case ui::ET_GESTURE_END:
      ClearState(STATE_HOVERED);
      return CustomButton::OnGestureEvent(event);
    case ui::ET_GESTURE_SCROLL_BEGIN:
      shelf_view_->PointerPressedOnButton(this, ShelfView::TOUCH, *event);
      event->SetHandled();
      return;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      shelf_view_->PointerDraggedOnButton(this, ShelfView::TOUCH, *event);
      event->SetHandled();
      return;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      shelf_view_->PointerReleasedOnButton(this, ShelfView::TOUCH, false);
      event->SetHandled();
      return;
    default:
      return CustomButton::OnGestureEvent(event);
  }
}

void ShelfButton::UpdateState() {
  UpdateBar();
  Shelf* shelf = shelf_view_->shelf();
  icon_view_->SetHorizontalAlignment(shelf->PrimaryAxisValue(
      views::ImageView::CENTER, views::ImageView::LEADING));
  icon_view_->SetVerticalAlignment(shelf->PrimaryAxisValue(
      views::ImageView::LEADING, views::ImageView::CENTER));
  SchedulePaint();
}

void ShelfButton::UpdateBar() {
  if (state_ & STATE_HIDDEN) {
    bar_->SetVisible(false);
    return;
  }

  int bar_id = 0;
  if (state_ & (STATE_ACTIVE))
    bar_id = IDR_ASH_SHELF_UNDERLINE_ACTIVE;
  else if (state_ & STATE_ATTENTION)
    bar_id = IDR_ASH_SHELF_UNDERLINE_ATTENTION;
  else if (state_ & STATE_RUNNING)
    bar_id = IDR_ASH_SHELF_UNDERLINE_RUNNING;

  if (bar_id != 0) {
    Shelf* shelf = shelf_view_->shelf();
    ResourceBundle* rb = &ResourceBundle::GetSharedInstance();
    gfx::ImageSkia image = *rb->GetImageNamed(bar_id).ToImageSkia();
    if (!shelf->IsHorizontalAlignment()) {
      image = gfx::ImageSkiaOperations::CreateRotatedImage(
          image, shelf->alignment() == SHELF_ALIGNMENT_LEFT
                     ? SkBitmapOperations::ROTATION_90_CW
                     : SkBitmapOperations::ROTATION_270_CW);
    }
    bar_->SetImage(image);
    bar_->SetHorizontalAlignment(shelf->SelectValueForShelfAlignment(
        views::ImageView::CENTER, views::ImageView::LEADING,
        views::ImageView::TRAILING));
    bar_->SetVerticalAlignment(shelf->SelectValueForShelfAlignment(
        views::ImageView::TRAILING, views::ImageView::CENTER,
        views::ImageView::CENTER));
    bar_->SchedulePaint();
  }

  bar_->SetVisible(bar_id != 0 && state_ != STATE_NORMAL);
}

}  // namespace ash
