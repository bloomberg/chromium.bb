// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/top_level_window_factory.h"

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "ash/common/test/ash_test.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/mus/test/wm_test_base.h"
#include "ash/mus/window_manager.h"
#include "ash/mus/window_manager_application.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

int64_t GetDisplayId(aura::Window* window) {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
}

}  // namespace

using TopLevelWindowFactoryTest = AshTest;

TEST_F(TopLevelWindowFactoryTest, CreateFullscreenWindow) {
  std::unique_ptr<WindowOwner> window_owner = CreateToplevelTestWindow();
  WmWindow* window = window_owner->window();
  window->SetFullscreen(true);
  WmWindow* root_window = WmShell::Get()->GetPrimaryRootWindow();
  EXPECT_EQ(root_window->GetBounds(), window->GetBounds());
}

using TopLevelWindowFactoryWmTest = mus::WmTestBase;

TEST_F(TopLevelWindowFactoryWmTest, IsWindowShownInCorrectDisplay) {
  UpdateDisplay("400x400,400x400");
  EXPECT_NE(GetPrimaryDisplay().id(), GetSecondaryDisplay().id());

  std::unique_ptr<aura::Window> window_primary_display(
      CreateFullscreenTestWindow(GetPrimaryDisplay().id()));
  std::unique_ptr<aura::Window> window_secondary_display(
      CreateFullscreenTestWindow(GetSecondaryDisplay().id()));

  EXPECT_EQ(GetPrimaryDisplay().id(),
            GetDisplayId(window_primary_display.get()));
  EXPECT_EQ(GetSecondaryDisplay().id(),
            GetDisplayId(window_secondary_display.get()));
}

using TopLevelWindowFactoryAshTest = test::AshTestBase;

TEST_F(TopLevelWindowFactoryAshTest, TopLevelNotShownOnCreate) {
  std::map<std::string, std::vector<uint8_t>> properties;
  std::unique_ptr<aura::Window> window(mus::CreateAndParentTopLevelWindow(
      ash_test_helper()->window_manager_app()->window_manager(),
      ui::mojom::WindowType::WINDOW, &properties));
  ASSERT_TRUE(window);
  EXPECT_FALSE(window->IsVisible());
}

}  // namespace ash
