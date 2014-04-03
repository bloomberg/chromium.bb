// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/first_run/first_run_helper_impl.h"

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray.h"
#include "base/logging.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/window.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

views::Widget* CreateFirstRunWindow() {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = Shell::GetScreen()->GetPrimaryDisplay().bounds();
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                      ash::kShellWindowId_OverlayContainer);
  views::Widget* window = new views::Widget;
  window->Init(params);
  return window;
}

}  // anonymous namespace

FirstRunHelperImpl::FirstRunHelperImpl()
    : widget_(CreateFirstRunWindow()) {
  Shell::GetInstance()->overlay_filter()->Activate(this);
}

FirstRunHelperImpl::~FirstRunHelperImpl() {
  Shell::GetInstance()->overlay_filter()->Deactivate();
  if (IsTrayBubbleOpened())
    CloseTrayBubble();
  widget_->Close();
}

views::Widget* FirstRunHelperImpl::GetOverlayWidget() {
  return widget_;
}

void FirstRunHelperImpl::OpenAppList() {
  if (Shell::GetInstance()->GetAppListTargetVisibility())
    return;
  Shell::GetInstance()->ToggleAppList(NULL);
}

void FirstRunHelperImpl::CloseAppList() {
  if (!Shell::GetInstance()->GetAppListTargetVisibility())
    return;
  Shell::GetInstance()->ToggleAppList(NULL);
}

gfx::Rect FirstRunHelperImpl::GetLauncherBounds() {
  Shelf* shelf = Shelf::ForPrimaryDisplay();
  return shelf->GetVisibleItemsBoundsInScreen();
}

gfx::Rect FirstRunHelperImpl::GetAppListButtonBounds() {
  Shelf* shelf = Shelf::ForPrimaryDisplay();
  views::View* app_button = shelf->GetAppListButtonView();
  return app_button->GetBoundsInScreen();
}

gfx::Rect FirstRunHelperImpl::GetAppListBounds() {
  app_list::AppListView* view = Shell::GetInstance()->GetAppListView();
  return view->GetBoundsInScreen();
}

void FirstRunHelperImpl::Cancel() {
  FOR_EACH_OBSERVER(Observer, observers(), OnCancelled());
}

bool FirstRunHelperImpl::IsCancelingKeyEvent(ui::KeyEvent* event) {
  return event->key_code() == ui::VKEY_ESCAPE;
}

aura::Window* FirstRunHelperImpl::GetWindow() {
  return widget_->GetNativeWindow();
}

void FirstRunHelperImpl::OpenTrayBubble() {
  SystemTray* tray = Shell::GetInstance()->GetPrimarySystemTray();
  tray->ShowPersistentDefaultView();
}

void FirstRunHelperImpl::CloseTrayBubble() {
  SystemTray* tray = Shell::GetInstance()->GetPrimarySystemTray();
  DCHECK(tray->HasSystemBubble()) << "Tray bubble is closed already.";
  tray->CloseSystemBubble();
}

bool FirstRunHelperImpl::IsTrayBubbleOpened() {
  SystemTray* tray = Shell::GetInstance()->GetPrimarySystemTray();
  return tray->HasSystemBubble();
}

gfx::Rect FirstRunHelperImpl::GetTrayBubbleBounds() {
  SystemTray* tray = Shell::GetInstance()->GetPrimarySystemTray();
  views::View* bubble = tray->GetSystemBubble()->bubble_view();
  return bubble->GetBoundsInScreen();
}

gfx::Rect FirstRunHelperImpl::GetHelpButtonBounds() {
  SystemTray* tray = Shell::GetInstance()->GetPrimarySystemTray();
  views::View* help_button = tray->GetHelpButtonView();
  return help_button->GetBoundsInScreen();
}

}  // namespace ash
