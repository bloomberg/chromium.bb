// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/test/wm_test_base.h"

#include <algorithm>
#include <vector>

#include "ash/mus/bridge/wm_window_mus_test_api.h"
#include "ash/mus/root_window_controller.h"
#include "ash/mus/test/wm_test_helper.h"
#include "ash/mus/window_manager.h"
#include "ash/mus/window_manager_application.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window_tree_client.h"

namespace ash {
namespace mus {
namespace {

ui::mojom::WindowType MusWindowTypeFromWmWindowType(
    ui::wm::WindowType wm_window_type) {
  switch (wm_window_type) {
    case ui::wm::WINDOW_TYPE_UNKNOWN:
      break;

    case ui::wm::WINDOW_TYPE_NORMAL:
      return ui::mojom::WindowType::WINDOW;

    case ui::wm::WINDOW_TYPE_POPUP:
      return ui::mojom::WindowType::POPUP;

    case ui::wm::WINDOW_TYPE_CONTROL:
      return ui::mojom::WindowType::CONTROL;

    case ui::wm::WINDOW_TYPE_PANEL:
      return ui::mojom::WindowType::PANEL;

    case ui::wm::WINDOW_TYPE_MENU:
      return ui::mojom::WindowType::MENU;

    case ui::wm::WINDOW_TYPE_TOOLTIP:
      return ui::mojom::WindowType::TOOLTIP;
  }

  NOTREACHED();
  return ui::mojom::WindowType::CONTROL;
}

}  // namespace

WmTestBase::WmTestBase() {}

WmTestBase::~WmTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called WmTestBase::SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called WmTestBase::TearDown";
}

bool WmTestBase::SupportsMultipleDisplays() const {
  return true;
}

void WmTestBase::UpdateDisplay(const std::string& display_spec) {
  test_helper_->UpdateDisplay(display_spec);
}

ui::Window* WmTestBase::GetPrimaryRootWindow() {
  std::vector<RootWindowController*> roots =
      test_helper_->GetRootsOrderedByDisplayId();
  DCHECK(!roots.empty());
  return roots[0]->root();
}

ui::Window* WmTestBase::GetSecondaryRootWindow() {
  std::vector<RootWindowController*> roots =
      test_helper_->GetRootsOrderedByDisplayId();
  return roots.size() < 2 ? nullptr : roots[1]->root();
}

display::Display WmTestBase::GetPrimaryDisplay() {
  std::vector<RootWindowController*> roots =
      test_helper_->GetRootsOrderedByDisplayId();
  DCHECK(!roots.empty());
  return roots[0]->display();
}

display::Display WmTestBase::GetSecondaryDisplay() {
  std::vector<RootWindowController*> roots =
      test_helper_->GetRootsOrderedByDisplayId();
  return roots.size() < 2 ? display::Display() : roots[1]->display();
}

ui::Window* WmTestBase::CreateTestWindow(const gfx::Rect& bounds) {
  return CreateTestWindow(bounds, ui::wm::WINDOW_TYPE_NORMAL);
}

ui::Window* WmTestBase::CreateTestWindow(const gfx::Rect& bounds,
                                         ui::wm::WindowType window_type) {
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[ui::mojom::WindowManager::kWindowType_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(MusWindowTypeFromWmWindowType(window_type)));
  if (!bounds.IsEmpty()) {
    properties[ui::mojom::WindowManager::kInitialBounds_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(bounds);
  }
  properties[ui::mojom::WindowManager::kResizeBehavior_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          ui::mojom::kResizeBehaviorCanResize |
          ui::mojom::kResizeBehaviorCanMaximize |
          ui::mojom::kResizeBehaviorCanMinimize);

  ui::Window* window = test_helper_->GetRootsOrderedByDisplayId()[0]
                           ->window_manager()
                           ->NewTopLevelWindow(&properties);
  window->SetVisible(true);
  // Most tests expect a minimum size of 0x0.
  WmWindowMusTestApi(WmWindowMus::Get(window)).set_use_empty_minimum_size(true);
  return window;
}

ui::Window* WmTestBase::CreateFullscreenTestWindow(int64_t display_id) {
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[ui::mojom::WindowManager::kShowState_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ui::mojom::ShowState::FULLSCREEN));

  if (display_id != display::kInvalidDisplayId) {
    properties[ui::mojom::WindowManager::kInitialDisplayId_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(display_id);
  }

  ui::Window* window = test_helper_->GetRootsOrderedByDisplayId()[0]
                           ->window_manager()
                           ->NewTopLevelWindow(&properties);
  window->SetVisible(true);
  return window;
}

ui::Window* WmTestBase::CreateChildTestWindow(ui::Window* parent,
                                              const gfx::Rect& bounds) {
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[ui::mojom::WindowManager::kWindowType_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(static_cast<int32_t>(
          MusWindowTypeFromWmWindowType(ui::wm::WINDOW_TYPE_NORMAL)));
  ui::Window* window = test_helper_->GetRootsOrderedByDisplayId()[0]
                           ->root()
                           ->window_tree()
                           ->NewWindow(&properties);
  window->SetBounds(bounds);
  window->SetVisible(true);
  parent->AddChild(window);
  return window;
}

void WmTestBase::SetUp() {
  setup_called_ = true;
  test_helper_.reset(new WmTestHelper);
  test_helper_->Init();
}

void WmTestBase::TearDown() {
  teardown_called_ = true;
  test_helper_.reset();
}

}  // namespace mus
}  // namespace ash
