// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/shadow_controller.h"

#include <algorithm>
#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/shadow.h"
#include "ash/wm/shadow_types.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"

namespace ash {
namespace internal {

typedef ash::test::AshTestBase ShadowControllerTest;

// Tests that various methods in Window update the Shadow object as expected.
TEST_F(ShadowControllerTest, Shadow) {
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  window->SetParent(NULL);

  // We should create the shadow before the window is visible (the shadow's
  // layer won't get drawn yet since it's a child of the window's layer).
  internal::ShadowController::TestApi api(
      ash::Shell::GetInstance()->shadow_controller());
  const internal::Shadow* shadow = api.GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);
  EXPECT_TRUE(shadow->layer()->visible());

  // The shadow should remain visible after window visibility changes.
  window->Show();
  EXPECT_TRUE(shadow->layer()->visible());
  window->Hide();
  EXPECT_TRUE(shadow->layer()->visible());

  // If the shadow is disabled, it should be hidden.
  internal::SetShadowType(window.get(), internal::SHADOW_TYPE_NONE);
  window->Show();
  EXPECT_FALSE(shadow->layer()->visible());
  internal::SetShadowType(window.get(), internal::SHADOW_TYPE_RECTANGULAR);
  EXPECT_TRUE(shadow->layer()->visible());

  // The shadow's layer should be a child of the window's layer.
  EXPECT_EQ(window->layer(), shadow->layer()->parent());

  window->parent()->RemoveChild(window.get());
  aura::Window* window_ptr = window.get();
  window.reset();
  EXPECT_TRUE(api.GetShadowForWindow(window_ptr) == NULL);
}

// Tests that the window's shadow's bounds are updated correctly.
TEST_F(ShadowControllerTest, ShadowBounds) {
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  window->SetParent(NULL);
  window->Show();

  const gfx::Rect kOldBounds(20, 30, 400, 300);
  window->SetBounds(kOldBounds);

  // When the shadow is first created, it should use the window's size (but
  // remain at the origin, since it's a child of the window's layer).
  internal::SetShadowType(window.get(), internal::SHADOW_TYPE_RECTANGULAR);
  internal::ShadowController::TestApi api(
      ash::Shell::GetInstance()->shadow_controller());
  const internal::Shadow* shadow = api.GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);
  EXPECT_EQ(gfx::Rect(kOldBounds.size()).ToString(),
            shadow->content_bounds().ToString());

  // When we change the window's bounds, the shadow's should be updated too.
  gfx::Rect kNewBounds(50, 60, 500, 400);
  window->SetBounds(kNewBounds);
  EXPECT_EQ(gfx::Rect(kNewBounds.size()).ToString(),
            shadow->content_bounds().ToString());
}

// Tests that activating a window changes the shadow style.
TEST_F(ShadowControllerTest, ShadowStyle) {
  ShadowController::TestApi api(
      ash::Shell::GetInstance()->shadow_controller());

  scoped_ptr<aura::Window> window1(new aura::Window(NULL));
  window1->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window1->Init(ui::LAYER_TEXTURED);
  window1->SetParent(NULL);
  window1->SetBounds(gfx::Rect(10, 20, 300, 400));
  window1->Show();
  wm::ActivateWindow(window1.get());

  // window1 is active, so style should have active appearance.
  Shadow* shadow1 = api.GetShadowForWindow(window1.get());
  ASSERT_TRUE(shadow1 != NULL);
  EXPECT_EQ(Shadow::STYLE_ACTIVE, shadow1->style());

  // Create another window and activate it.
  scoped_ptr<aura::Window> window2(new aura::Window(NULL));
  window2->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window2->Init(ui::LAYER_TEXTURED);
  window2->SetParent(NULL);
  window2->SetBounds(gfx::Rect(11, 21, 301, 401));
  window2->Show();
  wm::ActivateWindow(window2.get());

  // window1 is now inactive, so shadow should go inactive.
  Shadow* shadow2 = api.GetShadowForWindow(window2.get());
  ASSERT_TRUE(shadow2 != NULL);
  EXPECT_EQ(Shadow::STYLE_INACTIVE, shadow1->style());
  EXPECT_EQ(Shadow::STYLE_ACTIVE, shadow2->style());
}

// Tests that we use smaller shadows for tooltips and menus.
TEST_F(ShadowControllerTest, SmallShadowsForTooltipsAndMenus) {
  ShadowController::TestApi api(
      ash::Shell::GetInstance()->shadow_controller());

  scoped_ptr<aura::Window> tooltip_window(new aura::Window(NULL));
  tooltip_window->SetType(aura::client::WINDOW_TYPE_TOOLTIP);
  tooltip_window->Init(ui::LAYER_TEXTURED);
  tooltip_window->SetParent(NULL);
  tooltip_window->SetBounds(gfx::Rect(10, 20, 300, 400));
  tooltip_window->Show();

  Shadow* tooltip_shadow = api.GetShadowForWindow(tooltip_window.get());
  ASSERT_TRUE(tooltip_shadow != NULL);
  EXPECT_EQ(Shadow::STYLE_SMALL, tooltip_shadow->style());

  scoped_ptr<aura::Window> menu_window(new aura::Window(NULL));
  menu_window->SetType(aura::client::WINDOW_TYPE_MENU);
  menu_window->Init(ui::LAYER_TEXTURED);
  menu_window->SetParent(NULL);
  menu_window->SetBounds(gfx::Rect(10, 20, 300, 400));
  menu_window->Show();

  Shadow* menu_shadow = api.GetShadowForWindow(tooltip_window.get());
  ASSERT_TRUE(menu_shadow != NULL);
  EXPECT_EQ(Shadow::STYLE_SMALL, menu_shadow->style());
}

// http://crbug.com/120210 - transient parents of certain types of transients
// should not lose their shadow when they lose activation to the transient.
TEST_F(ShadowControllerTest, TransientParentKeepsActiveShadow) {
  ShadowController::TestApi api(
      ash::Shell::GetInstance()->shadow_controller());

  scoped_ptr<aura::Window> window1(new aura::Window(NULL));
  window1->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window1->Init(ui::LAYER_TEXTURED);
  window1->SetParent(NULL);
  window1->SetBounds(gfx::Rect(10, 20, 300, 400));
  window1->Show();
  wm::ActivateWindow(window1.get());

  // window1 is active, so style should have active appearance.
  Shadow* shadow1 = api.GetShadowForWindow(window1.get());
  ASSERT_TRUE(shadow1 != NULL);
  EXPECT_EQ(Shadow::STYLE_ACTIVE, shadow1->style());

  // Create a window that is transient to window1, and that has the 'hide on
  // deactivate' property set. Upon activation, window1 should still have an
  // active shadow.
  scoped_ptr<aura::Window> window2(new aura::Window(NULL));
  window2->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window2->Init(ui::LAYER_TEXTURED);
  window2->SetParent(NULL);
  window2->SetBounds(gfx::Rect(11, 21, 301, 401));
  window1->AddTransientChild(window2.get());
  aura::client::SetHideOnDeactivate(window2.get(), true);
  window2->Show();
  wm::ActivateWindow(window2.get());

  // window1 is now inactive, but its shadow should still appear active.
  EXPECT_EQ(Shadow::STYLE_ACTIVE, shadow1->style());
}

}  // namespace internal
}  // namespace ash
