// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_observer_hud.h"

#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget.h"

namespace ash {

TouchObserverHUD::TouchObserverHUD(aura::Window* initial_root)
    : display_id_(GetRootWindowSettings(initial_root)->display_id),
      root_window_(initial_root),
      widget_(NULL) {
  const gfx::Display& display =
      Shell::GetInstance()->display_manager()->GetDisplayForId(display_id_);

  views::View* content = new views::View;

  const gfx::Size& display_size = display.size();
  content->SetSize(display_size);

  widget_ = new views::Widget();
  views::Widget::InitParams
      params(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.accept_events = false;
  params.bounds = display.bounds();
  params.parent =
      Shell::GetContainer(root_window_, kShellWindowId_OverlayContainer);
  widget_->Init(params);
  widget_->SetContentsView(content);
  widget_->StackAtTop();
  widget_->Show();

  widget_->AddObserver(this);

  // Observe changes in display size and mode to update touch HUD.
  Shell::GetScreen()->AddObserver(this);
#if defined(OS_CHROMEOS)
  Shell::GetInstance()->display_configurator()->AddObserver(this);
#endif  // defined(OS_CHROMEOS)

  Shell::GetInstance()->display_controller()->AddObserver(this);
  root_window_->AddPreTargetHandler(this);
}

TouchObserverHUD::~TouchObserverHUD() {
  Shell::GetInstance()->display_controller()->RemoveObserver(this);

#if defined(OS_CHROMEOS)
  Shell::GetInstance()->display_configurator()->RemoveObserver(this);
#endif  // defined(OS_CHROMEOS)
  Shell::GetScreen()->RemoveObserver(this);

  widget_->RemoveObserver(this);
}

void TouchObserverHUD::Clear() {
}

void TouchObserverHUD::Remove() {
  root_window_->RemovePreTargetHandler(this);

  RootWindowController* controller = GetRootWindowController(root_window_);
  UnsetHudForRootWindowController(controller);

  widget_->CloseNow();
}

void TouchObserverHUD::OnTouchEvent(ui::TouchEvent* /*event*/) {
}

void TouchObserverHUD::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, widget_);
  delete this;
}

void TouchObserverHUD::OnDisplayAdded(const gfx::Display& new_display) {}

void TouchObserverHUD::OnDisplayRemoved(const gfx::Display& old_display) {
  if (old_display.id() != display_id_)
    return;
  widget_->CloseNow();
}

void TouchObserverHUD::OnDisplayMetricsChanged(const gfx::Display& display,
                                               uint32_t metrics) {
  if (display.id() != display_id_ || !(metrics & DISPLAY_METRIC_BOUNDS))
    return;

  widget_->SetSize(display.size());
}

#if defined(OS_CHROMEOS)
void TouchObserverHUD::OnDisplayModeChanged(
    const ui::DisplayConfigurator::DisplayStateList& outputs) {
  // Clear touch HUD for any change in display mode (single, dual extended, dual
  // mirrored, ...).
  Clear();
}
#endif  // defined(OS_CHROMEOS)

void TouchObserverHUD::OnDisplaysInitialized() {
  OnDisplayConfigurationChanged();
}

void TouchObserverHUD::OnDisplayConfigurationChanging() {
  if (!root_window_)
    return;

  root_window_->RemovePreTargetHandler(this);

  RootWindowController* controller = GetRootWindowController(root_window_);
  UnsetHudForRootWindowController(controller);

  views::Widget::ReparentNativeView(
      widget_->GetNativeView(),
      Shell::GetContainer(root_window_,
                          kShellWindowId_UnparentedControlContainer));

  root_window_ = NULL;
}

void TouchObserverHUD::OnDisplayConfigurationChanged() {
  if (root_window_)
    return;

  root_window_ = Shell::GetInstance()->display_controller()->
      GetRootWindowForDisplayId(display_id_);

  views::Widget::ReparentNativeView(
      widget_->GetNativeView(),
      Shell::GetContainer(root_window_, kShellWindowId_OverlayContainer));

  RootWindowController* controller = GetRootWindowController(root_window_);
  SetHudForRootWindowController(controller);

  root_window_->AddPreTargetHandler(this);
}

}  // namespace ash
