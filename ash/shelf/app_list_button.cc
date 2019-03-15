// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_button.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/assistant_overlay.h"
#include "ash/shelf/ink_drop_button_listener.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/shell_state.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
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
constexpr uint8_t kVoiceInteractionRunningAlpha = 255;     // 100% alpha
constexpr uint8_t kVoiceInteractionNotRunningAlpha = 138;  // 54% alpha

bool IsHomeScreenAvailable() {
  return Shell::Get()->home_screen_controller()->IsHomeScreenAvailable();
}

}  // namespace

// static
const char AppListButton::kViewClassName[] = "ash/AppListButton";

AppListButton::AppListButton(ShelfView* shelf_view, Shelf* shelf)
    : ShelfControlButton(shelf_view), shelf_(shelf) {
  DCHECK(shelf_);
  Shell::Get()->app_list_controller()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);

  Shell::Get()->voice_interaction_controller()->AddLocalObserver(this);
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE));
  set_notify_action(Button::NOTIFY_ON_PRESS);
  set_has_ink_drop_action_on_click(false);

  // Initialize voice interaction overlay and sync the flags if active user
  // session has already started. This could happen when an external monitor
  // is plugged in.
  if (Shell::Get()->session_controller()->IsActiveUserSessionStarted() &&
      chromeos::switches::IsAssistantEnabled()) {
    InitializeVoiceInteractionOverlay();
  }
}

AppListButton::~AppListButton() {
  // AppListController and TabletModeController are destroyed early when Shell
  // is being destroyed, they may not exist.
  if (Shell::Get()->app_list_controller())
    Shell::Get()->app_list_controller()->RemoveObserver(this);
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->voice_interaction_controller()->RemoveLocalObserver(this);
}

void AppListButton::OnAppListShown() {
  // Do not show a highlight if the home screen is available, since the home
  // screen view is always open in the background.
  if (!IsHomeScreenAvailable())
    AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
  is_showing_app_list_ = true;
  shelf_->UpdateAutoHideState();
}

void AppListButton::OnAppListDismissed() {
  AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  is_showing_app_list_ = false;
  shelf_->UpdateAutoHideState();
}

void AppListButton::OnGestureEvent(ui::GestureEvent* event) {
  // Handle gesture events that are on the app list circle.
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_CANCEL:
      if (UseVoiceInteractionStyle()) {
        assistant_overlay_->EndAnimation();
        assistant_animation_delay_timer_->Stop();
      }
      if (!Shell::Get()->app_list_controller()->IsVisible() ||
          IsHomeScreenAvailable()) {
        AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED, event);
      }

      Button::OnGestureEvent(event);
      return;
    case ui::ET_GESTURE_TAP_DOWN:
      // If |!ShouldEnterPushedState|, Button::OnGestureEvent will not set
      // the event to be handled. This will cause the |ET_GESTURE_TAP| or
      // |ET_GESTURE_TAP_CANCEL| not to be sent to |app_list_button|, therefore
      // leaving the assistant overlay ripple stays visible.
      if (!ShouldEnterPushedState(*event))
        return;

      if (UseVoiceInteractionStyle()) {
        assistant_animation_delay_timer_->Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(
                kVoiceInteractionAnimationDelayMs),
            base::Bind(&AppListButton::StartVoiceInteractionAnimation,
                       base::Unretained(this)));
      }
      if (!Shell::Get()->app_list_controller()->IsVisible() ||
          IsHomeScreenAvailable()) {
        AnimateInkDrop(views::InkDropState::ACTION_PENDING, event);
      }

      Button::OnGestureEvent(event);
      // If assistant overlay animation starts, we need to make sure the event
      // is handled in order to end the animation in |ET_GESTURE_TAP| or
      // |ET_GESTURE_TAP_CANCEL|.
      DCHECK(event->handled());
      return;
    case ui::ET_GESTURE_LONG_PRESS:
      if (UseVoiceInteractionStyle()) {
        base::RecordAction(base::UserMetricsAction(
            "VoiceInteraction.Started.AppListButtonLongPress"));
        assistant_overlay_->BurstAnimation();
        event->SetHandled();
        Shell::Get()->shell_state()->SetRootWindowForNewWindows(
            GetWidget()->GetNativeWindow()->GetRootWindow());
        Shell::Get()->assistant_controller()->ui_controller()->ShowUi(
            AssistantEntryPoint::kLongPressLauncher);
      } else {
        Button::OnGestureEvent(event);
      }
      return;
    case ui::ET_GESTURE_LONG_TAP:
      if (UseVoiceInteractionStyle()) {
        // Also consume the long tap event. This happens after the user long
        // presses and lifts the finger. We already handled the long press
        // ignore the long tap to avoid bringing up the context menu again.
        AnimateInkDrop(views::InkDropState::HIDDEN, event);
        event->SetHandled();
      } else {
        Button::OnGestureEvent(event);
      }
      return;
    default:
      Button::OnGestureEvent(event);
      return;
  }
}

const char* AppListButton::GetClassName() const {
  return kViewClassName;
}

