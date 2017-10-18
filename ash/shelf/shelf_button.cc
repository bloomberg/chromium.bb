// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_button.h"

#include <algorithm>
#include <memory>

#include "ash/ash_constants.h"
#include "ash/shelf/ink_drop_button_listener.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/time/time.h"
#include "skia/ext/image_operations.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/painter.h"

namespace {

constexpr int kIconSize = 32;
constexpr int kAttentionThrobDurationMS = 800;
constexpr int kMaxAnimationSeconds = 10;
constexpr int kIndicatorOffsetFromBottom = 3;
constexpr int kIndicatorRadiusDip = 2;
constexpr SkColor kIndicatorColor = SK_ColorWHITE;

// Shelf item ripple constants.
constexpr int kInkDropSmallSize = 48;
constexpr int kInkDropLargeSize = 60;

// Padding from the edge of the shelf to the application icon when the shelf
// is horizontally and vertically aligned, respectively.
constexpr int kIconPaddingHorizontal = 7;
constexpr int kIconPaddingVertical = 8;

// The time threshold before an item can be dragged.
constexpr int kDragTimeThresholdMs = 300;

// The drag and drop app icon should get scaled by this factor.
constexpr float kAppIconScale = 1.2f;

// The drag and drop app icon scaling up or down animation transition duration.
constexpr int kDragDropAppIconScaleTransitionMs = 20;

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

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
    if (!observers_.might_have_observers())
      animation_.Stop();
  }

  bool HasObserver(Observer* observer) const {
    return observers_.HasObserver(observer);
  }

  SkAlpha GetAlpha() {
    return GetThrobAnimation().CurrentValueBetween(SK_AlphaTRANSPARENT,
                                                   SK_AlphaOPAQUE);
  }

  double GetAnimation() { return GetThrobAnimation().GetCurrentValue(); }

 private:
  ShelfButtonAnimation() : animation_(this) {
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
    for (auto& observer : observers_)
      observer.AnimationProgressed();
  }

  gfx::ThrobAnimation animation_;
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ShelfButtonAnimation);
};

}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// ShelfButton::AppStatusIndicatorView

class ShelfButton::AppStatusIndicatorView
    : public views::View,
      public ShelfButtonAnimation::Observer {
 public:
  AppStatusIndicatorView()
      : show_attention_(false), animation_end_time_(base::TimeTicks()) {
    // Make sure the events reach the parent view for handling.
    set_can_process_events_within_subtree(false);
  }

  ~AppStatusIndicatorView() override {
    ShelfButtonAnimation::GetInstance()->RemoveObserver(this);
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped(canvas);
    if (show_attention_) {
      SkAlpha alpha = ShelfButtonAnimation::GetInstance()->HasObserver(this)
                          ? ShelfButtonAnimation::GetInstance()->GetAlpha()
                          : SK_AlphaOPAQUE;
      canvas->SaveLayerAlpha(alpha);
    }

    DCHECK_EQ(width(), height());
    DCHECK_EQ(kIndicatorRadiusDip, width() / 2);
    const float dsf = canvas->UndoDeviceScaleFactor();
    const int kStrokeWidthPx = 1;
    gfx::PointF center = gfx::RectF(GetLocalBounds()).CenterPoint();
    center.Scale(dsf);

    // Fill the center.
    cc::PaintFlags flags;
    flags.setColor(kIndicatorColor);
    flags.setAntiAlias(true);
    canvas->DrawCircle(center, dsf * kIndicatorRadiusDip - kStrokeWidthPx,
                       flags);

    // Stroke the border.
    flags.setColor(SkColorSetA(SK_ColorBLACK, 0x4D));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    canvas->DrawCircle(
        center, dsf * kIndicatorRadiusDip - kStrokeWidthPx / 2.0f, flags);
  }

  // ShelfButtonAnimation::Observer
  void AnimationProgressed() override {
    UpdateAnimating();
    SchedulePaint();
  }

  void ShowAttention(bool show) {
    if (show_attention_ == show)
      return;

    show_attention_ = show;
    if (show_attention_) {
      animation_end_time_ = base::TimeTicks::Now() +
                            base::TimeDelta::FromSeconds(kMaxAnimationSeconds);
      ShelfButtonAnimation::GetInstance()->AddObserver(this);
    } else {
      ShelfButtonAnimation::GetInstance()->RemoveObserver(this);
    }
  }

 private:
  void UpdateAnimating() {
    if (base::TimeTicks::Now() > animation_end_time_)
      ShelfButtonAnimation::GetInstance()->RemoveObserver(this);
  }

  bool show_attention_;
  base::TimeTicks animation_end_time_;  // For attention throbbing underline.

  DISALLOW_COPY_AND_ASSIGN(AppStatusIndicatorView);
};

