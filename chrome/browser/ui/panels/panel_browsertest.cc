// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/docked_panel_collection.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/test_panel_active_state_observer.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "extensions/common/constants.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/events/event_utils.h"
#include "ui/gfx/screen.h"

using content::WebContents;

class PanelBrowserTest : public BasePanelBrowserTest {
 public:
  PanelBrowserTest() : BasePanelBrowserTest() {
  }

 protected:
  // Helper function for debugging.
  void PrintAllPanelBounds() {
    const std::vector<Panel*>& panels = PanelManager::GetInstance()->panels();
    DLOG(WARNING) << "PanelBounds:";
    for (size_t i = 0; i < panels.size(); ++i) {
      DLOG(WARNING) << "#=" << i
                    << ", ptr=" << panels[i]
                    << ", x=" << panels[i]->GetBounds().x()
                    << ", y=" << panels[i]->GetBounds().y()
                    << ", width=" << panels[i]->GetBounds().width()
                    << ", height" << panels[i]->GetBounds().height();
    }
  }

  std::vector<gfx::Rect> GetAllPanelBounds() {
    std::vector<Panel*> panels = PanelManager::GetInstance()->panels();
    std::vector<gfx::Rect> bounds;
    for (size_t i = 0; i < panels.size(); i++)
      bounds.push_back(panels[i]->GetBounds());
    return bounds;
  }

  std::vector<gfx::Rect> AddXDeltaToBounds(const std::vector<gfx::Rect>& bounds,
                                           const std::vector<int>& delta_x) {
    std::vector<gfx::Rect> new_bounds = bounds;
    for (size_t i = 0; i < bounds.size(); ++i)
      new_bounds[i].Offset(delta_x[i], 0);
    return new_bounds;
  }

  std::vector<Panel::ExpansionState> GetAllPanelExpansionStates() {
    std::vector<Panel*> panels = PanelManager::GetInstance()->panels();
    std::vector<Panel::ExpansionState> expansion_states;
    for (size_t i = 0; i < panels.size(); i++)
      expansion_states.push_back(panels[i]->expansion_state());
    return expansion_states;
  }

  std::vector<bool> GetAllPanelActiveStates() {
    std::vector<Panel*> panels = PanelManager::GetInstance()->panels();
    std::vector<bool> active_states;
    for (size_t i = 0; i < panels.size(); i++)
      active_states.push_back(panels[i]->IsActive());
    return active_states;
  }

  std::vector<bool> ProduceExpectedActiveStates(
      int expected_active_panel_index) {
    std::vector<Panel*> panels = PanelManager::GetInstance()->panels();
    std::vector<bool> active_states;
    for (int i = 0; i < static_cast<int>(panels.size()); i++)
      active_states.push_back(i == expected_active_panel_index);
    return active_states;
  }

  void WaitForPanelActiveStates(const std::vector<bool>& old_states,
                                const std::vector<bool>& new_states) {
    DCHECK(old_states.size() == new_states.size());
    std::vector<Panel*> panels = PanelManager::GetInstance()->panels();
    for (size_t i = 0; i < old_states.size(); i++) {
      if (old_states[i] != new_states[i]){
        WaitForPanelActiveState(
            panels[i], new_states[i] ? SHOW_AS_ACTIVE : SHOW_AS_INACTIVE);
      }
    }
  }

