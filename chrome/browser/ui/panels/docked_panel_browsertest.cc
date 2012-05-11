// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/docked_panel_strip.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/test_panel_mouse_watcher.h"

class DockedPanelBrowserTest : public BasePanelBrowserTest {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    BasePanelBrowserTest::SetUpOnMainThread();

    // All the tests here assume using mocked 800x600 screen area for the
    // primary monitor. Do the check now.
    gfx::Rect primary_screen_area = PanelManager::GetInstance()->
        display_settings_provider()->GetPrimaryScreenArea();
    DCHECK(primary_screen_area.width() == 800);
    DCHECK(primary_screen_area.height() == 600);
  }
};

IN_PROC_BROWSER_TEST_F(DockedPanelBrowserTest, SqueezePanelsInDock) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();

  // Create some docked panels.
  Panel* panel1 = CreateDockedPanel("1", gfx::Rect(0, 0, 200, 100));
  Panel* panel2 = CreateDockedPanel("2", gfx::Rect(0, 0, 200, 100));
  Panel* panel3 = CreateDockedPanel("3", gfx::Rect(0, 0, 200, 100));
  ASSERT_EQ(3, docked_strip->num_panels());

  // Check that nothing has been squeezed so far.
  EXPECT_EQ(panel1->GetBounds().width(), panel1->GetRestoredBounds().width());
  EXPECT_EQ(panel2->GetBounds().width(), panel2->GetRestoredBounds().width());
  EXPECT_EQ(panel3->GetBounds().width(), panel3->GetRestoredBounds().width());

  // Create more panels so they start getting squeezed.
  Panel* panel4 = CreateDockedPanel("4", gfx::Rect(0, 0, 200, 100));
  Panel* panel5 = CreateDockedPanel("5", gfx::Rect(0, 0, 200, 100));
  Panel* panel6 = CreateDockedPanel("6", gfx::Rect(0, 0, 200, 100));
  Panel* panel7 = CreateDockedPanel("7", gfx::Rect(0, 0, 200, 100));

  // Wait for active states to settle.
  WaitForPanelActiveState(panel7, SHOW_AS_ACTIVE);

  // Wait for the scheduled layout to run.
  MessageLoopForUI::current()->RunAllPending();

  // The active panel should be at full width.
  EXPECT_EQ(panel7->GetBounds().width(), panel7->GetRestoredBounds().width());
  EXPECT_GT(panel7->GetBounds().x(), docked_strip->display_area().x());

  // The rest of them should be at reduced width.
  EXPECT_LT(panel1->GetBounds().width(), panel1->GetRestoredBounds().width());
  EXPECT_LT(panel2->GetBounds().width(), panel2->GetRestoredBounds().width());
  EXPECT_LT(panel3->GetBounds().width(), panel3->GetRestoredBounds().width());
  EXPECT_LT(panel4->GetBounds().width(), panel4->GetRestoredBounds().width());
  EXPECT_LT(panel5->GetBounds().width(), panel5->GetRestoredBounds().width());
  EXPECT_LT(panel6->GetBounds().width(), panel6->GetRestoredBounds().width());

  // Activate a different panel.
  panel2->Activate();

  // Wait for active states to settle.
  WaitForPanelActiveState(panel2, SHOW_AS_ACTIVE);

  // Wait for the scheduled layout to run.
  MessageLoopForUI::current()->RunAllPending();

  // The active panel should be at full width.
  EXPECT_EQ(panel2->GetBounds().width(), panel2->GetRestoredBounds().width());

  // The rest of them should be at reduced width.
  EXPECT_LT(panel1->GetBounds().width(), panel1->GetRestoredBounds().width());
  EXPECT_LT(panel3->GetBounds().width(), panel3->GetRestoredBounds().width());
  EXPECT_LT(panel4->GetBounds().width(), panel4->GetRestoredBounds().width());
  EXPECT_LT(panel5->GetBounds().width(), panel5->GetRestoredBounds().width());
  EXPECT_LT(panel6->GetBounds().width(), panel6->GetRestoredBounds().width());
  EXPECT_LT(panel7->GetBounds().width(), panel7->GetRestoredBounds().width());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(DockedPanelBrowserTest, SqueezeAndThenSomeMore) {
  PanelManager* panel_manager = PanelManager::GetInstance();

  // Create enough docked panels to get into squeezing.
  Panel* panel1 = CreateDockedPanel("1", gfx::Rect(0, 0, 200, 100));
  Panel* panel2 = CreateDockedPanel("2", gfx::Rect(0, 0, 200, 100));
  Panel* panel3 = CreateDockedPanel("3", gfx::Rect(0, 0, 200, 100));
  Panel* panel4 = CreateDockedPanel("4", gfx::Rect(0, 0, 200, 100));
  Panel* panel5 = CreateDockedPanel("5", gfx::Rect(0, 0, 200, 100));
  Panel* panel6 = CreateDockedPanel("6", gfx::Rect(0, 0, 200, 100));

  // Wait for active states to settle.
  WaitForPanelActiveState(panel6, SHOW_AS_ACTIVE);

  // Wait for the scheduled layout to run.
  MessageLoopForUI::current()->RunAllPending();

  // Record current widths of some panels.
  int panel_1_width_less_squeezed = panel1->GetBounds().width();
  int panel_2_width_less_squeezed = panel2->GetBounds().width();
  int panel_3_width_less_squeezed = panel3->GetBounds().width();
  int panel_4_width_less_squeezed = panel4->GetBounds().width();
  int panel_5_width_less_squeezed = panel5->GetBounds().width();

  // These widths should be reduced.
  EXPECT_LT(panel_1_width_less_squeezed, panel1->GetRestoredBounds().width());
  EXPECT_LT(panel_2_width_less_squeezed, panel2->GetRestoredBounds().width());
  EXPECT_LT(panel_3_width_less_squeezed, panel3->GetRestoredBounds().width());
  EXPECT_LT(panel_4_width_less_squeezed, panel4->GetRestoredBounds().width());
  EXPECT_LT(panel_5_width_less_squeezed, panel5->GetRestoredBounds().width());

  Panel* panel7 = CreateDockedPanel("7", gfx::Rect(0, 0, 200, 100));

  // Wait for active states to settle.
  WaitForPanelActiveState(panel7, SHOW_AS_ACTIVE);

  // Wait for the scheduled layout to run.
  MessageLoopForUI::current()->RunAllPending();

  // The active panel should be at full width.
  EXPECT_EQ(panel7->GetBounds().width(), panel7->GetRestoredBounds().width());

  // The panels should shrink in width.
  EXPECT_LT(panel1->GetBounds().width(), panel_1_width_less_squeezed);
  EXPECT_LT(panel2->GetBounds().width(), panel_2_width_less_squeezed);
  EXPECT_LT(panel3->GetBounds().width(), panel_3_width_less_squeezed);
  EXPECT_LT(panel4->GetBounds().width(), panel_4_width_less_squeezed);
  EXPECT_LT(panel5->GetBounds().width(), panel_5_width_less_squeezed);

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(DockedPanelBrowserTest, MinimizeSqueezedActive) {
  PanelManager* panel_manager = PanelManager::GetInstance();

  // Create enough docked panels to get into squeezing.
  Panel* panel1 = CreateDockedPanel("1", gfx::Rect(0, 0, 200, 100));
  Panel* panel2 = CreateDockedPanel("2", gfx::Rect(0, 0, 200, 100));
  Panel* panel3 = CreateDockedPanel("3", gfx::Rect(0, 0, 200, 100));
  Panel* panel4 = CreateDockedPanel("4", gfx::Rect(0, 0, 200, 100));
  Panel* panel5 = CreateDockedPanel("5", gfx::Rect(0, 0, 200, 100));
  Panel* panel6 = CreateDockedPanel("6", gfx::Rect(0, 0, 200, 100));
  Panel* panel7 = CreateDockedPanel("7", gfx::Rect(0, 0, 200, 100));

  // Wait for active states to settle.
  WaitForPanelActiveState(panel7, SHOW_AS_ACTIVE);

  // Wait for the scheduled layout to run.
  MessageLoopForUI::current()->RunAllPending();

  // The active panel should be at full width.
  EXPECT_EQ(panel7->GetBounds().width(), panel7->GetRestoredBounds().width());

  // The rest of them should be at reduced width.
  EXPECT_LT(panel1->GetBounds().width(), panel1->GetRestoredBounds().width());
  EXPECT_LT(panel2->GetBounds().width(), panel2->GetRestoredBounds().width());
  EXPECT_LT(panel3->GetBounds().width(), panel3->GetRestoredBounds().width());
  EXPECT_LT(panel4->GetBounds().width(), panel4->GetRestoredBounds().width());
  EXPECT_LT(panel5->GetBounds().width(), panel5->GetRestoredBounds().width());
  EXPECT_LT(panel6->GetBounds().width(), panel6->GetRestoredBounds().width());

  // Record the width of an inactive panel and minimize it.
  int width_of_panel3_squeezed = panel3->GetBounds().width();
  panel3->Minimize();

  // Wait for any possible events.
  MessageLoopForUI::current()->RunAllPending();

  // Check that this panel is still at the same width.
  EXPECT_EQ(width_of_panel3_squeezed, panel3->GetBounds().width());

  // Minimize the active panel. It should become inactive and shrink in width.
  panel7->Minimize();

  // Wait for active states to settle.
  WaitForPanelActiveState(panel7, SHOW_AS_INACTIVE);

  // Wait for the scheduled layout to run.
  MessageLoopForUI::current()->RunAllPending();

  // The minimized panel should now be at reduced width.
  EXPECT_LT(panel7->GetBounds().width(), panel7->GetRestoredBounds().width());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(DockedPanelBrowserTest, CloseSqueezedPanels) {
  PanelManager* panel_manager = PanelManager::GetInstance();

  // Create enough docked panels to get into squeezing.
  Panel* panel1 = CreateDockedPanel("1", gfx::Rect(0, 0, 200, 100));
  Panel* panel2 = CreateDockedPanel("2", gfx::Rect(0, 0, 200, 100));
  Panel* panel3 = CreateDockedPanel("3", gfx::Rect(0, 0, 200, 100));
  Panel* panel4 = CreateDockedPanel("4", gfx::Rect(0, 0, 200, 100));
  Panel* panel5 = CreateDockedPanel("5", gfx::Rect(0, 0, 200, 100));
  Panel* panel6 = CreateDockedPanel("6", gfx::Rect(0, 0, 200, 100));
  Panel* panel7 = CreateDockedPanel("7", gfx::Rect(0, 0, 200, 100));

  // Wait for active states to settle.
  WaitForPanelActiveState(panel7, SHOW_AS_ACTIVE);

  // Wait for the scheduled layout to run.
  MessageLoopForUI::current()->RunAllPending();

  // Record current widths of some panels.
  int panel_1_orig_width = panel1->GetBounds().width();
  int panel_2_orig_width = panel2->GetBounds().width();
  int panel_3_orig_width = panel3->GetBounds().width();
  int panel_4_orig_width = panel4->GetBounds().width();
  int panel_5_orig_width = panel5->GetBounds().width();
  int panel_6_orig_width = panel6->GetBounds().width();
  int panel_7_orig_width = panel7->GetBounds().width();

  // The active panel should be at full width.
  EXPECT_EQ(panel_7_orig_width, panel7->GetRestoredBounds().width());

  // The rest of them should be at reduced width.
  EXPECT_LT(panel_1_orig_width, panel1->GetRestoredBounds().width());
  EXPECT_LT(panel_2_orig_width, panel2->GetRestoredBounds().width());
  EXPECT_LT(panel_3_orig_width, panel3->GetRestoredBounds().width());
  EXPECT_LT(panel_4_orig_width, panel4->GetRestoredBounds().width());
  EXPECT_LT(panel_5_orig_width, panel5->GetRestoredBounds().width());
  EXPECT_LT(panel_6_orig_width, panel6->GetRestoredBounds().width());

  // Close one panel.
  panel2->Close();

  // Wait for all processing to finish.
  MessageLoopForUI::current()->RunAllPending();

  // The widths of the remaining panels should have increased.
  EXPECT_GT(panel1->GetBounds().width(), panel_1_orig_width);
  EXPECT_GT(panel3->GetBounds().width(), panel_3_orig_width);
  EXPECT_GT(panel4->GetBounds().width(), panel_4_orig_width);
  EXPECT_GT(panel5->GetBounds().width(), panel_5_orig_width);
  EXPECT_GT(panel6->GetBounds().width(), panel_6_orig_width);

  // The active panel should have stayed at full width.
  EXPECT_EQ(panel7->GetBounds().width(), panel_7_orig_width);

  // Close several panels.
  panel3->Close();
  panel5->Close();
  panel7->Close();

  // Wait for all processing to finish.
  MessageLoopForUI::current()->RunAllPending();

  // We should not have squeezing any more; all panels should be at full width.
  EXPECT_EQ(panel1->GetBounds().width(), panel1->GetRestoredBounds().width());
  EXPECT_EQ(panel4->GetBounds().width(), panel4->GetRestoredBounds().width());
  EXPECT_EQ(panel6->GetBounds().width(), panel6->GetRestoredBounds().width());

  panel_manager->CloseAll();
}
