// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/backdrop_controller.h"

#include "ash/accessibility_delegate.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_delegate.h"
#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "ui/app_list/app_list_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

class BackdropEventHandler : public ui::EventHandler {
 public:
  BackdropEventHandler() = default;
  ~BackdropEventHandler() override = default;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    // If the event is targeted at the backdrop, it means the user has made an
    // interaction that is outside the window's bounds and we want to capture
    // it (usually when in spoken feedback mode). Handle the event (to prevent
    // behind-windows from receiving it) and play an earcon to notify the user.
    if (event->IsLocatedEvent()) {
      switch (event->type()) {
        case ui::ET_MOUSE_PRESSED:
        case ui::ET_MOUSEWHEEL:
        case ui::ET_TOUCH_PRESSED:
        case ui::ET_POINTER_DOWN:
        case ui::ET_POINTER_WHEEL_CHANGED:
        case ui::ET_GESTURE_BEGIN:
        case ui::ET_SCROLL:
        case ui::ET_SCROLL_FLING_START:
          Shell::Get()->accessibility_delegate()->PlayEarcon(
              chromeos::SOUND_VOLUME_ADJUST);
          break;
        default:
          break;
      }
      event->SetHandled();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BackdropEventHandler);
};

}  // namespace

BackdropController::BackdropController(aura::Window* container)
    : container_(container), in_restacking_(false) {
  DCHECK(container_);
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->system_tray_notifier()->AddAccessibilityObserver(this);
}

BackdropController::~BackdropController() {
  Shell::Get()->system_tray_notifier()->RemoveAccessibilityObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  // TODO: animations won't work right with mus: http://crbug.com/548396.
  Hide();
}

void BackdropController::OnWindowAddedToLayout(aura::Window* child) {
  UpdateBackdrop();
}

void BackdropController::OnWindowRemovedFromLayout(aura::Window* child) {
  UpdateBackdrop();
}

void BackdropController::OnChildWindowVisibilityChanged(aura::Window* child,
                                                        bool visible) {
  UpdateBackdrop();
}

void BackdropController::OnWindowStackingChanged(aura::Window* window) {
  UpdateBackdrop();
}

void BackdropController::OnPostWindowStateTypeChange(
    wm::WindowState* window_state,
    wm::WindowStateType old_type) {
  UpdateBackdrop();
}

void BackdropController::SetBackdropDelegate(
    std::unique_ptr<BackdropDelegate> delegate) {
  delegate_ = std::move(delegate);
  UpdateBackdrop();
}

void BackdropController::UpdateBackdrop() {
  // Avoid recursive calls.
  if (in_restacking_ || force_hidden_counter_)
    return;

  aura::Window* window = GetTopmostWindowWithBackdrop();
  if (!window) {
    // Hide backdrop since no suitable window was found.
    Hide();
    return;
  }
  // We are changing the order of windows which will cause recursion.
  base::AutoReset<bool> lock(&in_restacking_, true);
  EnsureBackdropWidget();
  UpdateAccessibilityMode();

  if (window == backdrop_window_ && backdrop_->IsVisible())
    return;
  if (window->GetRootWindow() != backdrop_window_->GetRootWindow())
    return;

  if (!backdrop_->IsVisible())
    Show();

  // Since the backdrop needs to be immediately behind the window and the
  // stacking functions only guarantee a "it's above or below", we need
  // to re-arrange the two windows twice.
  container_->StackChildAbove(backdrop_window_, window);
  container_->StackChildAbove(window, backdrop_window_);
}

void BackdropController::OnOverviewModeStarting() {
  AddForceHidden();
}

void BackdropController::OnOverviewModeEnded() {
  RemoveForceHidden();
}

void BackdropController::OnSplitViewModeStarting() {
  AddForceHidden();
}

void BackdropController::OnSplitViewModeEnded() {
  RemoveForceHidden();
}