  void TestMinimizeRestore() {
    // This constant is used to generate a point 'sufficiently higher then
    // top edge of the panel'. On some platforms (Mac) we extend hover area
    // a bit above the minimized panel as well, so it takes significant
    // distance to 'move mouse out' of the hover-sensitive area.
    const int kFarEnoughFromHoverArea = 153;

    PanelManager* panel_manager = PanelManager::GetInstance();
    std::vector<Panel*> panels = panel_manager->panels();
    std::vector<gfx::Rect> test_begin_bounds = GetAllPanelBounds();
    std::vector<gfx::Rect> expected_bounds = test_begin_bounds;
    std::vector<Panel::ExpansionState> expected_expansion_states(
        panels.size(), Panel::EXPANDED);
    std::vector<NativePanelTesting*> native_panels_testing(panels.size());
    for (size_t i = 0; i < panels.size(); ++i) {
      native_panels_testing[i] = CreateNativePanelTesting(panels[i]);
    }

    // Verify titlebar click does not minimize.
    for (size_t index = 0; index < panels.size(); ++index) {
      // Press left mouse button.  Verify nothing changed.
      native_panels_testing[index]->PressLeftMouseButtonTitlebar(
          panels[index]->GetBounds().origin());
      EXPECT_EQ(expected_bounds, GetAllPanelBounds());
      EXPECT_EQ(expected_expansion_states, GetAllPanelExpansionStates());

      // Release mouse button.  Verify nothing changed.
      native_panels_testing[index]->ReleaseMouseButtonTitlebar();
      EXPECT_EQ(expected_bounds, GetAllPanelBounds());
      EXPECT_EQ(expected_expansion_states, GetAllPanelExpansionStates());
    }

    // Minimize all panels for next stage in test.
    for (size_t index = 0; index < panels.size(); ++index) {
      panels[index]->Minimize();
      expected_bounds[index].set_height(panel::kMinimizedPanelHeight);
      expected_bounds[index].set_y(
          test_begin_bounds[index].y() +
          test_begin_bounds[index].height() - panel::kMinimizedPanelHeight);
      expected_expansion_states[index] = Panel::MINIMIZED;
      EXPECT_EQ(expected_bounds, GetAllPanelBounds());
      EXPECT_EQ(expected_expansion_states, GetAllPanelExpansionStates());
    }

    // Setup bounds and expansion states for minimized and titlebar-only
    // states.
    std::vector<Panel::ExpansionState> titlebar_exposed_states(
        panels.size(), Panel::TITLE_ONLY);
    std::vector<gfx::Rect> minimized_bounds = expected_bounds;
    std::vector<Panel::ExpansionState> minimized_states(
        panels.size(), Panel::MINIMIZED);
    std::vector<gfx::Rect> titlebar_exposed_bounds = test_begin_bounds;
    for (size_t index = 0; index < panels.size(); ++index) {
      titlebar_exposed_bounds[index].set_height(
          panels[index]->native_panel()->TitleOnlyHeight());
      titlebar_exposed_bounds[index].set_y(
          test_begin_bounds[index].y() +
          test_begin_bounds[index].height() -
          panels[index]->native_panel()->TitleOnlyHeight());
    }

    // Test hover.  All panels are currently in minimized state.
    EXPECT_EQ(minimized_states, GetAllPanelExpansionStates());
    for (size_t index = 0; index < panels.size(); ++index) {
      // Hover mouse on minimized panel.
      // Verify titlebar is exposed on all panels.
      gfx::Point hover_point(panels[index]->GetBounds().origin());
      MoveMouseAndWaitForExpansionStateChange(panels[index], hover_point);
      EXPECT_EQ(titlebar_exposed_bounds, GetAllPanelBounds());
      EXPECT_EQ(titlebar_exposed_states, GetAllPanelExpansionStates());

      // Hover mouse above the panel. Verify all panels are minimized.
      hover_point.set_y(
          panels[index]->GetBounds().y() - kFarEnoughFromHoverArea);
      MoveMouseAndWaitForExpansionStateChange(panels[index], hover_point);
      EXPECT_EQ(minimized_bounds, GetAllPanelBounds());
      EXPECT_EQ(minimized_states, GetAllPanelExpansionStates());

      // Hover mouse below minimized panel.
      // Verify titlebar is exposed on all panels.
      hover_point.set_y(panels[index]->GetBounds().y() +
                        panels[index]->GetBounds().height() + 5);
      MoveMouseAndWaitForExpansionStateChange(panels[index], hover_point);
      EXPECT_EQ(titlebar_exposed_bounds, GetAllPanelBounds());
      EXPECT_EQ(titlebar_exposed_states, GetAllPanelExpansionStates());

      // Hover below titlebar exposed panel.  Verify nothing changed.
      hover_point.set_y(panels[index]->GetBounds().y() +
                        panels[index]->GetBounds().height() + 6);
      MoveMouse(hover_point);
      EXPECT_EQ(titlebar_exposed_bounds, GetAllPanelBounds());
      EXPECT_EQ(titlebar_exposed_states, GetAllPanelExpansionStates());

      // Hover mouse above panel.  Verify all panels are minimized.
      hover_point.set_y(
          panels[index]->GetBounds().y() - kFarEnoughFromHoverArea);
      MoveMouseAndWaitForExpansionStateChange(panels[index], hover_point);
      EXPECT_EQ(minimized_bounds, GetAllPanelBounds());
      EXPECT_EQ(minimized_states, GetAllPanelExpansionStates());
    }

    // Test restore.  All panels are currently in minimized state.
    for (size_t index = 0; index < panels.size(); ++index) {
      // Hover on the last panel.  This is to test the case of clicking on the
      // panel when it's in titlebar exposed state.
      if (index == panels.size() - 1)
        MoveMouse(minimized_bounds[index].origin());

      // Click minimized or title bar exposed panel as the case may be.
      // Verify panel is restored to its original size.
      native_panels_testing[index]->PressLeftMouseButtonTitlebar(
          panels[index]->GetBounds().origin());
      native_panels_testing[index]->ReleaseMouseButtonTitlebar();
      expected_bounds[index].set_height(
          test_begin_bounds[index].height());
      expected_bounds[index].set_y(test_begin_bounds[index].y());
      expected_expansion_states[index] = Panel::EXPANDED;
      EXPECT_EQ(expected_bounds, GetAllPanelBounds());
      EXPECT_EQ(expected_expansion_states, GetAllPanelExpansionStates());

      // Hover again on the last panel which is now restored, to reset the
      // titlebar exposed state.
      if (index == panels.size() - 1)
        MoveMouse(minimized_bounds[index].origin());
    }

    // The below could be separate tests, just adding a TODO here for tracking.
    // TODO(prasadt): Add test for dragging when in titlebar exposed state.
    // TODO(prasadt): Add test in presence of auto hiding task bar.

    for (size_t i = 0; i < panels.size(); ++i)
      delete native_panels_testing[i];
  }
};

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, CheckDockedPanelProperties) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelCollection* docked_collection = panel_manager->docked_collection();

  // Create 3 docked panels that are in expanded, title-only or minimized states
  // respectively.
  Panel* panel1 = CreatePanelWithBounds("1", gfx::Rect(0, 0, 100, 100));
  Panel* panel2 = CreatePanelWithBounds("2", gfx::Rect(0, 0, 100, 100));
  Panel* panel3 = CreatePanelWithBounds("3", gfx::Rect(0, 0, 100, 100));
  panel2->SetExpansionState(Panel::TITLE_ONLY);
  EXPECT_EQ(Panel::TITLE_ONLY, panel2->expansion_state());
  panel3->SetExpansionState(Panel::MINIMIZED);
  EXPECT_EQ(Panel::MINIMIZED, panel3->expansion_state());
  scoped_ptr<NativePanelTesting> panel1_testing(
      CreateNativePanelTesting(panel1));
  scoped_ptr<NativePanelTesting> panel2_testing(
      CreateNativePanelTesting(panel2));
  scoped_ptr<NativePanelTesting> panel3_testing(
      CreateNativePanelTesting(panel3));

  // Ensure that the layout message can get a chance to be processed so that
  // the button visibility can be updated.
  MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(3, panel_manager->num_panels());
  EXPECT_TRUE(docked_collection->HasPanel(panel1));
  EXPECT_TRUE(docked_collection->HasPanel(panel2));
  EXPECT_TRUE(docked_collection->HasPanel(panel3));

  EXPECT_EQ(Panel::EXPANDED, panel1->expansion_state());
  EXPECT_EQ(Panel::TITLE_ONLY, panel2->expansion_state());
  EXPECT_EQ(Panel::MINIMIZED, panel3->expansion_state());

  EXPECT_TRUE(panel1->IsAlwaysOnTop());
  EXPECT_TRUE(panel2->IsAlwaysOnTop());
  EXPECT_TRUE(panel3->IsAlwaysOnTop());

  EXPECT_TRUE(panel1_testing->IsButtonVisible(panel::CLOSE_BUTTON));
  EXPECT_TRUE(panel2_testing->IsButtonVisible(panel::CLOSE_BUTTON));
  EXPECT_TRUE(panel3_testing->IsButtonVisible(panel::CLOSE_BUTTON));

  EXPECT_TRUE(panel1_testing->IsButtonVisible(panel::MINIMIZE_BUTTON));
  EXPECT_FALSE(panel2_testing->IsButtonVisible(panel::MINIMIZE_BUTTON));
  EXPECT_FALSE(panel3_testing->IsButtonVisible(panel::MINIMIZE_BUTTON));

  EXPECT_FALSE(panel1_testing->IsButtonVisible(panel::RESTORE_BUTTON));
  EXPECT_TRUE(panel2_testing->IsButtonVisible(panel::RESTORE_BUTTON));
  EXPECT_TRUE(panel3_testing->IsButtonVisible(panel::RESTORE_BUTTON));

  // Expanded panel cannot be resized at the bottom.
  EXPECT_EQ(panel::RESIZABLE_EXCEPT_BOTTOM, panel1->CanResizeByMouse());
  EXPECT_EQ(panel::NOT_RESIZABLE, panel2->CanResizeByMouse());
  EXPECT_EQ(panel::NOT_RESIZABLE, panel3->CanResizeByMouse());

  EXPECT_EQ(panel::TOP_ROUNDED, panel1_testing->GetWindowCornerStyle());
  EXPECT_EQ(panel::TOP_ROUNDED, panel1_testing->GetWindowCornerStyle());
  EXPECT_EQ(panel::TOP_ROUNDED, panel3_testing->GetWindowCornerStyle());

  EXPECT_EQ(Panel::USE_PANEL_ATTENTION, panel1->attention_mode());
  EXPECT_EQ(Panel::USE_PANEL_ATTENTION, panel2->attention_mode());
  EXPECT_EQ(Panel::USE_PANEL_ATTENTION, panel3->attention_mode());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, CreatePanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  EXPECT_EQ(0, panel_manager->num_panels()); // No panels initially.

  Panel* panel = CreatePanel("PanelTest");
  EXPECT_EQ(1, panel_manager->num_panels());

  gfx::Rect bounds = panel->GetBounds();
  EXPECT_GT(bounds.x(), 0);
  EXPECT_GT(bounds.y(), 0);
  EXPECT_GT(bounds.width(), 0);
  EXPECT_GT(bounds.height(), 0);

  EXPECT_EQ(bounds.right(),
            panel_manager->docked_collection()->StartingRightPosition());

  CloseWindowAndWait(panel);

  EXPECT_EQ(0, panel_manager->num_panels());
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, CreateBigPanel) {
  gfx::Rect work_area = PanelManager::GetInstance()->
      display_settings_provider()->GetDisplayArea();
  Panel* panel = CreatePanelWithBounds("BigPanel", work_area);
  gfx::Rect bounds = panel->GetBounds();
  EXPECT_EQ(panel->max_size().width(), bounds.width());
  EXPECT_LT(bounds.width(), work_area.width());
  EXPECT_EQ(panel->max_size().height(), bounds.height());
  EXPECT_LT(bounds.height(), work_area.height());
  panel->Close();
}

class WaitForStableInitialSize : public TestPanelNotificationObserver {
 public:
  explicit WaitForStableInitialSize(Panel* panel)
      : TestPanelNotificationObserver(
          chrome::NOTIFICATION_PANEL_COLLECTION_UPDATED,
          content::NotificationService::AllSources()),
        panel_(panel) {}
  virtual ~WaitForStableInitialSize() {}

 protected:
  virtual bool AtExpectedState() OVERRIDE {
    return panel_->GetBounds().height() > panel_->TitleOnlyHeight();
  }
  Panel* panel_;
};

class WaitForAutoResizeWider : public TestPanelNotificationObserver {
 public:
  explicit WaitForAutoResizeWider(Panel* panel)
      : TestPanelNotificationObserver(
          chrome::NOTIFICATION_PANEL_COLLECTION_UPDATED,
          content::NotificationService::AllSources()),
        panel_(panel),
        initial_size_(panel->GetBounds().size()) {}
  virtual ~WaitForAutoResizeWider() {}

 protected:
  virtual bool AtExpectedState() OVERRIDE {
    return panel_->GetBounds().width() > initial_size_.width();
  }
  Panel* panel_;
  gfx::Size initial_size_;
};

class WaitForAutoResizeNarrower : public TestPanelNotificationObserver {
 public:
  explicit WaitForAutoResizeNarrower(Panel* panel)
      : TestPanelNotificationObserver(
          chrome::NOTIFICATION_PANEL_COLLECTION_UPDATED,
          content::NotificationService::AllSources()),
        panel_(panel),
        initial_size_(panel->GetBounds().size()) {}
  virtual ~WaitForAutoResizeNarrower() {}