////////////////////////////////////////////////////////////////////////////////
// ShelfButton

// static
const char ShelfButton::kViewClassName[] = "ash/ShelfButton";

ShelfButton::ShelfButton(InkDropButtonListener* listener, ShelfView* shelf_view)
    : Button(nullptr),
      listener_(listener),
      shelf_view_(shelf_view),
      icon_view_(new views::ImageView()),
      indicator_(new AppStatusIndicatorView()),
      state_(STATE_NORMAL),
      destroyed_flag_(nullptr) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetInkDropMode(InkDropMode::ON);
  set_ink_drop_base_color(kShelfInkDropBaseColor);
  set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);

  const gfx::ShadowValue kShadows[] = {
      gfx::ShadowValue(gfx::Vector2d(0, 2), 0, SkColorSetARGB(0x1A, 0, 0, 0)),
      gfx::ShadowValue(gfx::Vector2d(0, 3), 1, SkColorSetARGB(0x1A, 0, 0, 0)),
      gfx::ShadowValue(gfx::Vector2d(0, 0), 1, SkColorSetARGB(0x54, 0, 0, 0)),
  };
  icon_shadows_.assign(kShadows, kShadows + arraysize(kShadows));

  // TODO: refactor the layers so each button doesn't require 2.
  icon_view_->SetPaintToLayer();
  icon_view_->layer()->SetFillsBoundsOpaquely(false);
  icon_view_->SetHorizontalAlignment(views::ImageView::CENTER);
  icon_view_->SetVerticalAlignment(views::ImageView::LEADING);
  // Do not make this interactive, so that events are sent to ShelfView.
  icon_view_->set_can_process_events_within_subtree(false);

  AddChildView(indicator_);
  AddChildView(icon_view_);

  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
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

  SetShadowedImage(gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, gfx::Size(width, height)));
}

const gfx::ImageSkia& ShelfButton::GetImage() const {
  return icon_view_->GetImage();
}

void ShelfButton::AddState(State state) {
  if (!(state_ & state)) {
    state_ |= state;
    Layout();
    if (state & STATE_ATTENTION)
      indicator_->ShowAttention(true);

    if (state & STATE_DRAGGING)
      ScaleAppIcon(true);
  }
}

void ShelfButton::ClearState(State state) {
  if (state_ & state) {
    state_ &= ~state;
    Layout();
    if (state & STATE_ATTENTION)
      indicator_->ShowAttention(false);

    if (state & STATE_DRAGGING)
      ScaleAppIcon(false);
  }
}

gfx::Rect ShelfButton::GetIconBounds() const {
  return icon_view_->bounds();
}

views::InkDrop* ShelfButton::GetInkDropForTesting() {
  return GetInkDrop();
}

void ShelfButton::OnDragStarted(const ui::LocatedEvent* event) {
  AnimateInkDrop(views::InkDropState::HIDDEN, event);
}

void ShelfButton::ShowContextMenu(const gfx::Point& p,
                                  ui::MenuSourceType source_type) {
  if (!context_menu_controller())
    return;

  bool destroyed = false;
  destroyed_flag_ = &destroyed;

  Button::ShowContextMenu(p, source_type);

  if (!destroyed) {
    destroyed_flag_ = nullptr;
    // The menu will not propagate mouse events while its shown. To address,
    // the hover state gets cleared once the menu was shown (and this was not
    // destroyed). In case context menu is shown target view does not receive
    // OnMouseReleased events and we need to cancel capture manually.
    if (shelf_view_->drag_view() == this)
      OnMouseCaptureLost();
    else
      ClearState(STATE_HOVERED);
  }
}

