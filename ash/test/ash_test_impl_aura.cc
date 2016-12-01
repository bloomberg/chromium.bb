// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_impl_aura.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/test/ash_test.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// AshTestBase is abstract as TestBody() is pure virtual (the various TEST
// macros have the implementation). In order to create AshTestBase we have to
// subclass with an empty implementation of TestBody(). That's ok as the class
// isn't used as a normal test here.
class AshTestBaseImpl : public test::AshTestBase {
 public:
  AshTestBaseImpl() {}
  ~AshTestBaseImpl() override {}

  // AshTestBase:
  void TestBody() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AshTestBaseImpl);
};

}  // namespace

AshTestImplAura::AshTestImplAura()
    : ash_test_base_(base::MakeUnique<AshTestBaseImpl>()) {}

AshTestImplAura::~AshTestImplAura() {}

void AshTestImplAura::SetUp() {
  ash_test_base_->SetUp();
}

void AshTestImplAura::TearDown() {
  ash_test_base_->TearDown();
}

bool AshTestImplAura::SupportsMultipleDisplays() const {
  return ash_test_base_->SupportsMultipleDisplays();
}

void AshTestImplAura::UpdateDisplay(const std::string& display_spec) {
  ash_test_base_->UpdateDisplay(display_spec);
}

std::unique_ptr<WindowOwner> AshTestImplAura::CreateTestWindow(
    const gfx::Rect& bounds_in_screen,
    ui::wm::WindowType type,
    int shell_window_id) {
  return base::MakeUnique<WindowOwner>(WmWindowAura::Get(
      ash_test_base_->CreateTestWindowInShellWithDelegateAndType(
          nullptr, type, shell_window_id, bounds_in_screen)));
}

std::unique_ptr<WindowOwner> AshTestImplAura::CreateToplevelTestWindow(
    const gfx::Rect& bounds_in_screen,
    int shell_window_id) {
  aura::test::TestWindowDelegate* delegate =
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
  return base::MakeUnique<WindowOwner>(WmWindowAura::Get(
      ash_test_base_->CreateTestWindowInShellWithDelegateAndType(
          delegate, ui::wm::WINDOW_TYPE_NORMAL, shell_window_id,
          bounds_in_screen)));
}

display::Display AshTestImplAura::GetSecondaryDisplay() {
  return Shell::GetInstance()->display_manager()->GetSecondaryDisplay();
}

bool AshTestImplAura::SetSecondaryDisplayPlacement(
    display::DisplayPlacement::Position position,
    int offset) {
  Shell::GetInstance()->display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(
          Shell::GetInstance()->display_manager(), position, 0));
  return true;
}

void AshTestImplAura::ConfigureWidgetInitParamsForDisplay(
    WmWindow* window,
    views::Widget::InitParams* init_params) {
  init_params->context = WmWindowAura::GetAuraWindow(window);
}

void AshTestImplAura::AddTransientChild(WmWindow* parent, WmWindow* window) {
  ::wm::AddTransientChild(WmWindowAura::GetAuraWindow(parent),
                          WmWindowAura::GetAuraWindow(window));
}

// static
std::unique_ptr<AshTestImpl> AshTestImpl::Create() {
  return base::MakeUnique<AshTestImplAura>();
}

}  // namespace ash
