// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/first_run/first_run_helper.h"

#include "ash/first_run/desktop_cleaner.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_bubble.h"
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

}  // namespace

FirstRunHelper::FirstRunHelper() = default;

FirstRunHelper::~FirstRunHelper() = default;

void FirstRunHelper::BindRequest(mojom::FirstRunHelperRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void FirstRunHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FirstRunHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

views::Widget* FirstRunHelper::GetOverlayWidget() {
  return widget_;
}

void FirstRunHelper::CreateOverlayWidget() {
  widget_ = CreateFirstRunWindow();
  cleaner_ = std::make_unique<DesktopCleaner>();
  Shell::Get()->overlay_filter()->Activate(this);
}

void FirstRunHelper::CloseOverlayWidget() {
  Shell::Get()->overlay_filter()->Deactivate(this);
  // Ensure the tray is closed.
  Shell::Get()->GetPrimarySystemTray()->CloseBubble();
  widget_->Close();
  widget_ = nullptr;
  cleaner_.reset();
}

void FirstRunHelper::GetAppListButtonBounds(GetAppListButtonBoundsCallback cb) {
  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  AppListButton* app_button = shelf->shelf_widget()->GetAppListButton();
  std::move(cb).Run(app_button->GetBoundsInScreen());
}

void FirstRunHelper::OpenTrayBubble(OpenTrayBubbleCallback cb) {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();
  tray->ShowPersistentDefaultView();
  views::View* bubble = tray->GetSystemBubble()->bubble_view();
  std::move(cb).Run(bubble->GetBoundsInScreen());
}

void FirstRunHelper::CloseTrayBubble() {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();
  tray->CloseBubble();
}

void FirstRunHelper::GetHelpButtonBounds(GetHelpButtonBoundsCallback cb) {
  SystemTray* tray = Shell::Get()->GetPrimarySystemTray();
  views::View* help_button = tray->GetHelpButtonView();
  // |help_button| could be null if the tray isn't open.
  if (!help_button) {
    std::move(cb).Run(gfx::Rect());
    return;
  }
  std::move(cb).Run(help_button->GetBoundsInScreen());
}

// OverlayEventFilter::Delegate:

void FirstRunHelper::Cancel() {
  for (auto& observer : observers_)
    observer.OnCancelled();
}

bool FirstRunHelper::IsCancelingKeyEvent(ui::KeyEvent* event) {
  return event->key_code() == ui::VKEY_ESCAPE;
}

aura::Window* FirstRunHelper::GetWindow() {
  return widget_->GetNativeWindow();
}

}  // namespace ash