 protected:
  virtual bool AtExpectedState() OVERRIDE {
    return panel_->GetBounds().width() < initial_size_.width();
  }
  Panel* panel_;
  gfx::Size initial_size_;
};

// crbug.com/160504
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, DISABLED_AutoResize) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  panel_manager->enable_auto_sizing(true);
  // Bigger space is needed by this test.
  SetTestingAreas(gfx::Rect(0, 0, 1200, 900), gfx::Rect());

  // Create a test panel with web contents loaded.
  CreatePanelParams params("PanelTest1", gfx::Rect(), SHOW_AS_ACTIVE);
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("update-preferred-size.html"))));
  params.url = url;
  Panel* panel = CreatePanelWithParams(params);

  // Ensure panel has auto resized to original web content size.
  // The resize will update the docked panel collection.
  WaitForStableInitialSize initial_resize(panel);
  initial_resize.Wait();
  gfx::Rect initial_bounds = panel->GetBounds();

  // Expand the test page. The resize will update the docked panel collection.
  WaitForAutoResizeWider enlarge(panel);
  EXPECT_TRUE(content::ExecuteScript(
      panel->GetWebContents(), "changeSize(50);"));
  enlarge.Wait();
  gfx::Rect bounds_on_grow = panel->GetBounds();
  EXPECT_GT(bounds_on_grow.width(), initial_bounds.width());
  EXPECT_EQ(bounds_on_grow.height(), initial_bounds.height());

  // Shrink the test page. The resize will update the docked panel collection.
  WaitForAutoResizeNarrower shrink(panel);
  EXPECT_TRUE(content::ExecuteScript(
      panel->GetWebContents(), "changeSize(-30);"));
  shrink.Wait();
  gfx::Rect bounds_on_shrink = panel->GetBounds();
  EXPECT_LT(bounds_on_shrink.width(), bounds_on_grow.width());
  EXPECT_GT(bounds_on_shrink.width(), initial_bounds.width());
  EXPECT_EQ(bounds_on_shrink.height(), initial_bounds.height());

  // Verify resizing turns off auto-resizing and panel no longer auto-resizes.
  gfx::Rect previous_bounds = panel->GetBounds();
  // These should be identical because the panel is expanded.
  EXPECT_EQ(previous_bounds.size(), panel->GetRestoredBounds().size());
  gfx::Size new_size(previous_bounds.size());
  new_size.Enlarge(5, 5);
  gfx::Rect new_bounds(previous_bounds.origin(), new_size);
  panel->SetBounds(new_bounds);
  EXPECT_FALSE(panel->auto_resizable());
  EXPECT_EQ(new_bounds.size(), panel->GetBounds().size());
  EXPECT_EQ(new_bounds.size(), panel->GetRestoredBounds().size());

  // Turn back on auto-resize and verify that panel auto resizes.
  content::WindowedNotificationObserver auto_resize_enabled(
      chrome::NOTIFICATION_PANEL_COLLECTION_UPDATED,
      content::NotificationService::AllSources());
  panel->SetAutoResizable(true);
  auto_resize_enabled.Wait();
  gfx::Rect bounds_auto_resize_enabled = panel->GetBounds();
  EXPECT_EQ(bounds_on_shrink.width(), bounds_auto_resize_enabled.width());
  EXPECT_EQ(bounds_on_shrink.height(), bounds_auto_resize_enabled.height());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, ResizePanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  panel_manager->enable_auto_sizing(true);

  Panel* panel = CreatePanel("TestPanel");
  EXPECT_TRUE(panel->auto_resizable());
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());

  // Verify resizing turns off auto-resizing and that it works.
  gfx::Rect original_bounds = panel->GetBounds();
  // These should be identical because the panel is expanded.
  EXPECT_EQ(original_bounds.size(), panel->GetRestoredBounds().size());
  gfx::Size new_size(original_bounds.size());
  new_size.Enlarge(5, 5);
  gfx::Rect new_bounds(original_bounds.origin(), new_size);
  panel->SetBounds(new_bounds);
  EXPECT_FALSE(panel->auto_resizable());
  EXPECT_EQ(new_bounds.size(), panel->GetBounds().size());
  EXPECT_EQ(new_bounds.size(), panel->GetRestoredBounds().size());

  // Verify current height unaffected when panel is not expanded.
  panel->SetExpansionState(Panel::MINIMIZED);
  int original_height = panel->GetBounds().height();
  new_size.Enlarge(5, 5);
  new_bounds.set_size(new_size);
  panel->SetBounds(new_bounds);
  EXPECT_EQ(new_bounds.size().width(), panel->GetBounds().width());
  EXPECT_EQ(original_height, panel->GetBounds().height());
  EXPECT_EQ(new_bounds.size(), panel->GetRestoredBounds().size());

  panel->Close();
}

