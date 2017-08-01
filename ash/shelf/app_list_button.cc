// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_button.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/ink_drop_button_listener.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/voice_interaction_overlay.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/timer/timer.h"
#include "chromeos/chromeos_switches.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event_sink.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr int kVoiceInteractionAnimationDelayMs = 200;
constexpr int kVoiceInteractionAnimationHideDelayMs = 500;

}  // namespace

constexpr uint8_t kVoiceInteractionRunningAlpha = 255;     // 100% alpha
constexpr uint8_t kVoiceInteractionNotRunningAlpha = 138;  // 54% alpha

AppListButton::AppListButton(InkDropButtonListener* listener,
                             ShelfView* shelf_view,
                             Shelf* shelf)
    : views::ImageButton(nullptr),
      is_showing_app_list_(false),
      background_color_(kShelfDefaultBaseColor),
      listener_(listener),
      shelf_view_(shelf_view),
      shelf_(shelf) {
  DCHECK(listener_);
  DCHECK(shelf_view_);
  DCHECK(shelf_);
  Shell::Get()->AddShellObserver(this);
  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  set_ink_drop_base_color(kShelfInkDropBaseColor);
  set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE));
  SetSize(gfx::Size(kShelfSize, kShelfSize));
  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
  set_notify_action(CustomButton::NOTIFY_ON_PRESS);

  if (chromeos::switches::IsVoiceInteractionEnabled()) {
    voice_interaction_overlay_ = new VoiceInteractionOverlay(this);
    AddChildView(voice_interaction_overlay_);
    voice_interaction_overlay_->SetVisible(false);
    voice_interaction_animation_delay_timer_.reset(new base::OneShotTimer());
    voice_interaction_animation_hide_delay_timer_.reset(
        new base::OneShotTimer());
  } else {
    voice_interaction_overlay_ = nullptr;
  }
}

AppListButton::~AppListButton() {
  Shell::Get()->RemoveShellObserver(this);
}

void AppListButton::OnAppListShown() {
  AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
  is_showing_app_list_ = true;
  shelf_->UpdateAutoHideState();
}

void AppListButton::OnAppListDismissed() {
  AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  is_showing_app_list_ = false;
  shelf_->UpdateAutoHideState();
}

void AppListButton::UpdateShelfItemBackground(SkColor color) {
  background_color_ = color;
  SchedulePaint();
}

void AppListButton::OnGestureEvent(ui::GestureEvent* event) {
  last_event_is_back_event_ = IsBackEvent(event->location());
  // Handle gesture events that are on the back button.
  if (last_event_is_back_event_) {
    switch (event->type()) {
      case ui::ET_GESTURE_TAP:
        AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED, event);
        GenerateAndSendBackEvent(*event);
        return;
      case ui::ET_GESTURE_TAP_CANCEL:
        AnimateInkDrop(views::InkDropState::HIDDEN, event);
        return;
      case ui::ET_GESTURE_TAP_DOWN:
        AnimateInkDrop(views::InkDropState::ACTION_PENDING, event);
        GenerateAndSendBackEvent(*event);
        return;
      default:
        return;
    }
  }

  // Handle gesture events that are on the app list circle.
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      AnimateInkDrop(views::InkDropState::HIDDEN, event);
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
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_CANCEL:
      if (voice_interaction_overlay_) {
        voice_interaction_overlay_->EndAnimation();
        voice_interaction_animation_delay_timer_->Stop();
      }
      ImageButton::OnGestureEvent(event);
      return;
    case ui::ET_GESTURE_TAP_DOWN:
      if (voice_interaction_overlay_) {
        voice_interaction_animation_delay_timer_->Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(
                kVoiceInteractionAnimationDelayMs),
            base::Bind(&AppListButton::StartVoiceInteractionAnimation,
                       base::Unretained(this)));
      }
      if (!Shell::Get()->IsAppListVisible())
        AnimateInkDrop(views::InkDropState::ACTION_PENDING, event);
      ImageButton::OnGestureEvent(event);
      return;
    case ui::ET_GESTURE_LONG_PRESS:
      if (chromeos::switches::IsVoiceInteractionEnabled()) {
        base::RecordAction(base::UserMetricsAction(
            "VoiceInteraction.Started.AppListButtonLongPress"));
        Shell::Get()->app_list()->StartVoiceInteractionSession();
        DCHECK(voice_interaction_overlay_);
        voice_interaction_overlay_->BurstAnimation();
        event->SetHandled();
      } else {
        ImageButton::OnGestureEvent(event);
      }
      return;
    case ui::ET_GESTURE_LONG_TAP:
      if (chromeos::switches::IsVoiceInteractionEnabled()) {
        // Also consume the long tap event. This happens after the user long
        // presses and lifts the finger. We already handled the long press
        // ignore the long tap to avoid bringing up the context menu again.
        AnimateInkDrop(views::InkDropState::HIDDEN, event);
        event->SetHandled();
      } else {
        ImageButton::OnGestureEvent(event);
      }
      return;
    default:
      ImageButton::OnGestureEvent(event);
      return;
  }
}

