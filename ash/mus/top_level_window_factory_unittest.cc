// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/ash_test.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/mus/test/wm_test_base.h"
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
  window->SetFullscreen();
  WmWindow* root_window = WmShell::Get()->GetPrimaryRootWindow();
  EXPECT_EQ(root_window->GetBounds(), window->GetBounds());
}

using TopLevelWindowFactoryWmTest = mus::WmTestBase;

TEST_F(TopLevelWindowFactoryWmTest, IsWindowShownInCorrectDisplay) {
  if (!SupportsMultipleDisplays())
    return;

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

}  // namespace ash