#if defined(OS_LINUX)
// There is no animations on Linux, by design (http://crbug.com/144074).
#define MAYBE_AnimateBounds DISABLED_AnimateBounds
#else
#define MAYBE_AnimateBounds AnimateBounds
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_AnimateBounds) {
  Panel* panel = CreatePanelWithBounds("PanelTest", gfx::Rect(0, 0, 100, 100));
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));

  // Validates that no animation should be triggered when the panel is being
  // dragged.
  gfx::Point mouse_location(panel->GetBounds().origin());
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);
  panel_testing->DragTitlebar(mouse_location + gfx::Vector2d(-100, 5));
  EXPECT_FALSE(panel_testing->IsAnimatingBounds());
  panel_testing->FinishDragTitlebar();

  // Set bounds with animation.
  gfx::Rect bounds = gfx::Rect(10, 20, 150, 160);
  panel->SetPanelBounds(bounds);
  EXPECT_TRUE(panel_testing->IsAnimatingBounds());
  WaitForBoundsAnimationFinished(panel);
  EXPECT_FALSE(panel_testing->IsAnimatingBounds());
  EXPECT_EQ(bounds, panel->GetBounds());

  // Set bounds without animation.
  bounds = gfx::Rect(30, 40, 200, 220);
  panel->SetPanelBoundsInstantly(bounds);
  EXPECT_FALSE(panel_testing->IsAnimatingBounds());
  EXPECT_EQ(bounds, panel->GetBounds());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, RestoredBounds) {
  Panel* panel = CreatePanelWithBounds("PanelTest", gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());
  EXPECT_EQ(panel->GetBounds(), panel->GetRestoredBounds());

  panel->SetExpansionState(Panel::MINIMIZED);
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());
  gfx::Rect bounds = panel->GetBounds();
  gfx::Rect restored = panel->GetRestoredBounds();
  EXPECT_EQ(bounds.x(), restored.x());
  EXPECT_GT(bounds.y(), restored.y());
  EXPECT_EQ(bounds.width(), restored.width());
  EXPECT_LT(bounds.height(), restored.height());

  panel->SetExpansionState(Panel::TITLE_ONLY);
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());
  bounds = panel->GetBounds();
  restored = panel->GetRestoredBounds();
  EXPECT_EQ(bounds.x(), restored.x());
  EXPECT_GT(bounds.y(), restored.y());
  EXPECT_EQ(bounds.width(), restored.width());
  EXPECT_LT(bounds.height(), restored.height());

  panel->SetExpansionState(Panel::MINIMIZED);
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());
  bounds = panel->GetBounds();
  restored = panel->GetRestoredBounds();
  EXPECT_EQ(bounds.x(), restored.x());
  EXPECT_GT(bounds.y(), restored.y());
  EXPECT_EQ(bounds.width(), restored.width());
  EXPECT_LT(bounds.height(), restored.height());

  panel->SetExpansionState(Panel::EXPANDED);
  EXPECT_EQ(panel->GetBounds(), panel->GetRestoredBounds());

  // Verify that changing the panel bounds does not affect the restored height.
  int saved_restored_height = restored.height();
  panel->SetExpansionState(Panel::MINIMIZED);
  bounds = gfx::Rect(10, 20, 300, 400);
  panel->SetPanelBounds(bounds);
  EXPECT_EQ(saved_restored_height, panel->GetRestoredBounds().height());

  panel->SetExpansionState(Panel::TITLE_ONLY);
  bounds = gfx::Rect(20, 30, 100, 200);
  panel->SetPanelBounds(bounds);
  EXPECT_EQ(saved_restored_height, panel->GetRestoredBounds().height());

  panel->SetExpansionState(Panel::EXPANDED);
  bounds = gfx::Rect(40, 60, 300, 400);
  panel->SetPanelBounds(bounds);
  EXPECT_EQ(saved_restored_height, panel->GetRestoredBounds().height());
  panel->set_full_size(bounds.size());
  EXPECT_NE(saved_restored_height, panel->GetRestoredBounds().height());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MinimizeRestore) {
  // Test with one panel.
  CreatePanelWithBounds("PanelTest1", gfx::Rect(0, 0, 100, 100));
  TestMinimizeRestore();

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MinimizeRestoreTwoPanels) {
  // Test with two panels.
  CreatePanelWithBounds("PanelTest1", gfx::Rect(0, 0, 100, 100));
  CreatePanelWithBounds("PanelTest2", gfx::Rect(0, 0, 110, 110));
  TestMinimizeRestore();

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MinimizeRestoreThreePanels) {
  // Test with three panels.
  CreatePanelWithBounds("PanelTest1", gfx::Rect(0, 0, 100, 100));
  CreatePanelWithBounds("PanelTest2", gfx::Rect(0, 0, 110, 110));
  CreatePanelWithBounds("PanelTest3", gfx::Rect(0, 0, 120, 120));
  TestMinimizeRestore();

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MinimizeRestoreButtonClick) {
  // Test with three panels.
  Panel* panel1 = CreatePanel("PanelTest1");
  Panel* panel2 = CreatePanel("PanelTest2");
  Panel* panel3 = CreatePanel("PanelTest3");
  EXPECT_FALSE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  // Click restore button on an expanded panel. Expect no change.
  panel1->OnRestoreButtonClicked(panel::NO_MODIFIER);
  EXPECT_FALSE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  // Click minimize button on an expanded panel. Only that panel will minimize.
  panel1->OnMinimizeButtonClicked(panel::NO_MODIFIER);
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  // Click minimize button on a minimized panel. Expect no change.
  panel1->OnMinimizeButtonClicked(panel::NO_MODIFIER);
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  // Minimize all panels by clicking minimize button on an expanded panel
  // with the apply-all modifier.
  panel2->OnMinimizeButtonClicked(panel::APPLY_TO_ALL);
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_TRUE(panel2->IsMinimized());
  EXPECT_TRUE(panel3->IsMinimized());

  // Click restore button on a minimized panel. Only that panel will restore.
  panel2->OnRestoreButtonClicked(panel::NO_MODIFIER);
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_TRUE(panel3->IsMinimized());

  // Restore all panels by clicking restore button on a minimized panel.
  panel3->OnRestoreButtonClicked(panel::APPLY_TO_ALL);
  EXPECT_FALSE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, RestoreAllWithTitlebarClick) {
  // Test with three panels.
  Panel* panel1 = CreatePanel("PanelTest1");
  Panel* panel2 = CreatePanel("PanelTest2");
  Panel* panel3 = CreatePanel("PanelTest3");
  EXPECT_FALSE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  scoped_ptr<NativePanelTesting> test_panel1(
      CreateNativePanelTesting(panel1));
  scoped_ptr<NativePanelTesting> test_panel2(
      CreateNativePanelTesting(panel2));
  scoped_ptr<NativePanelTesting> test_panel3(
      CreateNativePanelTesting(panel3));

  // Click on an expanded panel's titlebar using the apply-all modifier.
  // Verify expansion state is unchanged.
  test_panel2->PressLeftMouseButtonTitlebar(panel2->GetBounds().origin(),
                                            panel::APPLY_TO_ALL);
  test_panel2->ReleaseMouseButtonTitlebar(panel::APPLY_TO_ALL);
  EXPECT_FALSE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  // Click on a minimized panel's titlebar using the apply-all modifier.
  panel1->Minimize();
  panel2->Minimize();
  panel3->Minimize();
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_TRUE(panel2->IsMinimized());
  EXPECT_TRUE(panel3->IsMinimized());

  // Nothing changes until mouse is released.
  test_panel1->PressLeftMouseButtonTitlebar(panel1->GetBounds().origin(),
                                            panel::APPLY_TO_ALL);
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_TRUE(panel2->IsMinimized());
  EXPECT_TRUE(panel3->IsMinimized());
  // Verify all panels restored when mouse is released.
  test_panel1->ReleaseMouseButtonTitlebar(panel::APPLY_TO_ALL);
  EXPECT_FALSE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  // Minimize a single panel. Then click on expanded panel with apply-all
  // modifier. Verify nothing changes.
  panel1->Minimize();
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  test_panel2->PressLeftMouseButtonTitlebar(panel2->GetBounds().origin(),
                                            panel::APPLY_TO_ALL);
  test_panel2->ReleaseMouseButtonTitlebar(panel::APPLY_TO_ALL);
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  // Minimize another panel. Then click on a minimized panel with apply-all
  // modifier to restore all panels.
  panel2->Minimize();
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_TRUE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  test_panel2->PressLeftMouseButtonTitlebar(panel2->GetBounds().origin(),
                                            panel::APPLY_TO_ALL);
  test_panel2->ReleaseMouseButtonTitlebar(panel::APPLY_TO_ALL);
  EXPECT_FALSE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  // Click on the single minimized panel. Verify all are restored.
  panel1->Minimize();
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  test_panel1->PressLeftMouseButtonTitlebar(panel1->GetBounds().origin(),
                                            panel::APPLY_TO_ALL);
  test_panel1->ReleaseMouseButtonTitlebar(panel::APPLY_TO_ALL);
  EXPECT_FALSE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  // Click on the single expanded panel. Verify nothing changes.
  panel1->Minimize();
  panel3->Minimize();
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_TRUE(panel3->IsMinimized());

  test_panel2->PressLeftMouseButtonTitlebar(panel2->GetBounds().origin(),
                                            panel::APPLY_TO_ALL);
  test_panel2->ReleaseMouseButtonTitlebar(panel::APPLY_TO_ALL);
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_TRUE(panel3->IsMinimized());

  // Hover over a minimized panel and click on the titlebar while it is in
  // title-only mode. Should restore all panels.
  panel2->Minimize();
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_TRUE(panel2->IsMinimized());
  EXPECT_TRUE(panel3->IsMinimized());

  MoveMouseAndWaitForExpansionStateChange(panel2, panel2->GetBounds().origin());
  EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
  EXPECT_EQ(Panel::TITLE_ONLY, panel2->expansion_state());
  EXPECT_EQ(Panel::TITLE_ONLY, panel3->expansion_state());

  test_panel3->PressLeftMouseButtonTitlebar(panel3->GetBounds().origin(),
                                            panel::APPLY_TO_ALL);
  test_panel3->ReleaseMouseButtonTitlebar(panel::APPLY_TO_ALL);
  EXPECT_FALSE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());

  // Draw attention to a minimized panel. Click on a minimized panel that is
  // not drawing attention. Verify restore all applies without affecting
  // draw attention.
  panel1->Minimize();
  panel2->Minimize();
  panel3->Minimize();
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_TRUE(panel2->IsMinimized());
  EXPECT_TRUE(panel3->IsMinimized());

  panel1->FlashFrame(true);
  EXPECT_TRUE(panel1->IsDrawingAttention());

  test_panel2->PressLeftMouseButtonTitlebar(panel2->GetBounds().origin(),
                                            panel::APPLY_TO_ALL);
  test_panel2->ReleaseMouseButtonTitlebar(panel::APPLY_TO_ALL);
  EXPECT_FALSE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());
  EXPECT_TRUE(panel1->IsDrawingAttention());

  // Restore all panels by clicking on the minimized panel that is drawing
  // attention. Verify restore all applies and clears draw attention.
  panel1->Minimize();
  panel2->Minimize();
  panel3->Minimize();
  EXPECT_TRUE(panel1->IsMinimized());
  EXPECT_TRUE(panel2->IsMinimized());
  EXPECT_TRUE(panel3->IsMinimized());

  test_panel1->PressLeftMouseButtonTitlebar(panel1->GetBounds().origin(),
                                            panel::APPLY_TO_ALL);
  test_panel1->ReleaseMouseButtonTitlebar(panel::APPLY_TO_ALL);
  EXPECT_FALSE(panel1->IsMinimized());
  EXPECT_FALSE(panel2->IsMinimized());
  EXPECT_FALSE(panel3->IsMinimized());
  EXPECT_FALSE(panel1->IsDrawingAttention());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest,
                       MinimizeRestoreOnAutoHidingDesktopBar) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelCollection* docked_collection = panel_manager->docked_collection();
  int expected_bottom_on_expanded = docked_collection->display_area().bottom();
  int expected_bottom_on_title_only = expected_bottom_on_expanded;
  int expected_bottom_on_minimized = expected_bottom_on_expanded;

  // Turn on auto-hiding.
  static const int bottom_bar_thickness = 40;
  mock_display_settings_provider()->EnableAutoHidingDesktopBar(
      DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
      true,
      bottom_bar_thickness);
  expected_bottom_on_title_only -= bottom_bar_thickness;

  Panel* panel = CreatePanel("1");
  int initial_height = panel->GetBounds().height();

  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());
  EXPECT_EQ(expected_bottom_on_expanded, panel->GetBounds().bottom());

  panel->Minimize();
  WaitForBoundsAnimationFinished(panel);
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());
  EXPECT_EQ(panel::kMinimizedPanelHeight, panel->GetBounds().height());
  EXPECT_EQ(expected_bottom_on_minimized, panel->GetBounds().bottom());

  panel->SetExpansionState(Panel::TITLE_ONLY);
  WaitForBoundsAnimationFinished(panel);
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());
  EXPECT_EQ(panel::kTitlebarHeight, panel->GetBounds().height());
  EXPECT_EQ(expected_bottom_on_title_only, panel->GetBounds().bottom());

  panel->Restore();
  WaitForBoundsAnimationFinished(panel);
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());
  EXPECT_EQ(initial_height, panel->GetBounds().height());
  EXPECT_EQ(expected_bottom_on_expanded, panel->GetBounds().bottom());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, ChangeAutoHideTaskBarThickness) {
  PanelManager* manager = PanelManager::GetInstance();
  DockedPanelCollection* docked_collection = manager->docked_collection();
  int initial_starting_right_position =
      docked_collection->StartingRightPosition();

  int bottom_bar_thickness = 20;
  int right_bar_thickness = 30;
  mock_display_settings_provider()->EnableAutoHidingDesktopBar(
      DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
      true,
      bottom_bar_thickness);
  mock_display_settings_provider()->EnableAutoHidingDesktopBar(
      DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_RIGHT,
      true,
      right_bar_thickness);
  EXPECT_EQ(initial_starting_right_position -
                docked_collection->StartingRightPosition(),
            right_bar_thickness);

  Panel* panel = CreatePanel("PanelTest");
  panel->SetExpansionState(Panel::TITLE_ONLY);
  WaitForBoundsAnimationFinished(panel);

  EXPECT_EQ(docked_collection->display_area().bottom() - bottom_bar_thickness,
            panel->GetBounds().bottom());
  EXPECT_EQ(docked_collection->StartingRightPosition(),
            panel->GetBounds().right());

  initial_starting_right_position = docked_collection->StartingRightPosition();
  int bottom_bar_thickness_delta = 10;
  bottom_bar_thickness += bottom_bar_thickness_delta;
  int right_bar_thickness_delta = 15;
  right_bar_thickness += right_bar_thickness_delta;
  mock_display_settings_provider()->SetDesktopBarThickness(
      DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
      bottom_bar_thickness);
  mock_display_settings_provider()->SetDesktopBarThickness(
      DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_RIGHT,
      right_bar_thickness);
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(initial_starting_right_position -
                docked_collection->StartingRightPosition(),
            right_bar_thickness_delta);
  EXPECT_EQ(docked_collection->display_area().bottom() - bottom_bar_thickness,
            panel->GetBounds().bottom());
  EXPECT_EQ(docked_collection->StartingRightPosition(),
            panel->GetBounds().right());

  initial_starting_right_position = docked_collection->StartingRightPosition();
  bottom_bar_thickness_delta = 20;
  bottom_bar_thickness -= bottom_bar_thickness_delta;
  right_bar_thickness_delta = 10;
  right_bar_thickness -= right_bar_thickness_delta;
  mock_display_settings_provider()->SetDesktopBarThickness(
      DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
      bottom_bar_thickness);
  mock_display_settings_provider()->SetDesktopBarThickness(
      DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_RIGHT,
      right_bar_thickness);
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(docked_collection->StartingRightPosition() -
                initial_starting_right_position,
            right_bar_thickness_delta);
  EXPECT_EQ(docked_collection->display_area().bottom() - bottom_bar_thickness,
            panel->GetBounds().bottom());
  EXPECT_EQ(docked_collection->StartingRightPosition(),
            panel->GetBounds().right());

  panel->Close();
}