const char* ShelfButton::GetClassName() const {
  return kViewClassName;
}

bool ShelfButton::OnMousePressed(const ui::MouseEvent& event) {
  Button::OnMousePressed(event);
  shelf_view_->PointerPressedOnButton(this, ShelfView::MOUSE, event);
  drag_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kDragTimeThresholdMs),
      base::Bind(&ShelfButton::OnTouchDragTimer, base::Unretained(this)));
  return true;
}

void ShelfButton::OnMouseReleased(const ui::MouseEvent& event) {
  Button::OnMouseReleased(event);
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, false);
  drag_timer_.Stop();
  ClearState(STATE_DRAGGING);
}

void ShelfButton::OnMouseCaptureLost() {
  ClearState(STATE_HOVERED);
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, true);
  Button::OnMouseCaptureLost();
}

bool ShelfButton::OnMouseDragged(const ui::MouseEvent& event) {
  Button::OnMouseDragged(event);
  shelf_view_->PointerDraggedOnButton(this, ShelfView::MOUSE, event);
  return true;
}

void ShelfButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_BUTTON;
  node_data->SetName(shelf_view_->GetTitleForView(this));
}

void ShelfButton::Layout() {
  const gfx::Rect button_bounds(GetContentsBounds());
  Shelf* shelf = shelf_view_->shelf();
  const bool is_horizontal_shelf = shelf->IsHorizontalAlignment();
  const int icon_pad =
      is_horizontal_shelf ? kIconPaddingHorizontal : kIconPaddingVertical;
  int x_offset = is_horizontal_shelf ? 0 : icon_pad;
  int y_offset = is_horizontal_shelf ? icon_pad : 0;

  int icon_width = std::min(kIconSize, button_bounds.width() - x_offset);
  int icon_height = std::min(kIconSize, button_bounds.height() - y_offset);

  // If on the left or top 'invert' the inset so the constant gap is on
  // the interior (towards the center of display) edge of the shelf.
  if (SHELF_ALIGNMENT_LEFT == shelf->alignment())
    x_offset = button_bounds.width() - (kIconSize + icon_pad);

  // Center icon with respect to the secondary axis.
  if (is_horizontal_shelf)
    x_offset = std::max(0, button_bounds.width() - icon_width) / 2;
  else
    y_offset = std::max(0, button_bounds.height() - icon_height) / 2;

  // Expand bounds to include shadows.
  gfx::Insets insets_shadows = gfx::ShadowValue::GetMargin(icon_shadows_);
  // Adjust offsets to center icon, not icon + shadow.
  x_offset += (insets_shadows.left() - insets_shadows.right()) / 2;
  y_offset += (insets_shadows.top() - insets_shadows.bottom()) / 2;
  gfx::Rect icon_view_bounds =
      gfx::Rect(button_bounds.x() + x_offset, button_bounds.y() + y_offset,
                icon_width, icon_height);
  // The indicator should be aligned with the icon, not the icon + shadow.
  gfx::Point indicator_midpoint = icon_view_bounds.CenterPoint();
  icon_view_bounds.Inset(insets_shadows);
  icon_view_bounds.AdjustToFit(gfx::Rect(size()));
  icon_view_->SetBoundsRect(icon_view_bounds);

  // Icon size has been incorrect when running
  // PanelLayoutManagerTest.PanelAlignmentSecondDisplay on valgrind bot, see
  // http://crbug.com/234854.
  DCHECK_LE(icon_width, kIconSize);
  DCHECK_LE(icon_height, kIconSize);

  switch (shelf->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      indicator_midpoint.set_y(button_bounds.bottom() - kIndicatorRadiusDip -
                               kIndicatorOffsetFromBottom);
      break;
    case SHELF_ALIGNMENT_LEFT:
      indicator_midpoint.set_x(button_bounds.x() + kIndicatorRadiusDip +
                               kIndicatorOffsetFromBottom);
      break;
    case SHELF_ALIGNMENT_RIGHT:
      indicator_midpoint.set_x(button_bounds.right() - kIndicatorRadiusDip -
                               kIndicatorOffsetFromBottom);
      break;
  }

  gfx::Rect indicator_bounds(indicator_midpoint, gfx::Size());
  indicator_bounds.Inset(gfx::Insets(-kIndicatorRadiusDip));
  indicator_->SetBoundsRect(indicator_bounds);

  UpdateState();
}

