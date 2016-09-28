// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/ash_test.h"

#include "ash/common/test/ash_test_impl.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/display.h"

namespace ash {

WindowOwner::WindowOwner(WmWindow* window) : window_(window) {}

WindowOwner::~WindowOwner() {
  window_->Destroy();
}

AshTest::AshTest() : test_impl_(AshTestImpl::Create()) {}

AshTest::~AshTest() {}

bool AshTest::SupportsMultipleDisplays() const {
  return test_impl_->SupportsMultipleDisplays();
}

void AshTest::UpdateDisplay(const std::string& display_spec) {
  return test_impl_->UpdateDisplay(display_spec);
}

std::unique_ptr<WindowOwner> AshTest::CreateTestWindow(const gfx::Rect& bounds,
                                                       ui::wm::WindowType type,
                                                       int shell_window_id) {
  return test_impl_->CreateTestWindow(bounds, type, shell_window_id);
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

display::Display AshTest::GetSecondaryDisplay() {
  return test_impl_->GetSecondaryDisplay();
}

bool AshTest::SetSecondaryDisplayPlacement(
    display::DisplayPlacement::Position position,
    int offset) {
  return test_impl_->SetSecondaryDisplayPlacement(position, offset);
}

void AshTest::SetUp() {
  test_impl_->SetUp();
}

void AshTest::TearDown() {
  test_impl_->TearDown();
}

}  // namespace ash
