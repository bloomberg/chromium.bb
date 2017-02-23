// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/ash_test.h"

#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/test/ash_test_impl.h"
#include "ash/common/test/test_session_state_delegate.h"
#include "ash/common/test/test_system_tray_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/root_window_controller.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/display.h"

namespace ash {

WindowOwner::WindowOwner(WmWindow* window) : window_(window) {}

WindowOwner::~WindowOwner() {
  window_->Destroy();
}

AshTest::AshTest() : test_impl_(AshTestImpl::Create()) {}

AshTest::~AshTest() {}

// static
WmShelf* AshTest::GetPrimaryShelf() {
  return WmShell::Get()
      ->GetPrimaryRootWindow()
      ->GetRootWindowController()
      ->GetShelf();
}

// static
SystemTray* AshTest::GetPrimarySystemTray() {
  return GetPrimaryShelf()->GetStatusAreaWidget()->system_tray();
}

// static
test::TestSystemTrayDelegate* AshTest::GetSystemTrayDelegate() {
  return static_cast<test::TestSystemTrayDelegate*>(
      WmShell::Get()->system_tray_delegate());
}

void AshTest::UpdateDisplay(const std::string& display_spec) {
  return test_impl_->UpdateDisplay(display_spec);
}

std::unique_ptr<WindowOwner> AshTest::CreateTestWindow(const gfx::Rect& bounds,
                                                       ui::wm::WindowType type,
                                                       int shell_window_id) {
  return test_impl_->CreateTestWindow(bounds, type, shell_window_id);
}

std::unique_ptr<WindowOwner> AshTest::CreateToplevelTestWindow(
    const gfx::Rect& bounds_in_screen,
    int shell_window_id) {
  return test_impl_->CreateToplevelTestWindow(bounds_in_screen,
                                              shell_window_id);
}

std::unique_ptr<WindowOwner> AshTest::CreateChildWindow(WmWindow* parent,
                                                        const gfx::Rect& bounds,
                                                        int shell_window_id) {
  std::unique_ptr<WindowOwner> window_owner =
      base::MakeUnique<WindowOwner>(WmShell::Get()->NewWindow(
          ui::wm::WINDOW_TYPE_NORMAL, ui::LAYER_NOT_DRAWN));
  window_owner->window()->SetBounds(bounds);
  window_owner->window()->SetShellWindowId(shell_window_id);
  parent->AddChild(window_owner->window());
  window_owner->window()->Show();
  return window_owner;
}

// static
std::unique_ptr<views::Widget> AshTest::CreateTestWidget(
    const gfx::Rect& bounds,
    views::WidgetDelegate* delegate,
    int container_id) {
  std::unique_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params;
  params.delegate = delegate;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = bounds;
  WmShell::Get()
      ->GetPrimaryRootWindow()
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(widget.get(), container_id,
                                              &params);
  widget->Init(params);
  widget->Show();
  return widget;
}

display::Display AshTest::GetSecondaryDisplay() {
  return test_impl_->GetSecondaryDisplay();
}

bool AshTest::SetSecondaryDisplayPlacement(
    display::DisplayPlacement::Position position,
    int offset) {
  if (WmShell::Get()->IsRunningInMash()) {
    NOTIMPLEMENTED();
    return false;
  }
  return test_impl_->SetSecondaryDisplayPlacement(position, offset);
}

void AshTest::ConfigureWidgetInitParamsForDisplay(
    WmWindow* window,
    views::Widget::InitParams* init_params) {
  test_impl_->ConfigureWidgetInitParamsForDisplay(window, init_params);
}

void AshTest::ParentWindowInPrimaryRootWindow(WmWindow* window) {
  window->SetParentUsingContext(WmShell::Get()->GetPrimaryRootWindow(),
                                gfx::Rect());
}

void AshTest::AddTransientChild(WmWindow* parent, WmWindow* window) {
  test_impl_->AddTransientChild(parent, window);
}

void AshTest::RunAllPendingInMessageLoop() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void AshTest::SetUp() {
  test_impl_->SetUp();
}

void AshTest::TearDown() {
  test_impl_->TearDown();
}

}  // namespace ash
