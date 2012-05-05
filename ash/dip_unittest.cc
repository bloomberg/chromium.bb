// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "ash/launcher/launcher.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/shadow.h"
#include "ash/wm/shadow_controller.h"
#include "ash/wm/shadow_types.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/monitor.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

#if defined(ENABLE_DIP)
#define MAYBE_Shadow FAILS_Shadow
#define MAYBE_WorkArea FAILS_WorkArea
#else
#define MAYBE_Shadow DISABLED_Shadow
#define MAYBE_WorkArea DISABLED_WorkArea
#endif

namespace ash {

typedef ash::test::AshTestBase DIPTest;

// Test if the shadow works correctly under different density
TEST_F(DIPTest, MAYBE_Shadow) {
  const gfx::Rect kBoundsInDIP(20, 30, 400, 300);

  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  window->SetParent(NULL);
  window->SetBounds(kBoundsInDIP);

  internal::ShadowController::TestApi api(
      Shell::GetInstance()->shadow_controller());
  const internal::Shadow* shadow = api.GetShadowForWindow(window.get());
  window->Show();

  const gfx::Rect layer_bounds_copy = shadow->layer()->bounds();
  window.reset();

  ChangeMonitorConfig(2.0f, gfx::Rect(0, 0, 1000, 1000));

  window.reset(new aura::Window(NULL));
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  window->SetParent(NULL);
  window->SetBounds(kBoundsInDIP);
  shadow = api.GetShadowForWindow(window.get());
  window->Show();
  EXPECT_EQ("40,60 800x600", window->GetBoundsInPixel().ToString());
  EXPECT_EQ(layer_bounds_copy.Scale(2.0f).ToString(),
            shadow->layer()->bounds().ToString());
}

// Test if the WM sets correct work area under different density.
TEST_F(DIPTest, MAYBE_WorkArea) {
  ChangeMonitorConfig(1.0f, gfx::Rect(0, 0, 1000, 900));

  aura::RootWindow* root = Shell::GetRootWindow();
  const gfx::Monitor monitor = gfx::Screen::GetMonitorNearestWindow(root);

  EXPECT_EQ("0,0 1000x900", monitor.bounds().ToString());
  const gfx::Rect work_area = monitor.work_area();
  const gfx::Insets insets = monitor.bounds().InsetsFrom(work_area);
  EXPECT_EQ("0,0,48,0", insets.ToString());

  ChangeMonitorConfig(2.0f, gfx::Rect(0, 0, 2000, 1800));

  const gfx::Monitor monitor_2x = gfx::Screen::GetMonitorNearestWindow(root);

  EXPECT_EQ("0,0 2000x1800", monitor_2x.bounds_in_pixel().ToString());
  EXPECT_EQ("0,0 1000x900", monitor_2x.bounds().ToString());

  EXPECT_EQ(work_area.ToString(), monitor_2x.work_area().ToString());

  Launcher* launcher = Shell::GetInstance()->launcher();
  EXPECT_EQ(
      monitor_2x.bounds().InsetsFrom(work_area).height() * 2,
      launcher->widget()->GetNativeView()->layer()->bounds().height());
}

}  // namespace ash
