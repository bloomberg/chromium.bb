// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/detached_panel_strip.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"

class DetachedPanelBrowserTest : public BasePanelBrowserTest {
};

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest, CheckDetachedPanelProperties) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();

  // Create an initially detached panel (as opposed to other tests which create
  // a docked panel, then detaches it).
  gfx::Rect bounds(300, 200, 250, 200);
  CreatePanelParams params("1", bounds, SHOW_AS_ACTIVE);
  params.create_mode = PanelManager::CREATE_AS_DETACHED;
  Panel* panel = CreatePanelWithParams(params);
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));

  EXPECT_EQ(1, panel_manager->num_panels());
  EXPECT_TRUE(detached_strip->HasPanel(panel));

  EXPECT_EQ(bounds, panel->GetBounds());
  EXPECT_FALSE(panel->always_on_top());

  EXPECT_TRUE(panel_testing->IsButtonVisible(panel::CLOSE_BUTTON));
  EXPECT_FALSE(panel_testing->IsButtonVisible(panel::MINIMIZE_BUTTON));
  EXPECT_FALSE(panel_testing->IsButtonVisible(panel::RESTORE_BUTTON));

  EXPECT_EQ(panel::RESIZABLE_ALL_SIDES, panel->CanResizeByMouse());

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
  EXPECT_TRUE(panel->IsActive());
  EXPECT_FALSE(panel->IsDrawingAttention());
  panel->FlashFrame(true);
  EXPECT_FALSE(panel->IsDrawingAttention());
  MessageLoop::current()->RunAllPending();
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  panel->Close();
}

#if defined(OS_LINUX)
// http://crbug.com/145740
#define MAYBE_DrawAttentionOnInactive FLAKY_DrawAttentionOnInactive
#else
#define MAYBE_DrawAttentionOnInactive DrawAttentionOnInactive
#endif
IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest,
                       MAYBE_DrawAttentionOnInactive) {
  // Create an inactive detached panel.
  Panel* panel = CreateDetachedPanel("1", gfx::Rect(300, 200, 250, 200));
  panel->Deactivate();
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);

  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));

  // Test that the attention is drawn when the detached panel is not in focus.
  EXPECT_FALSE(panel->IsActive());
  EXPECT_FALSE(panel->IsDrawingAttention());
  panel->FlashFrame(true);
  EXPECT_TRUE(panel->IsDrawingAttention());
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(native_panel_testing->VerifyDrawingAttention());

  // Stop drawing attention.
  panel->FlashFrame(false);
  EXPECT_FALSE(panel->IsDrawingAttention());
  MessageLoop::current()->RunAllPending();
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest, DrawAttentionResetOnActivate) {
  // Create 2 panels so we end up with an inactive panel that can
  // be made to draw attention.
  Panel* panel1 = CreatePanel("test panel1");
  Panel* panel2 = CreatePanel("test panel2");

  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel1));

  // Test that the attention is drawn when the detached panel is not in focus.
  panel1->FlashFrame(true);
  EXPECT_TRUE(panel1->IsDrawingAttention());
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(native_panel_testing->VerifyDrawingAttention());

  // Test that the attention is cleared when panel gets focus.
  panel1->Activate();
  WaitForPanelActiveState(panel1, SHOW_AS_ACTIVE);
  EXPECT_FALSE(panel1->IsDrawingAttention());
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  panel1->Close();
  panel2->Close();
}

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest, ClickTitlebar) {
  PanelManager* panel_manager = PanelManager::GetInstance();

  Panel* panel = CreateDetachedPanel("1", gfx::Rect(300, 200, 250, 200));
  EXPECT_TRUE(panel->IsActive());
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

  // Create a second panel to cause the first to become inactive.
  CreateDetachedPanel("2", gfx::Rect(100, 200, 230, 345));
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);

  // Clicking on an inactive detached panel's titlebar activates it.
  test_panel->PressLeftMouseButtonTitlebar(panel->GetBounds().origin());
  test_panel->ReleaseMouseButtonTitlebar();
  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);
  EXPECT_FALSE(panel->IsMinimized());

  panel_manager->CloseAll();
}