bool AppListButton::OnMousePressed(const ui::MouseEvent& event) {
  last_event_is_back_event_ = IsBackEvent(event.location());
  if (last_event_is_back_event_) {
    AnimateInkDrop(views::InkDropState::ACTION_PENDING, &event);
    GenerateAndSendBackEvent(*event.AsLocatedEvent());
  } else {
    ImageButton::OnMousePressed(event);
    shelf_view_->PointerPressedOnButton(this, ShelfView::MOUSE, event);
  }
  return true;
}

void AppListButton::OnMouseReleased(const ui::MouseEvent& event) {
  last_event_is_back_event_ = IsBackEvent(event.location());
  if (last_event_is_back_event_) {
    AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED, &event);
    GenerateAndSendBackEvent(*event.AsLocatedEvent());
  } else {
    ImageButton::OnMouseReleased(event);
    shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, false);
  }
}

void AppListButton::OnMouseCaptureLost() {
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, true);
  ImageButton::OnMouseCaptureLost();
}

bool AppListButton::OnMouseDragged(const ui::MouseEvent& event) {
  ImageButton::OnMouseDragged(event);
  shelf_view_->PointerDraggedOnButton(this, ShelfView::MOUSE, event);
  return true;
}

void AppListButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_BUTTON;
  node_data->SetName(shelf_view_->GetTitleForView(this));
}