// http://crbug.com/143247
#if !defined(OS_WIN)
#define MAYBE_ActivatePanelOrTabbedWindow DISABLED_ActivatePanelOrTabbedWindow
#else
#define MAYBE_ActivatePanelOrTabbedWindow ActivatePanelOrTabbedWindow
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_ActivatePanelOrTabbedWindow) {
  Panel* panel1 = CreatePanel("Panel1");
  Panel* panel2 = CreatePanel("Panel2");

  // Activate main tabbed window.
  browser()->window()->Activate();
  WaitForPanelActiveState(panel2, SHOW_AS_INACTIVE);

  // Activate a panel.
  panel2->Activate();
  WaitForPanelActiveState(panel2, SHOW_AS_ACTIVE);

  // Activate the main tabbed window back.
  browser()->window()->Activate();
  WaitForPanelActiveState(panel2, SHOW_AS_INACTIVE);

  // Activate another panel.
  panel1->Activate();
  WaitForPanelActiveState(panel1, SHOW_AS_ACTIVE);
  WaitForPanelActiveState(panel2, SHOW_AS_INACTIVE);

  // Switch focus between panels.
  panel2->Activate();
  WaitForPanelActiveState(panel2, SHOW_AS_ACTIVE);
  WaitForPanelActiveState(panel1, SHOW_AS_INACTIVE);

  PanelManager::GetInstance()->CloseAll();
}

// TODO(jianli): To be enabled for other platforms.
#if defined(OS_WIN)
#define MAYBE_ActivateDeactivateBasic ActivateDeactivateBasic
#else
#define MAYBE_ActivateDeactivateBasic DISABLED_ActivateDeactivateBasic
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_ActivateDeactivateBasic) {
  // Create an active panel.
  Panel* panel = CreatePanel("PanelTest");
  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));

  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);  // doublecheck active state
  EXPECT_TRUE(native_panel_testing->VerifyActiveState(true));

  // Deactivate the panel.
  panel->Deactivate();
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);
  EXPECT_TRUE(native_panel_testing->VerifyActiveState(false));

  // This test does not reactivate the panel because the panel might not be
  // reactivated programmatically once it is deactivated.
}

// http://crbug.com/143247
#if !defined(OS_WIN)
#define MAYBE_ActivateDeactivateMultiple DISABLED_ActivateDeactivateMultiple
#else
#define MAYBE_ActivateDeactivateMultiple ActivateDeactivateMultiple
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_ActivateDeactivateMultiple) {
  BrowserWindow* tabbed_window = browser()->window();

  // Create 4 panels in the following screen layout:
  //    P3  P2  P1  P0
  const int kNumPanels = 4;
  for (int i = 0; i < kNumPanels; ++i)
    CreatePanelWithBounds(MakePanelName(i), gfx::Rect(0, 0, 100, 100));
  const std::vector<Panel*>& panels = PanelManager::GetInstance()->panels();

  std::vector<bool> expected_active_states;
  std::vector<bool> last_active_states;

  // The last created panel, P3, should be active.
  expected_active_states = ProduceExpectedActiveStates(3);
  EXPECT_EQ(expected_active_states, GetAllPanelActiveStates());
  EXPECT_FALSE(tabbed_window->IsActive());

  // Activating P1 should cause P3 to lose focus.
  panels[1]->Activate();
  last_active_states = expected_active_states;
  expected_active_states = ProduceExpectedActiveStates(1);
  WaitForPanelActiveStates(last_active_states, expected_active_states);
  EXPECT_EQ(expected_active_states, GetAllPanelActiveStates());

  // Minimizing inactive panel P2 should not affect other panels' active states.
  panels[2]->SetExpansionState(Panel::MINIMIZED);
  EXPECT_EQ(expected_active_states, GetAllPanelActiveStates());
  EXPECT_FALSE(tabbed_window->IsActive());
}

// http://crbug.com/143247
#if !defined(OS_WIN)
#define MAYBE_DrawAttentionBasic DISABLED_DrawAttentionBasic
#else
#define MAYBE_DrawAttentionBasic DrawAttentionBasic
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_DrawAttentionBasic) {
  CreatePanelParams params("Initially Inactive", gfx::Rect(), SHOW_AS_INACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));

  // Test that the attention is drawn when the expanded panel is not in focus.
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());
  EXPECT_FALSE(panel->IsActive());
  EXPECT_FALSE(panel->IsDrawingAttention());
  panel->FlashFrame(true);
  EXPECT_TRUE(panel->IsDrawingAttention());
  EXPECT_TRUE(native_panel_testing->VerifyDrawingAttention());

  // Stop drawing attention.
  panel->FlashFrame(false);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  // Draw attention, then minimize. Titlebar should remain visible.
  panel->FlashFrame(true);
  EXPECT_TRUE(panel->IsDrawingAttention());

  panel->Minimize();
  EXPECT_TRUE(panel->IsDrawingAttention());
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

  // Stop drawing attention. Titlebar should no longer be visible.
  panel->FlashFrame(false);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());

  panel->Close();
}