void ShelfButton::ChildPreferredSizeChanged(views::View* child) {
  Layout();
}

void ShelfButton::OnFocus() {
  AddState(STATE_FOCUSED);
  Button::OnFocus();
}

void ShelfButton::OnBlur() {
  ClearState(STATE_FOCUSED);
  Button::OnBlur();
}

void ShelfButton::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      AddState(STATE_HOVERED);
      drag_timer_.Start(
          FROM_HERE, base::TimeDelta::FromMilliseconds(kDragTimeThresholdMs),
          base::Bind(&ShelfButton::OnTouchDragTimer, base::Unretained(this)));
      event->SetHandled();
      break;
    case ui::ET_GESTURE_END:
      drag_timer_.Stop();
      ClearState(STATE_HOVERED);
      ClearState(STATE_DRAGGING);
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (state_ & STATE_DRAGGING) {
        shelf_view_->PointerPressedOnButton(this, ShelfView::TOUCH, *event);
        event->SetHandled();
      } else {
        drag_timer_.Stop();
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if ((state_ & STATE_DRAGGING) && shelf_view_->IsDraggedView(this)) {
        shelf_view_->PointerDraggedOnButton(this, ShelfView::TOUCH, *event);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      if (state_ & STATE_DRAGGING) {
        ClearState(STATE_DRAGGING);
        shelf_view_->PointerReleasedOnButton(this, ShelfView::TOUCH, false);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_LONG_TAP:
      // Handle LONG_TAP to avoid opening the context menu twice.
      event->SetHandled();
      break;
    default:
      break;
  }

  if (!event->handled())
    return Button::OnGestureEvent(event);
}

std::unique_ptr<views::InkDropRipple> ShelfButton::CreateInkDropRipple() const {
  return std::make_unique<views::SquareInkDropRipple>(
      gfx::Size(kInkDropLargeSize, kInkDropLargeSize),
      kInkDropLargeCornerRadius,
      gfx::Size(kInkDropSmallSize, kInkDropSmallSize),
      kInkDropSmallCornerRadius, GetLocalBounds().CenterPoint(),
      GetInkDropBaseColor(), ink_drop_visible_opacity());
}

bool ShelfButton::ShouldEnterPushedState(const ui::Event& event) {
  if (!shelf_view_->ShouldEventActivateButton(this, event))
    return false;

  return Button::ShouldEnterPushedState(event);
}

std::unique_ptr<views::InkDrop> ShelfButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      Button::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  return std::move(ink_drop);
}

void ShelfButton::NotifyClick(const ui::Event& event) {
  Button::NotifyClick(event);
  if (listener_)
    listener_->ButtonPressed(this, event, GetInkDrop());
}

void ShelfButton::UpdateState() {
  indicator_->SetVisible(!(state_ & STATE_HIDDEN) &&
                         (state_ & STATE_ACTIVE || state_ & STATE_ATTENTION ||
                          state_ & STATE_RUNNING));

  const bool is_horizontal_shelf =
      shelf_view_->shelf()->IsHorizontalAlignment();
  icon_view_->SetHorizontalAlignment(is_horizontal_shelf
                                         ? views::ImageView::CENTER
                                         : views::ImageView::LEADING);
  icon_view_->SetVerticalAlignment(is_horizontal_shelf
                                       ? views::ImageView::LEADING
                                       : views::ImageView::CENTER);
  SchedulePaint();
}

void ShelfButton::OnTouchDragTimer() {
  AddState(STATE_DRAGGING);
}

void ShelfButton::ScaleAppIcon(bool scale_up) {
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kDragDropAppIconScaleTransitionMs));

  if (scale_up) {
    layer()->SetTransform(gfx::GetScaleTransform(
        gfx::Rect(layer()->bounds().size()).CenterPoint(), kAppIconScale));
  } else {
    layer()->SetTransform(gfx::Transform());
  }
}

}  // namespace ash