std::unique_ptr<views::InkDropRipple> AppListButton::CreateInkDropRipple()
    const {
  gfx::Point center = last_event_is_back_event_ ? GetBackButtonCenterPoint()
                                                : GetAppListButtonCenterPoint();
  gfx::Rect bounds(center.x() - kAppListButtonRadius,
                   center.y() - kAppListButtonRadius, 2 * kAppListButtonRadius,
                   2 * kAppListButtonRadius);
  return base::MakeUnique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

void AppListButton::NotifyClick(const ui::Event& event) {
  ImageButton::NotifyClick(event);
  if (listener_)
    listener_->ButtonPressed(this, event, GetInkDrop());
}

bool AppListButton::ShouldEnterPushedState(const ui::Event& event) {
  if (!shelf_view_->ShouldEventActivateButton(this, event))
    return false;
  if (Shell::Get()->IsAppListVisible())
    return false;
  return views::ImageButton::ShouldEnterPushedState(event);
}

std::unique_ptr<views::InkDrop> AppListButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      CustomButton::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropMask> AppListButton::CreateInkDropMask() const {
  return base::MakeUnique<views::CircleInkDropMask>(
      size(),
      last_event_is_back_event_ ? GetBackButtonCenterPoint()
                                : GetAppListButtonCenterPoint(),
      kAppListButtonRadius);
}

void AppListButton::PaintButtonContents(gfx::Canvas* canvas) {
  const bool is_tablet_mode = Shell::Get()
                                  ->tablet_mode_controller()
                                  ->IsTabletModeWindowManagerEnabled();
  const double current_animation_value =
      shelf_view_->GetAppListButtonAnimationCurrentValue();
  gfx::PointF circle_center(GetAppListButtonCenterPoint());

  // Paint the circular background.
  cc::PaintFlags bg_flags;
  bg_flags.setColor(background_color_);
  bg_flags.setAntiAlias(true);
  bg_flags.setStyle(cc::PaintFlags::kFill_Style);

  if (is_tablet_mode || current_animation_value > 0.0) {
    // Draw the tablet mode app list background. It will look something like
    // [1] when the shelf is horizontal and [2] when the shelf is vertical,
    // where 1. is the back button and 2. is the app launcher circle.
    //                                      _____
    // [1]   _______________         [2]   /  1. \
    //      /               \             |       |
    //      | 1.         2. |             |   2.  |
    //      \_______________/              \_____/

    // Calculate the rectangular bounds of the path. The primary axis will be
    // the distance between the back button and app circle centers and the
    // secondary axis will be 2 * |kAppListButtonRadius|. The origin will be
    // situated such that the back button center and app circle center are
    // located equal distance from the sides parallel to the primary axis. See
    // diagrams below; (1) is the back button and (2) is the app circle.
    //
    //    ___________         ____(1)____
    //    |         |         |         |
    //   (1)       (2)        |         |
    //    |_________|         |         |
    //                        |___(2)___|
    gfx::PointF back_center(GetBackButtonCenterPoint());
    gfx::RectF background_bounds(
        shelf_->PrimaryAxisValue(back_center.x(),
                                 back_center.x() - kAppListButtonRadius),
        shelf_->PrimaryAxisValue(back_center.y() - kAppListButtonRadius,
                                 back_center.y()),
        shelf_->PrimaryAxisValue(circle_center.x() - back_center.x(),
                                 2 * kAppListButtonRadius),
        shelf_->PrimaryAxisValue(2 * kAppListButtonRadius,
                                 circle_center.y() - back_center.y()));

    // Create the path by drawing two circles, one around the back button and
    // one around the app list circle. Join them with the rectangle calculated
    // previously.
    SkPath path;
    path.addCircle(circle_center.x(), circle_center.y(), kAppListButtonRadius);
    path.addCircle(back_center.x(), back_center.y(), kAppListButtonRadius);
    path.addRect(background_bounds.x(), background_bounds.y(),
                 background_bounds.right(), background_bounds.bottom());
    canvas->DrawPath(path, bg_flags);

    // Draw the back button icon.
    // TODO(sammiequon): Check if the back button should be flipped in RTL.
    gfx::ImageSkia back_button =
        CreateVectorIcon(kShelfBackIcon, SK_ColorTRANSPARENT);

    // Fades the back button in or out. The button is opaque when we are in
    // tablet mode and the animation has finished.
    int opacity = current_animation_value > 0.0
                      ? static_cast<int>((is_tablet_mode
                                              ? current_animation_value
                                              : 1.0 - current_animation_value) *
                                         255.0)
                      : 255;
    canvas->DrawImageInt(back_button, back_center.x() - back_button.width() / 2,
                         back_center.y() - back_button.height() / 2, opacity);
  } else {
    canvas->DrawCircle(circle_center, kAppListButtonRadius, bg_flags);
  }

  // Paint a white ring as the foreground for the app list circle. The ceil/dsf
  // math assures that the ring draws sharply and is centered at all scale
  // factors.
  float ring_outer_radius_dp = 7.f;
  float ring_thickness_dp = 1.5f;
  if (chromeos::switches::IsVoiceInteractionEnabled()) {
    ring_outer_radius_dp = 8.f;
    ring_thickness_dp = 1.f;
  }
  {
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float dsf = canvas->UndoDeviceScaleFactor();
    circle_center.Scale(dsf);
    cc::PaintFlags fg_flags;
    fg_flags.setAntiAlias(true);
    fg_flags.setStyle(cc::PaintFlags::kStroke_Style);
    fg_flags.setColor(kShelfIconColor);

    if (chromeos::switches::IsVoiceInteractionEnabled())
      // active: 100% alpha, inactive: 54% alpha
      fg_flags.setAlpha(voice_interaction_running_
                            ? kVoiceInteractionRunningAlpha
                            : kVoiceInteractionNotRunningAlpha);

    const float thickness = std::ceil(ring_thickness_dp * dsf);
    const float radius = std::ceil(ring_outer_radius_dp * dsf) - thickness / 2;
    fg_flags.setStrokeWidth(thickness);
    // Make sure the center of the circle lands on pixel centers.
    canvas->DrawCircle(circle_center, radius, fg_flags);

    if (chromeos::switches::IsVoiceInteractionEnabled()) {
      fg_flags.setAlpha(255);
      const float kCircleRadiusDp = 5.f;
      fg_flags.setStyle(cc::PaintFlags::kFill_Style);
      canvas->DrawCircle(circle_center, std::ceil(kCircleRadiusDp * dsf),
                         fg_flags);
    }
  }
}

gfx::Point AppListButton::GetAppListButtonCenterPoint() const {
  // For a bottom-aligned shelf, the button bounds could have a larger height
  // than width (in the case of touch-dragging the shelf upwards) or a larger
  // width than height (in the case of a shelf hide/show animation), so adjust
  // the y-position of the circle's center to ensure correct layout. Similarly
  // adjust the x-position for a left- or right-aligned shelf. In tablet
  // mode, the button will increase it's primary axis size to accommodate the
  // back button arrow in addition to the app list button circle.
  const int x_mid = width() / 2.f;
  const int y_mid = height() / 2.f;
  const bool is_tablet_mode = Shell::Get()
                                  ->tablet_mode_controller()
                                  ->IsTabletModeWindowManagerEnabled();
  const bool is_animating =
      shelf_view_->GetAppListButtonAnimationCurrentValue() > 0.0;

  ShelfAlignment alignment = shelf_->alignment();
  if (alignment == SHELF_ALIGNMENT_BOTTOM ||
      alignment == SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    if (is_tablet_mode || is_animating) {
      return gfx::Point(width() - kShelfButtonSize / 2.f,
                        kShelfButtonSize / 2.f);
    }
    return gfx::Point(x_mid, x_mid);
  } else if (alignment == SHELF_ALIGNMENT_RIGHT) {
    if (is_tablet_mode || is_animating) {
      return gfx::Point(kShelfButtonSize / 2.f,
                        height() - kShelfButtonSize / 2.f);
    }
    return gfx::Point(y_mid, y_mid);
  } else {
    DCHECK_EQ(alignment, SHELF_ALIGNMENT_LEFT);
    if (is_tablet_mode || is_animating) {
      return gfx::Point(width() - kShelfButtonSize / 2.f,
                        height() - kShelfButtonSize / 2.f);
    }
    return gfx::Point(width() - y_mid, y_mid);
  }
}

gfx::Point AppListButton::GetBackButtonCenterPoint() const {
  if (shelf_->alignment() == SHELF_ALIGNMENT_LEFT)
    return gfx::Point(width() - kShelfButtonSize / 2.f, kShelfButtonSize / 2.f);

  return gfx::Point(kShelfButtonSize / 2.f, kShelfButtonSize / 2.f);
}

void AppListButton::OnAppListVisibilityChanged(bool shown,
                                               aura::Window* root_window) {
  if (shelf_ != Shelf::ForWindow(root_window))
    return;

  if (shown)
    OnAppListShown();
  else
    OnAppListDismissed();
}

void AppListButton::OnVoiceInteractionStatusChanged(bool running) {
  voice_interaction_running_ = running;
  SchedulePaint();

  // Voice interaction window shows up, we start hiding the animation if it is
  // running.
  if (running && voice_interaction_overlay_->IsBursting()) {
    voice_interaction_animation_hide_delay_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(
            kVoiceInteractionAnimationHideDelayMs),
        base::Bind(&VoiceInteractionOverlay::HideAnimation,
                   base::Unretained(voice_interaction_overlay_)));
  }
}

