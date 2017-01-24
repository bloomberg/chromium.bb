// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/test/ash_test_impl_mus.h"

#include "ash/common/test/ash_test.h"
#include "ash/common/wm_window.h"
#include "base/memory/ptr_util.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace mus {
namespace {

// WmTestBase is abstract as TestBody() is pure virtual (the various TEST
// macros have the implementation). In order to create WmTestBase we have to
// subclass with an empty implementation of TestBody(). That's ok as the class
// isn't used as a normal test here.
class WmTestBaseImpl : public WmTestBase {
 public:
  WmTestBaseImpl() {}
  ~WmTestBaseImpl() override {}

  // WmTestBase:
  void TestBody() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WmTestBaseImpl);
};

}  // namespace

AshTestImplMus::AshTestImplMus()
    : wm_test_base_(base::MakeUnique<WmTestBaseImpl>()) {}

AshTestImplMus::~AshTestImplMus() {}

void AshTestImplMus::SetUp() {
  wm_test_base_->SetUp();
}

void AshTestImplMus::TearDown() {
  wm_test_base_->TearDown();
}

void AshTestImplMus::UpdateDisplay(const std::string& display_spec) {
  wm_test_base_->UpdateDisplay(display_spec);
}

std::unique_ptr<WindowOwner> AshTestImplMus::CreateTestWindow(
    const gfx::Rect& bounds_in_screen,
    ui::wm::WindowType type,
    int shell_window_id) {
  WmWindow* window =
      WmWindow::Get(wm_test_base_->CreateTestWindow(bounds_in_screen, type));
  window->SetShellWindowId(shell_window_id);
  return base::MakeUnique<WindowOwner>(window);
}

std::unique_ptr<WindowOwner> AshTestImplMus::CreateToplevelTestWindow(
    const gfx::Rect& bounds_in_screen,
    int shell_window_id) {
  // For mus CreateTestWindow() creates top level windows (assuming
  // WINDOW_TYPE_NORMAL).
  return CreateTestWindow(bounds_in_screen, ui::wm::WINDOW_TYPE_NORMAL,
                          shell_window_id);
}

display::Display AshTestImplMus::GetSecondaryDisplay() {
  return wm_test_base_->GetSecondaryDisplay();
}

bool AshTestImplMus::SetSecondaryDisplayPlacement(
    display::DisplayPlacement::Position position,
    int offset) {
  NOTIMPLEMENTED();
  return false;
}

void AshTestImplMus::ConfigureWidgetInitParamsForDisplay(
    WmWindow* window,
    views::Widget::InitParams* init_params) {
  init_params->context = WmWindow::GetAuraWindow(window);
  init_params
      ->mus_properties[ui::mojom::WindowManager::kDisplayId_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          window->GetDisplayNearestWindow().id());
}

void AshTestImplMus::AddTransientChild(WmWindow* parent, WmWindow* window) {
  // TODO(sky): remove this as both classes can share same implementation now.
  ::wm::AddTransientChild(WmWindow::GetAuraWindow(parent),
                          WmWindow::GetAuraWindow(window));
}

}  // namespace mus

// static
std::unique_ptr<AshTestImpl> AshTestImpl::Create() {
  return base::MakeUnique<mus::AshTestImplMus>();
}

}  // namespace ash