// http://crbug.com/143247
#if !defined(OS_WIN)
#define MAYBE_DrawAttentionWhileMinimized DISABLED_DrawAttentionWhileMinimized
#else
#define MAYBE_DrawAttentionWhileMinimized DrawAttentionWhileMinimized
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_DrawAttentionWhileMinimized) {
  // Create 3 panels so we end up with an inactive panel that can
  // be made to draw attention.
  Panel* panel = CreatePanel("test panel1");
  Panel* panel2 = CreatePanel("test panel2");
  Panel* panel3 = CreatePanel("test panel3");

  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));

  // Test that the attention is drawn and the title-bar is brought up when the
  // minimized panel is drawing attention.
  panel->Minimize();
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());
  panel->FlashFrame(true);
  EXPECT_TRUE(panel->IsDrawingAttention());
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());
  EXPECT_TRUE(native_panel_testing->VerifyDrawingAttention());

  // Test that we cannot bring up other minimized panel if the mouse is over
  // the panel that draws attension.
  panel2->Minimize();
  gfx::Point hover_point(panel->GetBounds().origin());
  MoveMouse(hover_point);
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());
  EXPECT_EQ(Panel::MINIMIZED, panel2->expansion_state());

  // Test that we cannot bring down the panel that is drawing the attention.
  hover_point.set_y(hover_point.y() - 200);
  MoveMouse(hover_point);
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

  // Test that the attention is cleared when activated.
  panel->Activate();
  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  panel->Close();
  panel2->Close();
  panel3->Close();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_StopDrawingAttentionWhileMinimized \
  DISABLED_StopDrawingAttentionWhileMinimized
#else
#define MAYBE_StopDrawingAttentionWhileMinimized \
  StopDrawingAttentionWhileMinimized
#endif
// Verify that minimized state of a panel is correct after draw attention
// is stopped when there are other minimized panels.
IN_PROC_BROWSER_TEST_F(PanelBrowserTest,
                       MAYBE_StopDrawingAttentionWhileMinimized) {
  Panel* panel1 = CreatePanel("panel1");
  Panel* panel2 = CreatePanel("panel2");

  panel1->Minimize();
  EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());
  panel2->Minimize();
  EXPECT_EQ(Panel::MINIMIZED, panel2->expansion_state());

  // Verify panel returns to minimized state when no longer drawing attention.
  panel1->FlashFrame(true);
  EXPECT_TRUE(panel1->IsDrawingAttention());
  EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());

  panel1->FlashFrame(false);
  EXPECT_FALSE(panel1->IsDrawingAttention());
  EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());

  // Hover over other minimized panel to bring up titlebars.
  gfx::Point hover_point(panel2->GetBounds().origin());
  MoveMouseAndWaitForExpansionStateChange(panel1, hover_point);
  EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
  EXPECT_EQ(Panel::TITLE_ONLY, panel2->expansion_state());

  // Verify panel keeps titlebar visible when no longer drawing attention
  // if titlebars are up.
  panel1->FlashFrame(true);
  EXPECT_TRUE(panel1->IsDrawingAttention());
  EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());

  panel1->FlashFrame(false);
  EXPECT_FALSE(panel1->IsDrawingAttention());
  EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());

  // Move mouse away. All panels should return to minimized state.
  hover_point.set_y(hover_point.y() - 200);
  MoveMouseAndWaitForExpansionStateChange(panel1, hover_point);
  EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());
  EXPECT_EQ(Panel::MINIMIZED, panel2->expansion_state());

  // Verify minimized panel that is drawing attention stays in title-only mode
  // after attention is cleared if mouse is in the titlebar area.
  panel1->FlashFrame(true);
  EXPECT_TRUE(panel1->IsDrawingAttention());
  EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());

  gfx::Point hover_point_in_panel(panel1->GetBounds().origin());
  MoveMouse(hover_point_in_panel);

  panel1->FlashFrame(false);
  EXPECT_FALSE(panel1->IsDrawingAttention());
  EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
  EXPECT_EQ(Panel::MINIMIZED, panel2->expansion_state());

  // Typical user scenario will detect the mouse in the panel
  // after attention is cleared, causing titles to pop up, so
  // we simulate that here.
  MoveMouseAndWaitForExpansionStateChange(panel2, hover_point_in_panel);
  EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
  EXPECT_EQ(Panel::TITLE_ONLY, panel2->expansion_state());

  // Move mouse away and panels should go back to fully minimized state.
  MoveMouseAndWaitForExpansionStateChange(panel1, hover_point);
  EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());
  EXPECT_EQ(Panel::MINIMIZED, panel2->expansion_state());

  panel1->Close();
  panel2->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, DrawAttentionWhenActive) {
  CreatePanelParams params("Initially Active", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));

  // Test that the attention should not be drawn if the expanded panel is in
  // focus.
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());
  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);  // doublecheck active state
  EXPECT_FALSE(panel->IsDrawingAttention());
  panel->FlashFrame(true);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  panel->Close();
}

// http://crbug.com/143247
#if !defined(OS_WIN)
#define MAYBE_DrawAttentionResetOnActivate DISABLED_DrawAttentionResetOnActivate
#else
#define MAYBE_DrawAttentionResetOnActivate DrawAttentionResetOnActivate
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_DrawAttentionResetOnActivate) {
  // Create 2 panels so we end up with an inactive panel that can
  // be made to draw attention.
  Panel* panel = CreatePanel("test panel1");
  Panel* panel2 = CreatePanel("test panel2");
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);

  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));

  panel->FlashFrame(true);
  EXPECT_TRUE(panel->IsDrawingAttention());
  EXPECT_TRUE(native_panel_testing->VerifyDrawingAttention());

  // Test that the attention is cleared when panel gets focus.
  panel->Activate();
  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  panel->Close();
  panel2->Close();
}

// http://crbug.com/143247
#if !defined(OS_WIN)
#define MAYBE_DrawAttentionMinimizedNotResetOnActivate DISABLED_DrawAttentionMinimizedNotResetOnActivate
#else
#define MAYBE_DrawAttentionMinimizedNotResetOnActivate DrawAttentionMinimizedNotResetOnActivate
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest,
                       MAYBE_DrawAttentionMinimizedNotResetOnActivate) {
  // Create 2 panels so we end up with an inactive panel that can
  // be made to draw attention.
  Panel* panel1 = CreatePanel("test panel1");
  Panel* panel2 = CreatePanel("test panel2");
  WaitForPanelActiveState(panel1, SHOW_AS_INACTIVE);

  panel1->Minimize();
  EXPECT_TRUE(panel1->IsMinimized());
  panel1->FlashFrame(true);
  EXPECT_TRUE(panel1->IsDrawingAttention());

  // Simulate panel being activated while minimized. Cannot call
  // Activate() as that expands the panel.
  panel1->OnActiveStateChanged(true);
  EXPECT_TRUE(panel1->IsDrawingAttention());  // Unchanged.

  // Unminimize panel to show that attention would have been cleared
  // if panel had not been minimized.
  panel1->Restore();
  EXPECT_FALSE(panel1->IsMinimized());
  EXPECT_TRUE(panel1->IsDrawingAttention());  // Unchanged.

  panel1->OnActiveStateChanged(true);
  EXPECT_FALSE(panel1->IsDrawingAttention());  // Attention cleared.

  panel1->Close();
  panel2->Close();
}

// http://crbug.com/143247
#if !defined(OS_WIN)
#define MAYBE_DrawAttentionResetOnClick DISABLED_DrawAttentionResetOnClick
#else
#define MAYBE_DrawAttentionResetOnClick DrawAttentionResetOnClick
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_DrawAttentionResetOnClick) {
  // Create 2 panels so we end up with an inactive panel that can
  // be made to draw attention.
  Panel* panel = CreatePanel("test panel1");
  Panel* panel2 = CreatePanel("test panel2");
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);

  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));

  panel->FlashFrame(true);
  EXPECT_TRUE(panel->IsDrawingAttention());
  EXPECT_TRUE(native_panel_testing->VerifyDrawingAttention());

  // Test that the attention is cleared when panel gets focus.
  native_panel_testing->PressLeftMouseButtonTitlebar(
      panel->GetBounds().origin());
  native_panel_testing->ReleaseMouseButtonTitlebar();

  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  panel->Close();
  panel2->Close();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_MinimizeImmediatelyAfterRestore \
  DISABLED_MinimizeImmediatelyAfterRestore
