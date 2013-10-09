// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_controller.h"

#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "base/timer/timer.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/point.h"

namespace ash {

namespace {

// The default wait time between last mouse movement and sending the autoclick.
int kDefaultClickWaitTimeMs = 500;

}  // namespace

class AutoclickControllerImpl : public AutoclickController,
                                public ui::EventHandler {
 public:
  AutoclickControllerImpl();
  virtual ~AutoclickControllerImpl();

 private:
  // AutoclickController overrides:
  virtual void SetEnabled(bool enabled) OVERRIDE;
  virtual bool IsEnabled() const OVERRIDE;
  virtual void SetClickWaitTime(int wait_time_ms) OVERRIDE;
  virtual int GetClickWaitTime() const OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

  void InitClickTimer();

  void DoAutoclick();

  bool enabled_;
  int wait_time_ms_;
  int mouse_event_flags_;
  scoped_ptr<base::Timer> autoclick_timer_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickControllerImpl);
};


AutoclickControllerImpl::AutoclickControllerImpl()
    : enabled_(false),
      wait_time_ms_(kDefaultClickWaitTimeMs),
      mouse_event_flags_(ui::EF_NONE) {
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

void AutoclickControllerImpl::SetClickWaitTime(int wait_time_ms) {
  wait_time_ms_ = wait_time_ms;
  InitClickTimer();
}

int AutoclickControllerImpl::GetClickWaitTime() const {
  return wait_time_ms_;
}

void AutoclickControllerImpl::InitClickTimer() {
  autoclick_timer_.reset(new base::Timer(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(wait_time_ms_),
      base::Bind(&AutoclickControllerImpl::DoAutoclick,
                 base::Unretained(this)),
      false));
}

void AutoclickControllerImpl::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_MOVED) {
    mouse_event_flags_ = event->flags();
    autoclick_timer_->Reset();
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
}

void AutoclickControllerImpl::OnTouchEvent(ui::TouchEvent* event) {
  autoclick_timer_->Stop();
}

void AutoclickControllerImpl::DoAutoclick() {
  gfx::Point screen_location =
      aura::Env::GetInstance()->last_mouse_location();
  aura::RootWindow* root_window = wm::GetRootWindowAt(screen_location);
  DCHECK(root_window) << "Root window not found while attempting autoclick.";

  gfx::Point click_location(screen_location);
  wm::ConvertPointFromScreen(root_window, &click_location);
  root_window->ConvertPointToHost(&click_location);

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED,
                             click_location,
                             click_location,
                             mouse_event_flags_ | ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED,
                               click_location,
                               click_location,
                               mouse_event_flags_ | ui::EF_LEFT_MOUSE_BUTTON);

  root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&press_event);
  root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&release_event);
}

// static.
AutoclickController* AutoclickController::CreateInstance() {
  return new AutoclickControllerImpl();
}

}  // namespace ash
