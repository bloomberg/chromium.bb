// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_controller.h"

#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "base/timer/timer.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_processor.h"
#include "ui/gfx/point.h"
#include "ui/gfx/vector2d.h"

namespace ash {

namespace {

// The threshold of mouse movement measured in DIP that will
// initiate a new autoclick.
const int kMovementThreshold = 20;

bool IsModifierKey(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_SHIFT ||
      key_code == ui::VKEY_LSHIFT ||
      key_code == ui::VKEY_CONTROL ||
      key_code == ui::VKEY_LCONTROL ||
      key_code == ui::VKEY_RCONTROL ||
      key_code == ui::VKEY_MENU ||
      key_code == ui::VKEY_LMENU ||
      key_code == ui::VKEY_RMENU;
}

}  // namespace

// static.
const int AutoclickController::kDefaultAutoclickDelayMs = 400;

class AutoclickControllerImpl : public AutoclickController,
                                public ui::EventHandler {
 public:
  AutoclickControllerImpl();
  virtual ~AutoclickControllerImpl();

 private:
  // AutoclickController overrides:
  virtual void SetEnabled(bool enabled) OVERRIDE;
  virtual bool IsEnabled() const OVERRIDE;
  virtual void SetAutoclickDelay(int delay_ms) OVERRIDE;
  virtual int GetAutoclickDelay() const OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;

  void InitClickTimer();

  void DoAutoclick();

  bool enabled_;
  int delay_ms_;
  int mouse_event_flags_;
  scoped_ptr<base::Timer> autoclick_timer_;
  // The position in screen coordinates used to determine
  // the distance the mouse has moved.
  gfx::Point anchor_location_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickControllerImpl);
};


AutoclickControllerImpl::AutoclickControllerImpl()
    : enabled_(false),
      delay_ms_(kDefaultAutoclickDelayMs),
      mouse_event_flags_(ui::EF_NONE),
      anchor_location_(-kMovementThreshold, -kMovementThreshold) {
  InitClickTimer();
}

AutoclickControllerImpl::~AutoclickControllerImpl() {
}

void AutoclickControllerImpl::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;

  if (enabled_) {
    Shell::GetInstance()->AddPreTargetHandler(this);
    autoclick_timer_->Stop();
  } else {
    Shell::GetInstance()->RemovePreTargetHandler(this);
  }
}

bool AutoclickControllerImpl::IsEnabled() const {
  return enabled_;
}

void AutoclickControllerImpl::SetAutoclickDelay(int delay_ms) {
  delay_ms_ = delay_ms;
  InitClickTimer();
}

int AutoclickControllerImpl::GetAutoclickDelay() const {
  return delay_ms_;
}

void AutoclickControllerImpl::InitClickTimer() {
  autoclick_timer_.reset(new base::Timer(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(delay_ms_),
      base::Bind(&AutoclickControllerImpl::DoAutoclick,
                 base::Unretained(this)),
      false));
}

void AutoclickControllerImpl::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_MOVED &&
      !(event->flags() & ui::EF_IS_SYNTHESIZED)) {
    mouse_event_flags_ = event->flags();

    gfx::Point mouse_location = event->root_location();
    ash::wm::ConvertPointToScreen(
        wm::GetRootWindowAt(mouse_location),
        &mouse_location);

    // The distance between the mouse location and the anchor location
    // must exceed a certain threshold to initiate a new autoclick countdown.
    // This ensures that mouse jitter caused by poor motor control does not
    // 1. initiate an unwanted autoclick from rest
    // 2. prevent the autoclick from ever occuring when the mouse
    //    arrives at the target.
    gfx::Vector2d delta = mouse_location - anchor_location_;
    if (delta.LengthSquared() >= kMovementThreshold * kMovementThreshold) {
      anchor_location_ = event->root_location();
      autoclick_timer_->Reset();
    }
  } else if (event->type() == ui::ET_MOUSE_PRESSED) {
    autoclick_timer_->Stop();
  } else if (event->type() == ui::ET_MOUSEWHEEL &&
             autoclick_timer_->IsRunning()) {
    autoclick_timer_->Reset();
  }
}

void AutoclickControllerImpl::OnKeyEvent(ui::KeyEvent* event) {
  int modifier_mask =
      ui::EF_SHIFT_DOWN |
      ui::EF_CONTROL_DOWN |
      ui::EF_ALT_DOWN |
      ui::EF_COMMAND_DOWN |
      ui::EF_EXTENDED;
  int new_modifiers = event->flags() & modifier_mask;
  mouse_event_flags_ = (mouse_event_flags_ & ~modifier_mask) | new_modifiers;

  if (!IsModifierKey(event->key_code()))
    autoclick_timer_->Stop();
}

void AutoclickControllerImpl::OnTouchEvent(ui::TouchEvent* event) {
  autoclick_timer_->Stop();
}

void AutoclickControllerImpl::OnGestureEvent(ui::GestureEvent* event) {
  autoclick_timer_->Stop();
}

void AutoclickControllerImpl::OnScrollEvent(ui::ScrollEvent* event) {
  autoclick_timer_->Stop();
}

void AutoclickControllerImpl::DoAutoclick() {
  gfx::Point screen_location =
      aura::Env::GetInstance()->last_mouse_location();
  aura::Window* root_window = wm::GetRootWindowAt(screen_location);
  DCHECK(root_window) << "Root window not found while attempting autoclick.";

  gfx::Point click_location(screen_location);
  anchor_location_ = click_location;
  wm::ConvertPointFromScreen(root_window, &click_location);

  aura::WindowTreeHost* host = root_window->GetHost();
  host->ConvertPointToHost(&click_location);

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED,
                             click_location,
                             click_location,
                             mouse_event_flags_ | ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED,
                               click_location,
                               click_location,
                               mouse_event_flags_ | ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);

  ui::EventDispatchDetails details =
      host->event_processor()->OnEventFromSource(&press_event);
  if (!details.dispatcher_destroyed)
    details = host->event_processor()->OnEventFromSource(&release_event);
  if (details.dispatcher_destroyed)
    return;
}

// static.
AutoclickController* AutoclickController::CreateInstance() {
  return new AutoclickControllerImpl();
}

}  // namespace ash