#else
#define MAYBE_MinimizeImmediatelyAfterRestore MinimizeImmediatelyAfterRestore
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest,
                       MAYBE_MinimizeImmediatelyAfterRestore) {
  CreatePanelParams params("Panel Test", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));

  PanelActiveStateObserver signal(panel, false);
  panel->Minimize();  // this should deactivate.
  signal.Wait();
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());

  panel->Restore();
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());

  // Verify that minimizing a panel right after expansion works.
  panel->Minimize();
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, FocusLostOnMinimize) {
  CreatePanelParams params("Initially Active", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());

  PanelActiveStateObserver signal(panel, false);
  panel->Minimize();
  signal.Wait();
  panel->Close();
}

// http://crbug.com/143247
#if !defined(OS_WIN)
#define MAYBE_CreateInactiveSwitchToActive DISABLED_CreateInactiveSwitchToActive
#else
#define MAYBE_CreateInactiveSwitchToActive CreateInactiveSwitchToActive
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_CreateInactiveSwitchToActive) {
  // Compiz will not activate initially inactive window.
  if (SkipTestIfCompizWM())
    return;

  CreatePanelParams params("Initially Inactive", gfx::Rect(), SHOW_AS_INACTIVE);
  Panel* panel = CreatePanelWithParams(params);

  panel->Activate();
  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);

  panel->Close();
}

// TODO(dimich): try/enable on other platforms. See bug 103253 for details on
// why this is disabled on windows.
#if defined(OS_MACOSX)
#define MAYBE_MinimizeTwoPanelsWithoutTabbedWindow \
    MinimizeTwoPanelsWithoutTabbedWindow
#else
#define MAYBE_MinimizeTwoPanelsWithoutTabbedWindow \
    DISABLED_MinimizeTwoPanelsWithoutTabbedWindow
#endif

// When there are 2 panels and no chrome window, minimizing one panel does
// not expand/focuses another.
IN_PROC_BROWSER_TEST_F(PanelBrowserTest,
                       MAYBE_MinimizeTwoPanelsWithoutTabbedWindow) {
  CreatePanelParams params("Initially Inactive", gfx::Rect(), SHOW_AS_INACTIVE);
  Panel* panel1 = CreatePanelWithParams(params);
  Panel* panel2 = CreatePanelWithParams(params);

  // Close main tabbed window.
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser()));
  chrome::CloseWindow(browser());
  signal.Wait();

  EXPECT_EQ(Panel::EXPANDED, panel1->expansion_state());
  EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());
  panel1->Activate();
  WaitForPanelActiveState(panel1, SHOW_AS_ACTIVE);

  panel1->SetExpansionState(Panel::MINIMIZED);
  MessageLoop::current()->RunUntilIdle();
  WaitForPanelActiveState(panel1, SHOW_AS_INACTIVE);
  EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());

  panel2->SetExpansionState(Panel::MINIMIZED);
  MessageLoop::current()->RunUntilIdle();
  WaitForPanelActiveState(panel2, SHOW_AS_INACTIVE);
  EXPECT_EQ(Panel::MINIMIZED, panel2->expansion_state());

  // Verify that panel1 is still minimized and not active.
  WaitForPanelActiveState(panel1, SHOW_AS_INACTIVE);
  EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());

  // Another check for the same.
  EXPECT_FALSE(panel1->IsActive());
  EXPECT_FALSE(panel2->IsActive());

  panel1->Close();
  panel2->Close();
}

