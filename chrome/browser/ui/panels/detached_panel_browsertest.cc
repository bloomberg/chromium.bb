// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/detached_panel_collection.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"

class DetachedPanelBrowserTest : public BasePanelBrowserTest {
};

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest, CheckDetachedPanelProperties) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create an initially detached panel (as opposed to other tests which create
  // a docked panel, then detaches it).
  gfx::Rect bounds(300, 200, 250, 200);
  CreatePanelParams params("1", bounds, SHOW_AS_ACTIVE);
  params.create_mode = PanelManager::CREATE_AS_DETACHED;
  Panel* panel = CreatePanelWithParams(params);
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));

  EXPECT_EQ(1, panel_manager->num_panels());
  EXPECT_TRUE(detached_collection->HasPanel(panel));

  EXPECT_EQ(bounds.x(), panel->GetBounds().x());
  // Ignore checking y position since the detached panel will be placed near
  // the top if the stacking mode is enabled.
  if (!PanelManager::IsPanelStackingEnabled())
    EXPECT_EQ(bounds.y(), panel->GetBounds().y());
  EXPECT_EQ(bounds.width(), panel->GetBounds().width());
  EXPECT_EQ(bounds.height(), panel->GetBounds().height());
  EXPECT_FALSE(panel->IsAlwaysOnTop());

  EXPECT_TRUE(panel_testing->IsButtonVisible(panel::CLOSE_BUTTON));
  // The minimize button will not be shown on some Linux desktop environment
  // that does not support system minimize.
  if (PanelManager::CanUseSystemMinimize() &&
      PanelManager::IsPanelStackingEnabled()) {
    EXPECT_TRUE(panel_testing->IsButtonVisible(panel::MINIMIZE_BUTTON));
  } else {
    EXPECT_FALSE(panel_testing->IsButtonVisible(panel::MINIMIZE_BUTTON));
  }
  EXPECT_FALSE(panel_testing->IsButtonVisible(panel::RESTORE_BUTTON));

  EXPECT_EQ(panel::RESIZABLE_ALL, panel->CanResizeByMouse());

  EXPECT_EQ(panel::ALL_ROUNDED, panel_testing->GetWindowCornerStyle());

  Panel::AttentionMode expected_attention_mode =
      static_cast<Panel::AttentionMode>(Panel::USE_PANEL_ATTENTION |
                                         Panel::USE_SYSTEM_ATTENTION);
  EXPECT_EQ(expected_attention_mode, panel->attention_mode());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest, DrawAttentionOnActive) {
  // Create a detached panel that is initially active.
  Panel* panel = CreateDetachedPanel("1", gfx::Rect(300, 200, 250, 200));
  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));

  // Test that the attention should not be drawn if the detached panel is in
  // focus.
  EXPECT_FALSE(panel->IsDrawingAttention());
  panel->FlashFrame(true);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest, DrawAttentionOnInactive) {
  // Create an inactive detached panel.
  Panel* panel =
      CreateInactiveDetachedPanel("1", gfx::Rect(300, 200, 250, 200));
  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));

  // Test that the attention is drawn when the detached panel is not in focus.
  EXPECT_FALSE(panel->IsActive());
  EXPECT_FALSE(panel->IsDrawingAttention());
  panel->FlashFrame(true);
  EXPECT_TRUE(panel->IsDrawingAttention());
  EXPECT_TRUE(native_panel_testing->VerifyDrawingAttention());

  // Stop drawing attention.
  panel->FlashFrame(false);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest, DrawAttentionResetOnActivate) {
  // Create an inactive detached panel.
  Panel* panel =
      CreateInactiveDetachedPanel("1", gfx::Rect(300, 200, 250, 200));
  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));

  // Test that the attention is drawn when the detached panel is not in focus.
  panel->FlashFrame(true);
  EXPECT_TRUE(panel->IsDrawingAttention());
  EXPECT_TRUE(native_panel_testing->VerifyDrawingAttention());

  // Test that the attention is cleared when panel gets focus.
  panel->Activate();
  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest, ClickTitlebar) {
  Panel* panel = CreateDetachedPanel("1", gfx::Rect(300, 200, 250, 200));
  EXPECT_FALSE(panel->IsMinimized());

  // Clicking on an active detached panel's titlebar has no effect, regardless
  // of modifier.
  scoped_ptr<NativePanelTesting> test_panel(
      CreateNativePanelTesting(panel));
  test_panel->PressLeftMouseButtonTitlebar(panel->GetBounds().origin());
  test_panel->ReleaseMouseButtonTitlebar();
  EXPECT_TRUE(panel->IsActive());
  EXPECT_FALSE(panel->IsMinimized());

  test_panel->PressLeftMouseButtonTitlebar(panel->GetBounds().origin(),
                                           panel::APPLY_TO_ALL);
  test_panel->ReleaseMouseButtonTitlebar(panel::APPLY_TO_ALL);
  EXPECT_TRUE(panel->IsActive());
  EXPECT_FALSE(panel->IsMinimized());

  // Clicking on an inactive detached panel's titlebar activates it.
  DeactivatePanel(panel);
  test_panel->PressLeftMouseButtonTitlebar(panel->GetBounds().origin());
  test_panel->ReleaseMouseButtonTitlebar();
  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);
  EXPECT_FALSE(panel->IsMinimized());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest,
                       UpdateDetachedPanelOnPrimaryDisplayChange) {
  PanelManager* panel_manager = PanelManager::GetInstance();

  // Create a big detached panel on the primary display.
  gfx::Rect initial_bounds(50, 50, 700, 500);
  Panel* panel = CreateDetachedPanel("1", initial_bounds);
  EXPECT_EQ(initial_bounds, panel->GetBounds());

  // Make the primary display smaller.
  // Expect that the panel should be resized to fit within the display.
  gfx::Rect primary_display_area(0, 0, 500, 300);
  gfx::Rect primary_work_area(0, 0, 500, 280);
  mock_display_settings_provider()->SetPrimaryDisplay(
      primary_display_area, primary_work_area);

  gfx::Rect bounds = panel->GetBounds();
  EXPECT_LE(primary_work_area.x(), bounds.x());
  EXPECT_LE(bounds.x(), primary_work_area.right());
  EXPECT_LE(primary_work_area.y(), bounds.y());
  EXPECT_LE(bounds.y(), primary_work_area.bottom());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest,
                       UpdateDetachedPanelOnSecondaryDisplayChange) {
  PanelManager* panel_manager = PanelManager::GetInstance();

  // Setup 2 displays with secondary display on the right side of primary
  // display.
  gfx::Rect primary_display_area(0, 0, 400, 600);
  gfx::Rect primary_work_area(0, 0, 400, 560);
  mock_display_settings_provider()->SetPrimaryDisplay(
      primary_display_area, primary_work_area);
  gfx::Rect secondary_display_area(400, 0, 400, 500);
  gfx::Rect secondary_work_area(400, 0, 400, 460);
  mock_display_settings_provider()->SetSecondaryDisplay(
      secondary_display_area, secondary_work_area);

  // Create a big detached panel on the seconday display.
  gfx::Rect initial_bounds(450, 50, 350, 400);
  Panel* panel = CreateDetachedPanel("1", initial_bounds);
  EXPECT_EQ(initial_bounds, panel->GetBounds());

  // Move down the secondary display and make it smaller.
  // Expect that the panel should be resized to fit within the display.
  secondary_display_area.SetRect(400, 100, 300, 400);
  secondary_work_area.SetRect(400, 100, 300, 360);
  mock_display_settings_provider()->SetSecondaryDisplay(
      secondary_display_area, secondary_work_area);

  gfx::Rect bounds = panel->GetBounds();
  EXPECT_LE(secondary_work_area.x(), bounds.x());
  EXPECT_LE(bounds.x(), secondary_work_area.right());
  EXPECT_LE(secondary_work_area.y(), bounds.y());
  EXPECT_LE(bounds.y(), secondary_work_area.bottom());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest,
                       KeepShowingDetachedPanelCreatedBeforeFullScreenMode) {
  // Create a detached panel.
  CreatePanelParams params("1", gfx::Rect(300, 200, 250, 200), SHOW_AS_ACTIVE);
  params.create_mode = PanelManager::CREATE_AS_DETACHED;
  Panel* panel = CreatePanelWithParams(params);
  scoped_ptr<NativePanelTesting> panel_testing(CreateNativePanelTesting(panel));

  // Panel should be visible at first.
  EXPECT_TRUE(panel_testing->IsWindowVisible());

  // Panel's visibility should not be affected when entering full-screen mode.
  mock_display_settings_provider()->EnableFullScreenMode(true);
  EXPECT_TRUE(panel_testing->IsWindowVisible());

  // Panel's visibility should not be affected when leaving full-screen mode.
  mock_display_settings_provider()->EnableFullScreenMode(false);
  EXPECT_TRUE(panel_testing->IsWindowVisible());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest,
                       HideDetachedPanelCreatedOnFullScreenMode) {
  // Enable full-screen mode first.
  mock_display_settings_provider()->EnableFullScreenMode(true);

  // Create a detached panel without waiting it to be shown since it is not
  // supposed to be shown on full-screen mode.
  CreatePanelParams params("1", gfx::Rect(300, 200, 250, 200), SHOW_AS_ACTIVE);
  params.create_mode = PanelManager::CREATE_AS_DETACHED;
  params.wait_for_fully_created = false;
  Panel* panel = CreatePanelWithParams(params);
  scoped_ptr<NativePanelTesting> panel_testing(CreateNativePanelTesting(panel));

  // Panel should not be shown on full-screen mode.
  EXPECT_FALSE(panel_testing->IsWindowVisible());

  // Panel should become visible when leaving full-screen mode.
  mock_display_settings_provider()->EnableFullScreenMode(false);
  EXPECT_TRUE(panel_testing->IsWindowVisible());

  PanelManager::GetInstance()->CloseAll();
}