std::unique_ptr<views::InkDropRipple> AppListButton::CreateInkDropRipple()
    const {
  const int app_list_button_radius = ShelfConstants::control_border_radius();
  gfx::Point center = GetCenterPoint();
  gfx::Rect bounds(center.x() - app_list_button_radius,
                   center.y() - app_list_button_radius,
                   2 * app_list_button_radius, 2 * app_list_button_radius);
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

void AppListButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::PointF circle_center(GetCenterPoint());

  // Paint a white ring as the foreground for the app list circle. The ceil/dsf
  // math assures that the ring draws sharply and is centered at all scale
  // factors.
  float ring_outer_radius_dp = 7.f;
  float ring_thickness_dp = 1.5f;
  if (UseVoiceInteractionStyle()) {
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

    if (UseVoiceInteractionStyle()) {
      mojom::VoiceInteractionState state =
          Shell::Get()
              ->voice_interaction_controller()
              ->voice_interaction_state()
              .value_or(mojom::VoiceInteractionState::STOPPED);
      // active: 100% alpha, inactive: 54% alpha
      fg_flags.setAlpha(state == mojom::VoiceInteractionState::RUNNING
                            ? kVoiceInteractionRunningAlpha
                            : kVoiceInteractionNotRunningAlpha);
    }

    const float thickness = std::ceil(ring_thickness_dp * dsf);
    const float radius = std::ceil(ring_outer_radius_dp * dsf) - thickness / 2;
    fg_flags.setStrokeWidth(thickness);
    // Make sure the center of the circle lands on pixel centers.
    canvas->DrawCircle(circle_center, radius, fg_flags);

    if (UseVoiceInteractionStyle()) {
      fg_flags.setAlpha(255);
      const float kCircleRadiusDp = 5.f;
      fg_flags.setStyle(cc::PaintFlags::kFill_Style);
      canvas->DrawCircle(circle_center, std::ceil(kCircleRadiusDp * dsf),
                         fg_flags);
    }
  }
}

void AppListButton::OnAppListVisibilityChanged(bool shown, int64_t display_id) {
  if (display::Screen::GetScreen()
          ->GetDisplayNearestWindow(GetWidget()->GetNativeWindow())
          .id() != display_id) {
    return;
  }
  if (shown)
    OnAppListShown();
  else
    OnAppListDismissed();
}

void AppListButton::OnVoiceInteractionStatusChanged(
    mojom::VoiceInteractionState state) {
  SchedulePaint();

  if (!assistant_overlay_)
    return;

  switch (state) {
    case mojom::VoiceInteractionState::STOPPED:
      UMA_HISTOGRAM_TIMES(
          "VoiceInteraction.OpenDuration",
          base::TimeTicks::Now() - voice_interaction_start_timestamp_);
      break;
    case mojom::VoiceInteractionState::NOT_READY:
      // If we are showing the bursting or waiting animation, no need to do
      // anything. Otherwise show the waiting animation now.
      // NOTE: No waiting animation for native assistant.
      if (!chromeos::switches::IsAssistantEnabled() &&
          !assistant_overlay_->IsBursting() &&
          !assistant_overlay_->IsWaiting()) {
        assistant_overlay_->WaitingAnimation();
      }
      break;
    case mojom::VoiceInteractionState::RUNNING:
      // we start hiding the animation if it is running.
      if (assistant_overlay_->IsBursting() || assistant_overlay_->IsWaiting()) {
        assistant_animation_hide_delay_timer_->Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(
                kVoiceInteractionAnimationHideDelayMs),
            base::Bind(&AssistantOverlay::HideAnimation,
                       base::Unretained(assistant_overlay_)));
      }

      voice_interaction_start_timestamp_ = base::TimeTicks::Now();
      break;
  }
}

void AppListButton::OnVoiceInteractionSettingsEnabled(bool enabled) {
  SchedulePaint();
}

void AppListButton::OnVoiceInteractionConsentStatusUpdated(
    mojom::ConsentStatus consent_status) {
  SchedulePaint();
}

void AppListButton::OnActiveUserSessionChanged(const AccountId& account_id) {
  SchedulePaint();
  // Initialize voice interaction overlay when primary user session becomes
  // active.
  if (Shell::Get()->session_controller()->IsUserPrimary() &&
      !assistant_overlay_ && chromeos::switches::IsAssistantEnabled()) {
    InitializeVoiceInteractionOverlay();
  }
}

void AppListButton::OnTabletModeStarted() {
  AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
}

void AppListButton::StartVoiceInteractionAnimation() {
  assistant_overlay_->StartAnimation(false);
}

bool AppListButton::UseVoiceInteractionStyle() {
  VoiceInteractionController* controller =
      Shell::Get()->voice_interaction_controller();
  bool settings_enabled = controller->settings_enabled().value_or(false);

  const bool consent_given = controller->consent_status() ==
                             mojom::ConsentStatus::kActivityControlAccepted;

  bool is_feature_allowed =
      controller->allowed_state() == mojom::AssistantAllowedState::ALLOWED;
  if (assistant_overlay_ && is_feature_allowed &&
      (settings_enabled || !consent_given)) {
    return true;
  }
  return false;
}

void AppListButton::InitializeVoiceInteractionOverlay() {
  assistant_overlay_ = new AssistantOverlay(this);
  AddChildView(assistant_overlay_);
  assistant_overlay_->SetVisible(false);
  assistant_animation_delay_timer_ = std::make_unique<base::OneShotTimer>();
  assistant_animation_hide_delay_timer_ =
      std::make_unique<base::OneShotTimer>();
}

}  // namespace ash