void AppListButton::StartVoiceInteractionAnimation() {
  // We only show the voice interaction icon and related animation when the
  // shelf is at the bottom position and voice interaction is not running.
  ShelfAlignment alignment = shelf_->alignment();
  bool show_icon = (alignment == SHELF_ALIGNMENT_BOTTOM ||
                    alignment == SHELF_ALIGNMENT_BOTTOM_LOCKED) &&
                   !voice_interaction_running_;
  voice_interaction_overlay_->StartAnimation(show_icon);
}

bool AppListButton::IsBackEvent(const gfx::Point& location) {
  if (!Shell::Get()
           ->tablet_mode_controller()
           ->IsTabletModeWindowManagerEnabled()) {
    return false;
  }

  return (location - GetBackButtonCenterPoint()).LengthSquared() <
         (location - GetAppListButtonCenterPoint()).LengthSquared();
}

void AppListButton::GenerateAndSendBackEvent(
    const ui::LocatedEvent& original_event) {
  ui::EventType event_type;
  switch (original_event.type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_GESTURE_TAP_DOWN:
      event_type = ui::ET_KEY_PRESSED;
      break;
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_GESTURE_TAP:
      event_type = ui::ET_KEY_RELEASED;
      break;
    default:
      return;
  }

  // Send the back event to the root window of the app list button's widget.
  const views::Widget* widget = GetWidget();
  if (widget && widget->GetNativeWindow()) {
    aura::Window* root_window = widget->GetNativeWindow()->GetRootWindow();
    ui::KeyEvent key_event(event_type, ui::VKEY_BROWSER_BACK, ui::EF_NONE);
    ignore_result(
        root_window->GetHost()->event_sink()->OnEventFromSource(&key_event));
  }
}

}  // namespace ash
