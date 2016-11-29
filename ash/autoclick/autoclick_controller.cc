// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_controller.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/autoclick/common/autoclick_controller_common.h"
#include "ash/autoclick/common/autoclick_controller_common_delegate.h"
#include "ash/common/wm/root_window_finder.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

// static.
base::TimeDelta AutoclickController::GetDefaultAutoclickDelay() {
  return base::TimeDelta::FromMilliseconds(int64_t{kDefaultAutoclickDelayMs});
}

const int AutoclickController::kDefaultAutoclickDelayMs = 1000;

class AutoclickControllerImpl : public AutoclickController,
                                public ui::EventHandler,
                                public AutoclickControllerCommonDelegate,
                                public aura::WindowObserver {
 public:
  AutoclickControllerImpl();
  ~AutoclickControllerImpl() override;

 private:
  void SetTapDownTarget(aura::Window* target);

  // AutoclickController overrides:
  void SetEnabled(bool enabled) override;
  bool IsEnabled() const override;
  void SetAutoclickDelay(base::TimeDelta delay) override;

  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;

  // AutoclickControllerCommonDelegate overrides:
  views::Widget* CreateAutoclickRingWidget(
      const gfx::Point& event_location) override;
  void UpdateAutoclickRingWidget(views::Widget* widget,
                                 const gfx::Point& event_location) override;
  void DoAutoclick(const gfx::Point& event_location,
                   const int mouse_event_flags) override;
  void OnAutoclickCanceled() override;

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override;

  bool enabled_;
  // The target window is observed by AutoclickControllerImpl for the duration
  // of a autoclick gesture.
  aura::Window* tap_down_target_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<AutoclickControllerCommon> autoclick_controller_common_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickControllerImpl);
};

AutoclickControllerImpl::AutoclickControllerImpl()
    : enabled_(false),
      tap_down_target_(nullptr),
      autoclick_controller_common_(
          new AutoclickControllerCommon(GetDefaultAutoclickDelay(), this)) {}

AutoclickControllerImpl::~AutoclickControllerImpl() {
  SetTapDownTarget(nullptr);
}

void AutoclickControllerImpl::SetTapDownTarget(aura::Window* target) {
  if (tap_down_target_ == target)
    return;

  if (tap_down_target_)
    tap_down_target_->RemoveObserver(this);
  tap_down_target_ = target;
  if (tap_down_target_)
    tap_down_target_->AddObserver(this);
}

void AutoclickControllerImpl::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;

  if (enabled_) {
    Shell::GetInstance()->AddPreTargetHandler(this);
    autoclick_controller_common_->CancelAutoclick();
  } else {
    Shell::GetInstance()->RemovePreTargetHandler(this);
  }
}

bool AutoclickControllerImpl::IsEnabled() const {
  return enabled_;
}

void AutoclickControllerImpl::SetAutoclickDelay(base::TimeDelta delay) {
  autoclick_controller_common_->SetAutoclickDelay(delay);
}

void AutoclickControllerImpl::OnMouseEvent(ui::MouseEvent* event) {
  autoclick_controller_common_->HandleMouseEvent(*event);
}

void AutoclickControllerImpl::OnKeyEvent(ui::KeyEvent* event) {
  autoclick_controller_common_->HandleKeyEvent(*event);
}

void AutoclickControllerImpl::OnTouchEvent(ui::TouchEvent* event) {
  autoclick_controller_common_->CancelAutoclick();
}

void AutoclickControllerImpl::OnGestureEvent(ui::GestureEvent* event) {
  autoclick_controller_common_->CancelAutoclick();
}

void AutoclickControllerImpl::OnScrollEvent(ui::ScrollEvent* event) {
  autoclick_controller_common_->CancelAutoclick();
}

views::Widget* AutoclickControllerImpl::CreateAutoclickRingWidget(
    const gfx::Point& event_location) {
  aura::Window* target =
      WmWindowAura::GetAuraWindow(ash::wm::GetRootWindowAt(event_location));
  SetTapDownTarget(target);
  aura::Window* root_window = target->GetRootWindow();
  widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = root_window;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);
  widget_->Init(params);
  widget_->SetOpacity(1.f);
  return widget_.get();
}

void AutoclickControllerImpl::UpdateAutoclickRingWidget(
    views::Widget* widget,
    const gfx::Point& event_location) {
  aura::Window* target =
      WmWindowAura::GetAuraWindow(ash::wm::GetRootWindowAt(event_location));
  SetTapDownTarget(target);
  aura::Window* root_window = target->GetRootWindow();
  if (widget->GetNativeView()->GetRootWindow() != root_window) {
    views::Widget::ReparentNativeView(
        widget->GetNativeView(),
        Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));
  }
}

void AutoclickControllerImpl::DoAutoclick(const gfx::Point& event_location,
                                          const int mouse_event_flags) {
  aura::Window* root_window =
      WmWindowAura::GetAuraWindow(wm::GetRootWindowAt(event_location));
  DCHECK(root_window) << "Root window not found while attempting autoclick.";

  gfx::Point click_location(event_location);
  ::wm::ConvertPointFromScreen(root_window, &click_location);
  aura::WindowTreeHost* host = root_window->GetHost();
  host->ConvertDIPToPixels(&click_location);

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, click_location,
                             click_location, ui::EventTimeForNow(),
                             mouse_event_flags | ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, click_location,
                               click_location, ui::EventTimeForNow(),
                               mouse_event_flags | ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);

  ui::EventDispatchDetails details =
      host->event_processor()->OnEventFromSource(&press_event);
  if (!details.dispatcher_destroyed)
    details = host->event_processor()->OnEventFromSource(&release_event);
  if (details.dispatcher_destroyed)
    return;
}

void AutoclickControllerImpl::OnAutoclickCanceled() {
  SetTapDownTarget(nullptr);
}

void AutoclickControllerImpl::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(tap_down_target_, window);
  autoclick_controller_common_->CancelAutoclick();
}

// static.
AutoclickController* AutoclickController::CreateInstance() {
  return new AutoclickControllerImpl();
}

}  // namespace ash