void BackdropController::OnAppListVisibilityChanged(bool shown,
                                                    aura::Window* root_window) {
  // Ignore the notification if it is not for this display.
  if (container_->GetRootWindow() != root_window)
    return;

  // Hide or update backdrop only for fullscreen app list.
  if (!app_list::features::IsFullscreenAppListEnabled())
    return;

  if (shown)
    AddForceHidden();
  else
    RemoveForceHidden();
}

void BackdropController::OnAccessibilityModeChanged(
    AccessibilityNotificationVisibility notify) {
  UpdateBackdrop();
}

void BackdropController::EnsureBackdropWidget() {
  if (backdrop_)
    return;

  backdrop_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = container_->GetBoundsInScreen();
  params.layer_type = ui::LAYER_SOLID_COLOR;
  params.name = "WorkspaceBackdropDelegate";
  // To disallow the MRU list from picking this window up it should not be
  // activateable.
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  DCHECK_NE(kShellWindowId_Invalid, container_->id());
  params.parent = container_;
  backdrop_->Init(params);
  backdrop_window_ = backdrop_->GetNativeWindow();
  backdrop_window_->SetName("Backdrop");
  ::wm::SetWindowVisibilityAnimationType(
      backdrop_window_, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  backdrop_window_->layer()->SetColor(SK_ColorBLACK);
}

void BackdropController::UpdateAccessibilityMode() {
  if (!backdrop_)
    return;

  bool enabled =
      Shell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled();
  if (enabled) {
    if (!backdrop_event_handler_) {
      backdrop_event_handler_ = base::MakeUnique<BackdropEventHandler>();
      original_event_handler_ =
          backdrop_window_->SetTargetHandler(backdrop_event_handler_.get());
    }
  } else if (backdrop_event_handler_) {
    backdrop_window_->SetTargetHandler(original_event_handler_);
    backdrop_event_handler_.reset();
  }
}

aura::Window* BackdropController::GetTopmostWindowWithBackdrop() {
  // ARC app should always have a backdrop when spoken feedback is enabled.
  if (Shell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled()) {
    aura::Window* active_window = wm::GetActiveWindow();
    if (active_window && active_window->parent() == container_ &&
        active_window->GetProperty(aura::client::kAppType) ==
            static_cast<int>(AppType::ARC_APP)) {
      return active_window;
    }
  }

  const aura::Window::Windows windows = container_->children();
  for (auto window_iter = windows.rbegin(); window_iter != windows.rend();
       ++window_iter) {
    aura::Window* window = *window_iter;
    if (window != backdrop_window_ && window->layer()->GetTargetVisibility() &&
        window->type() == aura::client::WINDOW_TYPE_NORMAL &&
        ::wm::CanActivateWindow(window) && WindowShouldHaveBackdrop(window)) {
      return window;
    }
  }
  return nullptr;
}

bool BackdropController::WindowShouldHaveBackdrop(aura::Window* window) {
  if (window->GetAllPropertyKeys().count(aura::client::kHasBackdrop) &&
      window->GetProperty(aura::client::kHasBackdrop)) {
    return true;
  }
  return delegate_ ? delegate_->HasBackdrop(window) : false;
}

void BackdropController::Show() {
  backdrop_->Show();
  backdrop_->SetFullscreen(true);
}

void BackdropController::Hide() {
  if (!backdrop_)
    return;
  backdrop_->Close();
  backdrop_ = nullptr;
  backdrop_window_ = nullptr;
  original_event_handler_ = nullptr;
  backdrop_event_handler_.reset();
}

void BackdropController::AddForceHidden() {
  force_hidden_counter_++;
  CHECK_GE(force_hidden_counter_, 0);
  if (force_hidden_counter_)
    Hide();
  else
    UpdateBackdrop();
}

void BackdropController::RemoveForceHidden() {
  force_hidden_counter_--;
  CHECK_GE(force_hidden_counter_, 0);
  if (force_hidden_counter_)
    Hide();
  else
    UpdateBackdrop();
}

}  // namespace ash
