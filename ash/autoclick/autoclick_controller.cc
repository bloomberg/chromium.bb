// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_controller.h"

#include "ash/autoclick/autoclick_ring_handler.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/root_window_finder.h"
#include "base/timer/timer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The threshold of mouse movement measured in DIP that will
// initiate a new autoclick.
const int kMovementThreshold = 20;

bool IsModifierKey(const ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_SHIFT || key_code == ui::VKEY_LSHIFT ||
         key_code == ui::VKEY_CONTROL || key_code == ui::VKEY_LCONTROL ||
         key_code == ui::VKEY_RCONTROL || key_code == ui::VKEY_MENU ||
         key_code == ui::VKEY_LMENU || key_code == ui::VKEY_RMENU;
}

}  // namespace

// static.
base::TimeDelta AutoclickController::GetDefaultAutoclickDelay() {
  return base::TimeDelta::FromMilliseconds(int64_t{kDefaultAutoclickDelayMs});
}

AutoclickController::AutoclickController()
    : enabled_(false),
      event_type_(kDefaultAutoclickEventType),
      tap_down_target_(nullptr),
      delay_(GetDefaultAutoclickDelay()),
      mouse_event_flags_(ui::EF_NONE),
      anchor_location_(-kMovementThreshold, -kMovementThreshold),
      autoclick_ring_handler_(std::make_unique<AutoclickRingHandler>()) {
  InitClickTimer();
}

AutoclickController::~AutoclickController() {
  SetTapDownTarget(nullptr);
}

void AutoclickController::SetTapDownTarget(aura::Window* target) {
  if (tap_down_target_ == target)
    return;

  if (tap_down_target_)
    tap_down_target_->RemoveObserver(this);
  tap_down_target_ = target;
  if (tap_down_target_)
    tap_down_target_->AddObserver(this);
}

void AutoclickController::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;

  if (enabled_)
    Shell::Get()->AddPreTargetHandler(this);
  else
    Shell::Get()->RemovePreTargetHandler(this);

  CancelAutoclickAction();
}

bool AutoclickController::IsEnabled() const {
  return enabled_;
}

void AutoclickController::SetAutoclickDelay(base::TimeDelta delay) {
  delay_ = delay;
  InitClickTimer();
}

void AutoclickController::SetAutoclickEventType(
    mojom::AutoclickEventType type) {
  if (event_type_ == type)
    return;
  event_type_ = type;
  CancelAutoclickAction();
}

void AutoclickController::CreateAutoclickRingWidget(
    const gfx::Point& point_in_screen) {
  aura::Window* target = ash::wm::GetRootWindowAt(point_in_screen);
  SetTapDownTarget(target);
  aura::Window* root_window = target->GetRootWindow();
  widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);
  widget_->Init(params);
  widget_->SetOpacity(1.f);
}

void AutoclickController::UpdateAutoclickRingWidget(
    views::Widget* widget,
    const gfx::Point& point_in_screen) {
  aura::Window* target = ash::wm::GetRootWindowAt(point_in_screen);
  SetTapDownTarget(target);
  aura::Window* root_window = target->GetRootWindow();
  if (widget->GetNativeView()->GetRootWindow() != root_window) {
    views::Widget::ReparentNativeView(
        widget->GetNativeView(),
        Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));
  }
}

void AutoclickController::DoAutoclickAction() {
  gfx::Point point_in_screen =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  anchor_location_ = point_in_screen;
  aura::Window* root_window = wm::GetRootWindowAt(point_in_screen);
  DCHECK(root_window) << "Root window not found while attempting autoclick.";

  gfx::Point location_in_pixels(point_in_screen);
  ::wm::ConvertPointFromScreen(root_window, &location_in_pixels);
  aura::WindowTreeHost* host = root_window->GetHost();
  host->ConvertDIPToPixels(&location_in_pixels);

  // TODO(katie): Implement drag-and-drop.
  if (event_type_ == mojom::AutoclickEventType::kLeftClick ||
      event_type_ == mojom::AutoclickEventType::kRightClick ||
      event_type_ == mojom::AutoclickEventType::kDoubleClick) {
    int button = event_type_ == mojom::AutoclickEventType::kRightClick
                     ? ui::EF_RIGHT_MOUSE_BUTTON
                     : ui::EF_LEFT_MOUSE_BUTTON;
    ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, location_in_pixels,
                               location_in_pixels, ui::EventTimeForNow(),
                               mouse_event_flags_ | button, button);
    ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, location_in_pixels,
                                 location_in_pixels, ui::EventTimeForNow(),
                                 mouse_event_flags_ | button, button);

    ui::EventDispatchDetails details =
        host->event_sink()->OnEventFromSource(&press_event);
    if (details.dispatcher_destroyed)
      return;
    details = host->event_sink()->OnEventFromSource(&release_event);

    // Now a single click has been completed.
    if (event_type_ != mojom::AutoclickEventType::kDoubleClick ||
        details.dispatcher_destroyed)
      return;

    ui::MouseEvent double_press_event(
        ui::ET_MOUSE_PRESSED, location_in_pixels, location_in_pixels,
        ui::EventTimeForNow(),
        mouse_event_flags_ | button | ui::EF_IS_DOUBLE_CLICK, button);
    ui::MouseEvent double_release_event(
        ui::ET_MOUSE_RELEASED, location_in_pixels, location_in_pixels,
        ui::EventTimeForNow(),
        mouse_event_flags_ | button | ui::EF_IS_DOUBLE_CLICK, button);
    details = host->event_sink()->OnEventFromSource(&double_press_event);
    if (details.dispatcher_destroyed)
      return;
    details = host->event_sink()->OnEventFromSource(&double_release_event);
  }
}

