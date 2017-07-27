// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/first_run/first_run_helper_impl.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "base/logging.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

views::Widget* CreateFirstRunWindow() {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                      ash::kShellWindowId_OverlayContainer);
  views::Widget* window = new views::Widget;
  window->Init(params);
  return window;
}

}  // anonymous namespace

FirstRunHelperImpl::FirstRunHelperImpl() : widget_(CreateFirstRunWindow()) {
  Shell::Get()->overlay_filter()->Activate(this);
}

FirstRunHelperImpl::~FirstRunHelperImpl() {
  Shell::Get()->overlay_filter()->Deactivate(this);
  if (IsTrayBubbleOpened())
    CloseTrayBubble();
  widget_->Close();
}

views::Widget* FirstRunHelperImpl::GetOverlayWidget() {
  return widget_;
}

gfx::Rect FirstRunHelperImpl::GetAppListButtonBounds() {
  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  AppListButton* app_button = shelf->shelf_widget()->GetAppListButton();
  return app_button->GetBoundsInScreen();
}

void FirstRunHelperImpl::Cancel() {
  for (auto& observer : observers())
    observer.OnCancelled();
}

bool FirstRunHelperImpl::IsCancelingKeyEvent(ui::KeyEvent* event) {
  return event->key_code() == ui::VKEY_ESCAPE;
}

aura::Window* FirstRunHelperImpl::GetWindow() {
  return widget_->GetNativeWindow();
}

void FirstRunHelperImpl::OpenTrayBubble() {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();
  tray->ShowPersistentDefaultView();
}

void FirstRunHelperImpl::CloseTrayBubble() {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();
  DCHECK(tray->HasSystemBubble()) << "Tray bubble is closed already.";
  tray->CloseBubble();
}

bool FirstRunHelperImpl::IsTrayBubbleOpened() {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();
  return tray->HasSystemBubble();
}

gfx::Rect FirstRunHelperImpl::GetTrayBubbleBounds() {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();
  views::View* bubble = tray->GetSystemBubble()->bubble_view();
  return bubble->GetBoundsInScreen();
}

gfx::Rect FirstRunHelperImpl::GetHelpButtonBounds() {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();
  views::View* help_button = tray->GetHelpButtonView();
  return help_button->GetBoundsInScreen();
}

}  // namespace ash