// http://crbug.com/143247
#if !defined(OS_WIN)
#define MAYBE_NonExtensionDomainPanelsCloseOnUninstall DISABLED_NonExtensionDomainPanelsCloseOnUninstall
#else
#define MAYBE_NonExtensionDomainPanelsCloseOnUninstall NonExtensionDomainPanelsCloseOnUninstall
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest,
                       MAYBE_NonExtensionDomainPanelsCloseOnUninstall) {
  // Create a test extension.
  DictionaryValue empty_value;
  scoped_refptr<extensions::Extension> extension =
      CreateExtension(FILE_PATH_LITERAL("TestExtension"),
                      extensions::Manifest::INVALID_LOCATION, empty_value);
  std::string extension_app_name =
      web_app::GenerateApplicationNameFromExtensionId(extension->id());

  PanelManager* panel_manager = PanelManager::GetInstance();
  EXPECT_EQ(0, panel_manager->num_panels());

  // Create a panel with the extension as host.
  CreatePanelParams params(extension_app_name, gfx::Rect(), SHOW_AS_INACTIVE);
  std::string extension_domain_url(extensions::kExtensionScheme);
  extension_domain_url += "://";
  extension_domain_url += extension->id();
  extension_domain_url += "/hello.html";
  params.url = GURL(extension_domain_url);
  Panel* panel = CreatePanelWithParams(params);
  EXPECT_EQ(1, panel_manager->num_panels());

  // Create a panel with a non-extension host.
  CreatePanelParams params1(extension_app_name, gfx::Rect(), SHOW_AS_INACTIVE);
  params1.url = GURL(chrome::kAboutBlankURL);
  Panel* panel1 = CreatePanelWithParams(params1);
  EXPECT_EQ(2, panel_manager->num_panels());

  // Create another extension and a panel from that extension.
  scoped_refptr<extensions::Extension> extension_other =
      CreateExtension(FILE_PATH_LITERAL("TestExtensionOther"),
                      extensions::Manifest::INVALID_LOCATION, empty_value);
  std::string extension_app_name_other =
      web_app::GenerateApplicationNameFromExtensionId(extension_other->id());
  Panel* panel_other = CreatePanel(extension_app_name_other);

  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_PANEL_CLOSED,
      content::Source<Panel>(panel));
  content::WindowedNotificationObserver signal1(
      chrome::NOTIFICATION_PANEL_CLOSED,
      content::Source<Panel>(panel1));

  // Send unload notification on the first extension.
  extensions::UnloadedExtensionInfo details(extension,
                                extension_misc::UNLOAD_REASON_UNINSTALL);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::Source<Profile>(browser()->profile()),
      content::Details<extensions::UnloadedExtensionInfo>(&details));

  // Wait for the panels opened by the first extension to close.
  signal.Wait();
  signal1.Wait();

  // Verify that the panel that's left is the panel from the second extension.
  EXPECT_EQ(panel_other, panel_manager->panels()[0]);
  panel_other->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, OnBeforeUnloadOnClose) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  EXPECT_EQ(0, panel_manager->num_panels()); // No panels initially.

  const string16 title_first_close = UTF8ToUTF16("TitleFirstClose");
  const string16 title_second_close = UTF8ToUTF16("TitleSecondClose");

  // Create a test panel with web contents loaded.
  CreatePanelParams params("PanelTest1", gfx::Rect(0, 0, 300, 300),
                           SHOW_AS_ACTIVE);
  params.url = ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("onbeforeunload.html")));
  Panel* panel = CreatePanelWithParams(params);
  EXPECT_EQ(1, panel_manager->num_panels());

  // Close panel and verify it closes despite having a onbeforeunload handler.
  CloseWindowAndWait(panel);
  EXPECT_EQ(0, panel_manager->num_panels());
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, SizeClamping) {
  // Using '0' sizes is equivalent of not providing sizes in API and causes
  // minimum sizes to be applied to facilitate auto-sizing.
  CreatePanelParams params("Panel", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  EXPECT_EQ(panel->min_size().width(), panel->GetBounds().width());
  EXPECT_EQ(panel->min_size().height(), panel->GetBounds().height());
  int reasonable_width = panel->min_size().width() + 10;
  int reasonable_height = panel->min_size().height() + 20;

  panel->Close();

  // Using reasonable actual sizes should avoid clamping.
  CreatePanelParams params1("Panel1",
                            gfx::Rect(0, 0,
                                      reasonable_width, reasonable_height),
                            SHOW_AS_ACTIVE);
  panel = CreatePanelWithParams(params1);
  EXPECT_EQ(reasonable_width, panel->GetBounds().width());
  EXPECT_EQ(reasonable_height, panel->GetBounds().height());
  panel->Close();

  // Using just one size should auto-compute some reasonable other size.
  int given_height = 200;
  CreatePanelParams params2("Panel2", gfx::Rect(0, 0, 0, given_height),
                            SHOW_AS_ACTIVE);
  panel = CreatePanelWithParams(params2);
  EXPECT_GT(panel->GetBounds().width(), 0);
  EXPECT_EQ(given_height, panel->GetBounds().height());
  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest,
                       TightAutosizeAroundSingleLine) {
  PanelManager::GetInstance()->enable_auto_sizing(true);
  // Using 0 sizes triggers auto-sizing.
  CreatePanelParams params("Panel", gfx::Rect(), SHOW_AS_ACTIVE);
  params.url = GURL("data:text/html;charset=utf-8,<!doctype html><body>");
  Panel* panel = CreatePanelWithParams(params);

  // Ensure panel has auto resized to original web content size.
  WaitForStableInitialSize initial_resize(panel);
  initial_resize.Wait();

  int initial_width = panel->GetBounds().width();
  int initial_height = panel->GetBounds().height();

  // Inject some HTML content into the panel.
  WaitForAutoResizeWider enlarge(panel);
  EXPECT_TRUE(content::ExecuteScript(
      panel->GetWebContents(),
      "document.body.innerHTML ="
      "    '<nobr>line of text and a <button>Button</button>';"));
  enlarge.Wait();

  // The panel should have become larger in both dimensions (the minimums
  // has to be set to be smaller then a simple 1-line content, so the autosize
  // can work correctly.
  EXPECT_GT(panel->GetBounds().width(), initial_width);
  EXPECT_GT(panel->GetBounds().height(), initial_height);

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest,
                       DefaultMaxSizeOnDisplaySettingsChange) {
  Panel* panel = CreatePanelWithBounds("1", gfx::Rect(0, 0, 240, 220));

  gfx::Size old_max_size = panel->max_size();
  gfx::Size old_full_size = panel->full_size();

  // Shrink the work area. Expect max size and full size become smaller.
  gfx::Size smaller_work_area_size = gfx::Size(500, 300);
  SetTestingAreas(gfx::Rect(gfx::Point(0, 0), smaller_work_area_size),
                  gfx::Rect());
  EXPECT_GT(old_max_size.width(), panel->max_size().width());
  EXPECT_GT(old_max_size.height(), panel->max_size().height());
  EXPECT_GT(smaller_work_area_size.width(), panel->max_size().width());
  EXPECT_GT(smaller_work_area_size.height(), panel->max_size().height());
  EXPECT_GT(old_full_size.width(), panel->full_size().width());
  EXPECT_GT(old_full_size.height(), panel->full_size().height());
  EXPECT_GE(panel->max_size().width(), panel->full_size().width());
  EXPECT_GE(panel->max_size().height(), panel->full_size().height());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest,
                       CustomMaxSizeOnDisplaySettingsChange) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  Panel* panel = CreatePanelWithBounds("1", gfx::Rect(0, 0, 240, 220));

  // Trigger custom max size by user resizing.
  gfx::Size bigger_size = gfx::Size(550, 400);
  gfx::Point mouse_location = panel->GetBounds().origin();
  panel_manager->StartResizingByMouse(panel,
                                      mouse_location,
                                      panel::RESIZE_TOP_LEFT);
  mouse_location.Offset(panel->GetBounds().width() - bigger_size.width(),
                        panel->GetBounds().height() - bigger_size.height());
  panel_manager->ResizeByMouse(mouse_location);
  panel_manager->EndResizingByMouse(false);

  gfx::Size old_max_size = panel->max_size();
  EXPECT_EQ(bigger_size, old_max_size);
  gfx::Size old_full_size = panel->full_size();
  EXPECT_EQ(bigger_size, old_full_size);

  // Shrink the work area. Expect max size and full size become smaller.
  gfx::Size smaller_work_area_size = gfx::Size(500, 300);
  SetTestingAreas(gfx::Rect(gfx::Point(0, 0), smaller_work_area_size),
                  gfx::Rect());
  EXPECT_GT(old_max_size.width(), panel->max_size().width());
  EXPECT_GT(old_max_size.height(), panel->max_size().height());
  EXPECT_GE(smaller_work_area_size.width(), panel->max_size().width());
  EXPECT_EQ(smaller_work_area_size.height(), panel->max_size().height());
  EXPECT_GT(old_full_size.width(), panel->full_size().width());
  EXPECT_GT(old_full_size.height(), panel->full_size().height());
  EXPECT_GE(panel->max_size().width(), panel->full_size().width());
  EXPECT_GE(panel->max_size().height(), panel->full_size().height());
  EXPECT_EQ(smaller_work_area_size.height(), panel->full_size().height());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, DevTools) {
  // Create a test panel with web contents loaded.
  CreatePanelParams params("1", gfx::Rect(0, 0, 200, 220), SHOW_AS_ACTIVE);
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("update-preferred-size.html"))));
  params.url = url;
  Panel* panel = CreatePanelWithParams(params);

  // Open devtools.
  size_t num_browsers = 1;
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(browser()->profile()));
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::NotificationService::AllSources());
  EXPECT_TRUE(panel->ExecuteCommandIfEnabled(IDC_DEV_TOOLS));
  signal.Wait();

  // Check that the new browser window that opened is dev tools window.
  ++num_browsers;
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(browser()->profile()));
  for (chrome::BrowserIterator iter; !iter.done(); iter.Next()) {
    if (*iter == browser())
      continue;
    ASSERT_TRUE((*iter)->is_devtools());
  }

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, DevToolsConsole) {
  // Create a test panel with web contents loaded.
  CreatePanelParams params("1", gfx::Rect(0, 0, 200, 220), SHOW_AS_ACTIVE);
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("update-preferred-size.html"))));
  params.url = url;
  Panel* panel = CreatePanelWithParams(params);

  // Open devtools console.
  size_t num_browsers = 1;
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(browser()->profile()));
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::NotificationService::AllSources());
  EXPECT_TRUE(panel->ExecuteCommandIfEnabled(IDC_DEV_TOOLS_CONSOLE));
  signal.Wait();

  // Check that the new browser window that opened is dev tools window.
  ++num_browsers;
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(browser()->profile()));
  for (chrome::BrowserIterator iter; !iter.done(); iter.Next()) {
    if (*iter == browser())
      continue;
    ASSERT_TRUE((*iter)->is_devtools());
  }

  panel->Close();
}

#if defined(OS_WIN)
#define MAYBE_Accelerator Accelerator
#else
#define MAYBE_Accelerator DISABLED_Accelerator
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_Accelerator) {
  PanelManager* panel_manager = PanelManager::GetInstance();

  // Create a test panel with web contents loaded.
  CreatePanelParams params("1", gfx::Rect(), SHOW_AS_ACTIVE);
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("update-preferred-size.html"))));
  params.url = url;
  Panel* panel = CreatePanelWithParams(params);
  EXPECT_EQ(1, panel_manager->num_panels());

  // Close the panel by acclerator.
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_PANEL_CLOSED,
      content::Source<Panel>(panel));
#if defined(USE_AURA)
  double now = ui::EventTimeForNow().InSecondsF();
  content::NativeWebKeyboardEvent key_event(
      ui::ET_KEY_PRESSED,
      false,
      ui::VKEY_W,
      ui::EF_CONTROL_DOWN,
      now);
#elif defined(OS_WIN)
  ::MSG key_msg = { NULL, WM_KEYDOWN, ui::VKEY_W, 0 };
  content::NativeWebKeyboardEvent key_event(key_msg);
  key_event.modifiers = content::NativeWebKeyboardEvent::ControlKey;
#else
  content::NativeWebKeyboardEvent key_event;
#endif
  panel->HandleKeyboardEvent(key_event);
  signal.Wait();
  EXPECT_EQ(0, panel_manager->num_panels());
}

class PanelExtensionApiTest : public ExtensionApiTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
  }
};

#if defined(OS_LINUX) || (!defined(OS_WIN) && defined(USE_AURA))
// Focus test fails if there is no window manager on Linux.
// Aura panels have different behavior that do not apply to this test.
#define MAYBE_FocusChangeEventOnMinimize DISABLED_FocusChangeEventOnMinimize
#else
#define MAYBE_FocusChangeEventOnMinimize FocusChangeEventOnMinimize
#endif
IN_PROC_BROWSER_TEST_F(PanelExtensionApiTest,
                       MAYBE_FocusChangeEventOnMinimize) {
  // This is needed so the subsequently created panels can be activated.
  // On a Mac, it transforms background-only test process into foreground one.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(RunExtensionTest("panels/focus_change_on_minimize")) << message_;
}