void AutoclickController::CancelAutoclickAction() {
  if (autoclick_timer_) {
    autoclick_timer_->Stop();
  }
  autoclick_ring_handler_->StopGesture();
  SetTapDownTarget(nullptr);
}

void AutoclickController::InitClickTimer() {
  CancelAutoclickAction();
  autoclick_timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, delay_,
      base::BindRepeating(&AutoclickController::DoAutoclickAction,
                          base::Unretained(this)));
}

void AutoclickController::UpdateRingWidget(const gfx::Point& point_in_screen) {
  if (!widget_) {
    CreateAutoclickRingWidget(point_in_screen);
  } else {
    UpdateAutoclickRingWidget(widget_.get(), point_in_screen);
  }
}

void AutoclickController::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(event->target());
  if (event_type_ == mojom::AutoclickEventType::kNoAction)
    return;
  gfx::Point point_in_screen = event->target()->GetScreenLocation(*event);
  if (event->type() == ui::ET_MOUSE_MOVED &&
      !(event->flags() & ui::EF_IS_SYNTHESIZED)) {
    mouse_event_flags_ = event->flags();
    UpdateRingWidget(point_in_screen);

    // The distance between the mouse location and the anchor location
    // must exceed a certain threshold to initiate a new autoclick countdown.
    // This ensures that mouse jitter caused by poor motor control does not
    // 1. initiate an unwanted autoclick from rest
    // 2. prevent the autoclick from ever occurring when the mouse
    //    arrives at the target.
    gfx::Vector2d delta = point_in_screen - anchor_location_;
    if (delta.LengthSquared() >= kMovementThreshold * kMovementThreshold) {
      anchor_location_ = point_in_screen;
      autoclick_timer_->Reset();
      autoclick_ring_handler_->StartGesture(delay_, anchor_location_,
                                            widget_.get());
    } else if (autoclick_timer_->IsRunning()) {
      autoclick_ring_handler_->SetGestureCenter(point_in_screen, widget_.get());
    }
  } else if (event->type() == ui::ET_MOUSE_PRESSED) {
    CancelAutoclickAction();
  } else if (event->type() == ui::ET_MOUSEWHEEL &&
             autoclick_timer_->IsRunning()) {
    autoclick_timer_->Reset();
    UpdateRingWidget(point_in_screen);
    autoclick_ring_handler_->StartGesture(delay_, anchor_location_,
                                          widget_.get());
  }
}

void AutoclickController::OnKeyEvent(ui::KeyEvent* event) {
  int modifier_mask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                      ui::EF_IS_EXTENDED_KEY;
  int new_modifiers = event->flags() & modifier_mask;
  mouse_event_flags_ = (mouse_event_flags_ & ~modifier_mask) | new_modifiers;

  if (!IsModifierKey(event->key_code()))
    CancelAutoclickAction();
}

void AutoclickController::OnTouchEvent(ui::TouchEvent* event) {
  CancelAutoclickAction();
}

void AutoclickController::OnGestureEvent(ui::GestureEvent* event) {
  CancelAutoclickAction();
}

void AutoclickController::OnScrollEvent(ui::ScrollEvent* event) {
  CancelAutoclickAction();
}

void AutoclickController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(tap_down_target_, window);
  CancelAutoclickAction();
}

}  // namespace ash
