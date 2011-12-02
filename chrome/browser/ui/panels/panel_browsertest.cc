// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/notifications/balloon_collection_impl.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_settings_menu_model.h"
#include "chrome/browser/ui/panels/test_panel_mouse_watcher.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/net/url_request_mock_http_job.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/show_desktop_notification_params.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/screen.h"

using content::BrowserThread;

class PanelBrowserTest : public BasePanelBrowserTest {
 public:
  PanelBrowserTest() : BasePanelBrowserTest() {
  }

 protected:
  void CloseWindowAndWait(Browser* browser) {
    // Closing a browser window may involve several async tasks. Need to use
    // message pump and wait for the notification.
    size_t browser_count = BrowserList::size();
    ui_test_utils::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::Source<Browser>(browser));
    browser->CloseWindow();
    signal.Wait();
    // Now we have one less browser instance.
    EXPECT_EQ(browser_count - 1, BrowserList::size());
  }

  void MoveMouse(const gfx::Point& position) {
    PanelManager::GetInstance()->mouse_watcher()->NotifyMouseMovement(position);
    MessageLoopForUI::current()->RunAllPending();
  }

  void MoveMouseAndWaitForExpansionStateChange(Panel* panel,
                                               const gfx::Point& position) {
    ui_test_utils::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_PANEL_CHANGED_EXPANSION_STATE,
        content::Source<Panel>(panel));
    MoveMouse(position);
    signal.Wait();
  }

  void TestCreatePanelOnOverflow() {
    PanelManager* panel_manager = PanelManager::GetInstance();
    EXPECT_EQ(0, panel_manager->num_panels()); // No panels initially.

    // Create testing extensions.
    DictionaryValue empty_value;
    scoped_refptr<Extension> extension1 =
        CreateExtension(FILE_PATH_LITERAL("extension1"),
        Extension::INVALID, empty_value);
    scoped_refptr<Extension> extension2 =
        CreateExtension(FILE_PATH_LITERAL("extension2"),
        Extension::INVALID, empty_value);
    scoped_refptr<Extension> extension3 =
        CreateExtension(FILE_PATH_LITERAL("extension3"),
        Extension::INVALID, empty_value);

    // First, create 3 panels.
    Panel* panel1 = CreatePanelWithBounds(
        web_app::GenerateApplicationNameFromExtensionId(extension1->id()),
        gfx::Rect(0, 0, 250, 200));
    Panel* panel2 = CreatePanelWithBounds(
        web_app::GenerateApplicationNameFromExtensionId(extension2->id()),
        gfx::Rect(0, 0, 300, 200));
    Panel* panel3 = CreatePanelWithBounds(
        web_app::GenerateApplicationNameFromExtensionId(extension1->id()),
        gfx::Rect(0, 0, 200, 200));
    ASSERT_EQ(3, panel_manager->num_panels());

    // Open a panel that would overflow.
    Panel* panel4 = CreatePanelWithBounds(
        web_app::GenerateApplicationNameFromExtensionId(extension2->id()),
        gfx::Rect(0, 0, 280, 200));
    ASSERT_EQ(4, panel_manager->num_panels());
    EXPECT_LT(panel4->GetBounds().right(), panel3->GetBounds().x());
    EXPECT_GT(0, panel4->GetBounds().x());

    // Open another panel that would overflow.
    Panel* panel5 = CreatePanelWithBounds(
        web_app::GenerateApplicationNameFromExtensionId(extension3->id()),
        gfx::Rect(0, 0, 300, 200));
    ASSERT_EQ(5, panel_manager->num_panels());
    EXPECT_LT(panel5->GetBounds().right(), panel4->GetBounds().x());
    EXPECT_GT(0, panel5->GetBounds().x());

    // Close a visible panel. Expect an overflow panel to slide over.
    CloseWindowAndWait(panel2->browser());
    ASSERT_EQ(4, panel_manager->num_panels());
    EXPECT_LT(panel4->GetBounds().right(), panel3->GetBounds().x());
    EXPECT_LE(0, panel4->GetBounds().x());
    EXPECT_GT(0, panel5->GetBounds().x());

    // Close another visible panel. Remaining overflow panel should slide over
    // but still not enough room to be fully visible.
    CloseWindowAndWait(panel3->browser());
    ASSERT_EQ(3, panel_manager->num_panels());
    EXPECT_LT(panel5->GetBounds().right(), panel4->GetBounds().x());
    EXPECT_GT(0, panel5->GetBounds().x());

    // Closing one more panel makes room for all panels to fit on screen.
    CloseWindowAndWait(panel4->browser());
    ASSERT_EQ(2, panel_manager->num_panels());
    EXPECT_LT(panel5->GetBounds().right(), panel1->GetBounds().x());
    EXPECT_LE(0, panel5->GetBounds().x());

    panel1->Close();
    panel5->Close();
  }

  int horizontal_spacing() {
    return PanelManager::horizontal_spacing();
  }

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

  // This is a bit mask - a set of flags that controls the specific drag actions
  // to be carried out by TestDragging function.
  enum DragAction {
    DRAG_ACTION_BEGIN = 1,
    // Can only specify one of FINISH or CANCEL.
    DRAG_ACTION_FINISH = 2,
    DRAG_ACTION_CANCEL = 4
  };

  // This is called from tests that might change the order of panels, like
  // dragging test.
  std::vector<gfx::Rect> GetPanelBounds(
      const std::vector<Panel*>& panels) {
    std::vector<gfx::Rect> bounds;
    for (size_t i = 0; i < panels.size(); i++)
      bounds.push_back(panels[i]->GetBounds());
    return bounds;
  }

  std::vector<gfx::Rect> GetAllPanelBounds() {
    return GetPanelBounds(PanelManager::GetInstance()->panels());
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

  void TestDragging(int delta_x,
                    int delta_y,
                    size_t drag_index,
                    std::vector<int> expected_delta_x_after_drag,
                    std::vector<int> expected_delta_x_after_finish,
                    std::vector<gfx::Rect> expected_bounds_after_cancel,
                    int drag_action) {
    std::vector<Panel*> panels = PanelManager::GetInstance()->panels();

    // These are bounds at the beginning of this test.  This would be different
    // from expected_bounds_after_cancel in the case where we're testing for the
    // case of multiple drags before finishing the drag.  Here is an example:
    //
    // Test 1 - Create three panels and drag a panel to the right but don't
    //          finish or cancel the drag.
    //          expected_bounds_after_cancel == test_begin_bounds
    // Test 2 - Do another drag on the same panel.  There is no button press
    //          in this case as its the same drag that's continuing, this is
    //          simulating multiple drag events before button release.
    //          expected_bounds_after_cancel is still the same as in Test1.
    //          So in this case
    //              expected_bounds_after_cancel != test_begin_bounds.
    std::vector<gfx::Rect> test_begin_bounds = GetAllPanelBounds();

    NativePanel* panel_to_drag = panels[drag_index]->native_panel();
    scoped_ptr<NativePanelTesting> panel_testing_to_drag(
        NativePanelTesting::Create(panel_to_drag));

    if (drag_action & DRAG_ACTION_BEGIN) {
      // Trigger the mouse-pressed event.
      // All panels should remain in their original positions.
      panel_testing_to_drag->PressLeftMouseButtonTitlebar(
          panels[drag_index]->GetBounds().origin());
      EXPECT_EQ(test_begin_bounds, GetPanelBounds(panels));
    }

    // Trigger the drag.
    panel_testing_to_drag->DragTitlebar(delta_x, delta_y);

    // Compare against expected bounds.
    EXPECT_EQ(AddXDeltaToBounds(test_begin_bounds, expected_delta_x_after_drag),
              GetPanelBounds(panels));

    if (drag_action & DRAG_ACTION_CANCEL) {
      // Cancel the drag.
      // All panels should return to their initial positions.
      panel_testing_to_drag->CancelDragTitlebar();
      EXPECT_EQ(expected_bounds_after_cancel, GetAllPanelBounds());
    } else if (drag_action & DRAG_ACTION_FINISH) {
      // Finish the drag.
      // Compare against expected bounds.
      panel_testing_to_drag->FinishDragTitlebar();
      EXPECT_EQ(
          AddXDeltaToBounds(test_begin_bounds, expected_delta_x_after_finish),
          GetPanelBounds(panels));
    }
  }

  struct MenuItem {
    int id;
    bool enabled;
  };

  void ValidateSettingsMenuItems(ui::SimpleMenuModel* settings_menu_contents,
                                 size_t num_expected_menu_items,
                                 const MenuItem* expected_menu_items) {
    ASSERT_TRUE(settings_menu_contents);
    EXPECT_EQ(static_cast<int>(num_expected_menu_items),
              settings_menu_contents->GetItemCount());
    for (size_t i = 0; i < num_expected_menu_items; ++i) {
      if (expected_menu_items[i].id == -1) {
        EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
                  settings_menu_contents->GetTypeAt(i));
      } else {
        EXPECT_EQ(expected_menu_items[i].id,
                  settings_menu_contents->GetCommandIdAt(i));
        EXPECT_EQ(expected_menu_items[i].enabled,
                  settings_menu_contents->IsEnabledAt(i));
      }
    }
  }

  void TestCreateSettingsMenuForExtension(const FilePath::StringType& path,
                                          Extension::Location location,
                                          const std::string& homepage_url,
                                          const std::string& options_page) {
    // Creates a testing extension.
    DictionaryValue extra_value;
    if (!homepage_url.empty()) {
      extra_value.SetString(extension_manifest_keys::kHomepageURL,
                            homepage_url);
    }
    if (!options_page.empty()) {
      extra_value.SetString(extension_manifest_keys::kOptionsPage,
                            options_page);
    }
    scoped_refptr<Extension> extension = CreateExtension(
        path, location, extra_value);

    // Creates a panel with the app name that comes from the extension ID.
    Panel* panel = CreatePanel(
        web_app::GenerateApplicationNameFromExtensionId(extension->id()));

    scoped_ptr<PanelSettingsMenuModel> settings_menu_model(
        new PanelSettingsMenuModel(panel));

    // Validates the settings menu items.
    MenuItem expected_panel_menu_items[] = {
        { PanelSettingsMenuModel::COMMAND_NAME, false },
        { -1, false },  // Separator
        { PanelSettingsMenuModel::COMMAND_CONFIGURE, false },
        { PanelSettingsMenuModel::COMMAND_DISABLE, false },
        { PanelSettingsMenuModel::COMMAND_UNINSTALL, false },
        { -1, false },  // Separator
        { PanelSettingsMenuModel::COMMAND_MANAGE, true }
    };
    if (!homepage_url.empty())
      expected_panel_menu_items[0].enabled = true;
    if (!options_page.empty())
      expected_panel_menu_items[2].enabled = true;
    if (location != Extension::EXTERNAL_POLICY_DOWNLOAD) {
      expected_panel_menu_items[3].enabled = true;
      expected_panel_menu_items[4].enabled = true;
    }
    ValidateSettingsMenuItems(settings_menu_model.get(),
                              arraysize(expected_panel_menu_items),
                              expected_panel_menu_items);

    panel->Close();
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
      native_panels_testing[i] =
          NativePanelTesting::Create(panels[i]->native_panel());
    }

    // Test minimize.
    for (size_t index = 0; index < panels.size(); ++index) {
      // Press left mouse button.  Verify nothing changed.
      native_panels_testing[index]->PressLeftMouseButtonTitlebar(
          panels[index]->GetBounds().origin());
      EXPECT_EQ(expected_bounds, GetAllPanelBounds());
      EXPECT_EQ(expected_expansion_states, GetAllPanelExpansionStates());

      // Release mouse button.  Verify minimized.
      native_panels_testing[index]->ReleaseMouseButtonTitlebar();
      expected_bounds[index].set_height(Panel::kMinimizedPanelHeight);
      expected_bounds[index].set_y(
          test_begin_bounds[index].y() +
          test_begin_bounds[index].height() - Panel::kMinimizedPanelHeight);
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

  EXPECT_EQ(bounds.right(), panel_manager->StartingRightPosition());

  CloseWindowAndWait(panel->browser());

  EXPECT_EQ(0, panel_manager->num_panels());
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, CreateBigPanel) {
  Panel* panel = CreatePanelWithBounds("BigPanel", testing_work_area());
  gfx::Rect bounds = panel->GetBounds();
  EXPECT_EQ(panel->max_size().width(), bounds.width());
  EXPECT_LT(bounds.width(), testing_work_area().width());
  EXPECT_EQ(panel->max_size().height(), bounds.height());
  EXPECT_LT(bounds.height(), testing_work_area().height());
  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, FindBar) {
  Panel* panel = CreatePanelWithBounds("PanelTest", gfx::Rect(0, 0, 400, 400));
  Browser* browser = panel->browser();
  // FindBar needs tab contents.
  CreateTestTabContents(browser);
  browser->ShowFindBar();
  ASSERT_TRUE(browser->GetFindBarController()->find_bar()->IsFindBarVisible());
  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, DISABLED_CreatePanelOnOverflow) {
  TestCreatePanelOnOverflow();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, DragPanels) {
  static const int max_panels = 3;
  static const int zero_delta = 0;
  static const int small_delta = 10;
  static const int big_delta = 70;
  static const int bigger_delta = 120;
  static const int biggest_delta = 200;
  static const std::vector<int> zero_deltas(max_panels, zero_delta);

  std::vector<int> expected_delta_x_after_drag(max_panels, zero_delta);
  std::vector<int> expected_delta_x_after_finish(max_panels, zero_delta);
  std::vector<gfx::Rect> current_bounds;
  std::vector<gfx::Rect> initial_bounds;

  // Tests with a single panel.
  {
    CreatePanelWithBounds("PanelTest1", gfx::Rect(0, 0, 100, 100));

    // Drag left.
    expected_delta_x_after_drag[0] = -big_delta;
    expected_delta_x_after_finish = zero_deltas;
    TestDragging(-big_delta, zero_delta, 0, expected_delta_x_after_drag,
                 zero_deltas, GetAllPanelBounds(),
                 DRAG_ACTION_BEGIN | DRAG_ACTION_FINISH);

    // Drag left and cancel.
    expected_delta_x_after_drag[0] = -big_delta;
    expected_delta_x_after_finish = zero_deltas;
    TestDragging(-big_delta, zero_delta, 0, expected_delta_x_after_drag,
                 zero_deltas, GetAllPanelBounds(),
                 DRAG_ACTION_BEGIN | DRAG_ACTION_CANCEL);

    // Drag right.
    expected_delta_x_after_drag[0] = big_delta;
    TestDragging(big_delta, zero_delta, 0, expected_delta_x_after_drag,
                 zero_deltas, GetAllPanelBounds(),
                 DRAG_ACTION_BEGIN | DRAG_ACTION_FINISH);

    // Drag right and up.  Expect no vertical movement.
    TestDragging(big_delta, big_delta, 0, expected_delta_x_after_drag,
                 zero_deltas, GetAllPanelBounds(),
                 DRAG_ACTION_BEGIN | DRAG_ACTION_FINISH);

    // Drag up.  Expect no movement on drag.
    TestDragging(0, -big_delta, 0, zero_deltas, zero_deltas,
                 GetAllPanelBounds(), DRAG_ACTION_BEGIN | DRAG_ACTION_FINISH);

    // Drag down.  Expect no movement on drag.
    TestDragging(0, big_delta, 0, zero_deltas, zero_deltas,
                 GetAllPanelBounds(), DRAG_ACTION_BEGIN | DRAG_ACTION_FINISH);
  }

  // Tests with two panels.
  {
    CreatePanelWithBounds("PanelTest2", gfx::Rect(0, 0, 120, 120));

    // Drag left, small delta, expect no shuffle.
    {
      expected_delta_x_after_drag = zero_deltas;
      expected_delta_x_after_drag[0] = -small_delta;
      TestDragging(-small_delta, zero_delta, 0, expected_delta_x_after_drag,
                   zero_deltas, GetAllPanelBounds(),
                   DRAG_ACTION_BEGIN | DRAG_ACTION_FINISH);

      // Drag right panel i.e index 0, towards left, big delta, expect shuffle.
      initial_bounds = GetAllPanelBounds();
      expected_delta_x_after_drag = zero_deltas;

      // Deltas for panel being dragged.
      expected_delta_x_after_drag[0] = -big_delta;
      expected_delta_x_after_finish[0] =
          -(initial_bounds[1].width() + horizontal_spacing());

      // Deltas for panel being shuffled.
      expected_delta_x_after_drag[1] =
          initial_bounds[0].width() + horizontal_spacing();
      expected_delta_x_after_finish[1] = expected_delta_x_after_drag[1];

      TestDragging(-big_delta, zero_delta, 0, expected_delta_x_after_drag,
                   expected_delta_x_after_finish, initial_bounds,
                   DRAG_ACTION_BEGIN | DRAG_ACTION_FINISH);
    }

    // Drag left panel i.e index 1, towards right, big delta, expect shuffle.
    {
      initial_bounds = GetAllPanelBounds();
      expected_delta_x_after_drag = zero_deltas;
      expected_delta_x_after_finish = zero_deltas;

      // Deltas for panel being dragged.
      expected_delta_x_after_drag[1] = big_delta;
      expected_delta_x_after_finish[1] =
          initial_bounds[0].width() + horizontal_spacing();

      // Deltas for panel being shuffled.
      expected_delta_x_after_drag[0] =
          -(initial_bounds[1].width() + horizontal_spacing());
      expected_delta_x_after_finish[0] = expected_delta_x_after_drag[0];

      TestDragging(big_delta, zero_delta, 1, expected_delta_x_after_drag,
                   expected_delta_x_after_finish, initial_bounds,
                   DRAG_ACTION_BEGIN | DRAG_ACTION_FINISH);
    }

    // Drag left panel i.e index 1, towards right, big delta, expect shuffle.
    // Cancel drag.
    {
      initial_bounds = GetAllPanelBounds();
      expected_delta_x_after_drag = zero_deltas;

      // Delta for panel being dragged.
      expected_delta_x_after_drag[1] = big_delta;

      // Delta for panel being shuffled.
      expected_delta_x_after_drag[0] =
          -(initial_bounds[1].width() + horizontal_spacing());

      // As the drag is being canceled, we don't need expected_delta_x_after
      // finish.  Instead initial_bounds will be used.
      TestDragging(big_delta, zero_delta, 1, expected_delta_x_after_drag,
                   zero_deltas, initial_bounds,
                   DRAG_ACTION_BEGIN | DRAG_ACTION_CANCEL);
    }
  }

  // Tests with three panels.
  {
    CreatePanelWithBounds("PanelTest3", gfx::Rect(0, 0, 110, 110));

    // Drag leftmost panel to become rightmost with two shuffles.
    // We test both shuffles.
    {
      // Drag the left-most panel towards right without ending or cancelling it.
      // Expect shuffle.
      initial_bounds = GetAllPanelBounds();
      expected_delta_x_after_drag = zero_deltas;

      // Delta for panel being dragged.
      expected_delta_x_after_drag[2] = big_delta;

      // Delta for panel being shuffled.
      expected_delta_x_after_drag[1] =
          -(initial_bounds[2].width() + horizontal_spacing());

      // There is no delta after finish as drag is not done yet.
      TestDragging(big_delta, zero_delta, 2, expected_delta_x_after_drag,
                   zero_deltas, initial_bounds, DRAG_ACTION_BEGIN);

      // The drag index changes from 2 to 1 because of the first shuffle above.
      // Drag the panel further enough to the right to trigger a another
      // shuffle.  We finish the drag here.
      current_bounds = GetAllPanelBounds();
      expected_delta_x_after_drag = zero_deltas;
      expected_delta_x_after_finish = zero_deltas;

      // big_delta is not enough to cause the second shuffle as the panel being
      // dragged is in the middle of a drag and big_delta won't go far enough.
      // So we use bigger_delta.

      // Deltas for panel being dragged.
      expected_delta_x_after_drag[1] = bigger_delta;
      int x_after_finish = current_bounds[0].x() +
          (current_bounds[0].width() - current_bounds[1].width());
      expected_delta_x_after_finish[1] = x_after_finish - current_bounds[1].x();

      // Deltas for panel being shuffled.
      expected_delta_x_after_drag[0] =
          -(current_bounds[1].width() + horizontal_spacing());
      expected_delta_x_after_finish[0] = expected_delta_x_after_drag[0];

      TestDragging(bigger_delta, zero_delta, 1, expected_delta_x_after_drag,
                   expected_delta_x_after_finish, initial_bounds,
                   DRAG_ACTION_FINISH);
    }

    // Drag rightmost panel to become leftmost with two shuffles.
    // And then cancel the drag.
    {
      // First drag and shuffle.
      initial_bounds = GetAllPanelBounds();
      expected_delta_x_after_drag = zero_deltas;

      // Delta for panel being dragged.
      expected_delta_x_after_drag[0] = -big_delta;

      // Delta for panel being shuffled.
      expected_delta_x_after_drag[1] =
          (initial_bounds[0].width() + horizontal_spacing());

      // There is no delta after finish as drag is done yet.
      TestDragging(-big_delta, zero_delta, 0, expected_delta_x_after_drag,
                   zero_deltas, initial_bounds, DRAG_ACTION_BEGIN);

      // Second drag and shuffle.  We cancel the drag here.  The drag index
      // changes from 0 to 1 because of the first shuffle above.
      current_bounds = GetAllPanelBounds();
      expected_delta_x_after_drag = zero_deltas;

      // Delta for panel being dragged.
      expected_delta_x_after_drag[1] = -bigger_delta;

      // Deltas for panel being shuffled.
      int x_after_shuffle = current_bounds[0].x() - horizontal_spacing()
          - current_bounds[2].width();
      expected_delta_x_after_drag[2] = x_after_shuffle - current_bounds[2].x();

      // There is no delta after finish as drag canceled.
      TestDragging(-bigger_delta, zero_delta, 1, expected_delta_x_after_drag,
                   zero_deltas, initial_bounds, DRAG_ACTION_CANCEL);
    }

    // Drag leftmost panel to become the rightmost in a single drag.  This
    // will shuffle middle panel to leftmost and rightmost to middle.
    {
      initial_bounds = GetAllPanelBounds();
      expected_delta_x_after_drag = zero_deltas;
      expected_delta_x_after_finish = zero_deltas;

      // Use a delta big enough to go across two panels.
      // Deltas for panel being dragged.
      expected_delta_x_after_drag[2] = biggest_delta;
      expected_delta_x_after_finish[2] =
          initial_bounds[1].width() + horizontal_spacing() +
          initial_bounds[0].width() + horizontal_spacing();

      // Deltas for middle panels being shuffled.
      expected_delta_x_after_drag[1] =
          -(initial_bounds[2].width() + horizontal_spacing());
      expected_delta_x_after_finish[1] = expected_delta_x_after_drag[1];

      expected_delta_x_after_drag[0] =  expected_delta_x_after_drag[1];
      expected_delta_x_after_finish[0] = expected_delta_x_after_drag[0];

      TestDragging(biggest_delta, zero_delta, 2, expected_delta_x_after_drag,
                   expected_delta_x_after_finish, initial_bounds,
                   DRAG_ACTION_BEGIN | DRAG_ACTION_FINISH);
    }

    // Drag rightmost panel to become the leftmost in a single drag.  This
    // will shuffle middle panel to rightmost and leftmost to middle.
    {
      initial_bounds = GetAllPanelBounds();
      expected_delta_x_after_drag = zero_deltas;
      expected_delta_x_after_finish = zero_deltas;

      // Deltas for panel being dragged.
      expected_delta_x_after_drag[0] = -biggest_delta;
      expected_delta_x_after_finish[0] =
          -(initial_bounds[1].width() + horizontal_spacing() +
            initial_bounds[2].width() + horizontal_spacing());

      // Deltas for panels being shuffled.
      expected_delta_x_after_drag[1] =
          initial_bounds[0].width() + horizontal_spacing();
      expected_delta_x_after_finish[1] = expected_delta_x_after_drag[1];

      expected_delta_x_after_drag[2] = expected_delta_x_after_drag[1];
      expected_delta_x_after_finish[2] = expected_delta_x_after_drag[2];

      TestDragging(-biggest_delta, zero_delta, 0, expected_delta_x_after_drag,
                   expected_delta_x_after_finish, initial_bounds,
                   DRAG_ACTION_BEGIN | DRAG_ACTION_FINISH);
    }

    // Drag rightmost panel to become the leftmost in a single drag.  Then
    // cancel the drag.
    {
      initial_bounds = GetAllPanelBounds();
      expected_delta_x_after_drag = zero_deltas;

      // Deltas for panel being dragged.
      expected_delta_x_after_drag[0] = -biggest_delta;

      // Deltas for panels being shuffled.
      expected_delta_x_after_drag[1] =
          initial_bounds[0].width() + horizontal_spacing();
      expected_delta_x_after_drag[2] = expected_delta_x_after_drag[1];

      // No delta after finish as drag is canceled.
      TestDragging(-biggest_delta, zero_delta, 0, expected_delta_x_after_drag,
                   zero_deltas, initial_bounds,
                   DRAG_ACTION_BEGIN | DRAG_ACTION_CANCEL);
    }
  }

  PanelManager::GetInstance()->RemoveAll();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, CreateSettingsMenu) {
  TestCreateSettingsMenuForExtension(
      FILE_PATH_LITERAL("extension1"), Extension::EXTERNAL_POLICY_DOWNLOAD,
      "", "");
  TestCreateSettingsMenuForExtension(
      FILE_PATH_LITERAL("extension2"), Extension::INVALID,
      "http://home", "options.html");
}

#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_AutoResize AutoResize
#else
#define MAYBE_AutoResize FLAKY_AutoResize
#endif
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_AutoResize) {
  PanelManager::GetInstance()->enable_auto_sizing(true);
  set_testing_work_area(gfx::Rect(0, 0, 1200, 900));

  // Create a test panel with tab contents loaded.
  CreatePanelParams params("PanelTest1", gfx::Rect(), SHOW_AS_ACTIVE);
  GURL url(ui_test_utils::GetTestUrl(
      FilePath(kTestDir),
      FilePath(FILE_PATH_LITERAL("update-preferred-size.html"))));
  params.url = url;
  Panel* panel = CreatePanelWithParams(params);

  // Expand the test page.
  gfx::Rect initial_bounds = panel->GetBounds();
  ui_test_utils::WindowedNotificationObserver enlarge(
      chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
      content::Source<Panel>(panel));
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScript(
      panel->browser()->GetSelectedTabContents()->render_view_host(),
      std::wstring(),
      L"changeSize(50);"));
  enlarge.Wait();
  gfx::Rect bounds_on_grow = panel->GetBounds();
  EXPECT_GT(bounds_on_grow.width(), initial_bounds.width());
  EXPECT_EQ(bounds_on_grow.height(), initial_bounds.height());

  // Shrink the test page.
  ui_test_utils::WindowedNotificationObserver shrink(
      chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
      content::Source<Panel>(panel));
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScript(
      panel->browser()->GetSelectedTabContents()->render_view_host(),
      std::wstring(),
      L"changeSize(-30);"));
  shrink.Wait();
  gfx::Rect bounds_on_shrink = panel->GetBounds();
  EXPECT_LT(bounds_on_shrink.width(), bounds_on_grow.width());
  EXPECT_GT(bounds_on_shrink.width(), initial_bounds.width());
  EXPECT_EQ(bounds_on_shrink.height(), initial_bounds.height());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, AnimateBounds) {
  Panel* panel = CreatePanelWithBounds("PanelTest", gfx::Rect(0, 0, 100, 100));
  scoped_ptr<NativePanelTesting> panel_testing(
      NativePanelTesting::Create(panel->native_panel()));

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
  // Disable mouse watcher. We don't care about mouse movements in this test.
  PanelManager* panel_manager = PanelManager::GetInstance();
  PanelMouseWatcher* mouse_watcher = new TestPanelMouseWatcher();
  panel_manager->set_mouse_watcher(mouse_watcher);
  Panel* panel = CreatePanelWithBounds("PanelTest", gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());
  EXPECT_EQ(panel->GetBounds(), panel->GetRestoredBounds());
  EXPECT_EQ(0, panel_manager->minimized_panel_count());

  panel->SetExpansionState(Panel::MINIMIZED);
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());
  gfx::Rect bounds = panel->GetBounds();
  gfx::Rect restored = panel->GetRestoredBounds();
  EXPECT_EQ(bounds.x(), restored.x());
  EXPECT_GT(bounds.y(), restored.y());
  EXPECT_EQ(bounds.width(), restored.width());
  EXPECT_LT(bounds.height(), restored.height());
  EXPECT_EQ(1, panel_manager->minimized_panel_count());

  panel->SetExpansionState(Panel::TITLE_ONLY);
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());
  bounds = panel->GetBounds();
  restored = panel->GetRestoredBounds();
  EXPECT_EQ(bounds.x(), restored.x());
  EXPECT_GT(bounds.y(), restored.y());
  EXPECT_EQ(bounds.width(), restored.width());
  EXPECT_LT(bounds.height(), restored.height());
  EXPECT_EQ(1, panel_manager->minimized_panel_count());

  panel->SetExpansionState(Panel::MINIMIZED);
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());
  bounds = panel->GetBounds();
  restored = panel->GetRestoredBounds();
  EXPECT_EQ(bounds.x(), restored.x());
  EXPECT_GT(bounds.y(), restored.y());
  EXPECT_EQ(bounds.width(), restored.width());
  EXPECT_LT(bounds.height(), restored.height());
  EXPECT_EQ(1, panel_manager->minimized_panel_count());

  panel->SetExpansionState(Panel::EXPANDED);
  EXPECT_EQ(panel->GetBounds(), panel->GetRestoredBounds());
  EXPECT_EQ(0, panel_manager->minimized_panel_count());

  // Verify that changing the panel bounds only affects restored height
  // when panel is expanded.
  int saved_restored_height = restored.height();
  panel->SetExpansionState(Panel::MINIMIZED);
  bounds = gfx::Rect(10, 20, 300, 400);
  panel->SetPanelBounds(bounds);
  EXPECT_EQ(saved_restored_height, panel->GetRestoredBounds().height());
  EXPECT_EQ(1, panel_manager->minimized_panel_count());

  panel->SetExpansionState(Panel::TITLE_ONLY);
  bounds = gfx::Rect(20, 30, 100, 200);
  panel->SetPanelBounds(bounds);
  EXPECT_EQ(saved_restored_height, panel->GetRestoredBounds().height());
  EXPECT_EQ(1, panel_manager->minimized_panel_count());

  panel->SetExpansionState(Panel::EXPANDED);
  bounds = gfx::Rect(40, 60, 300, 400);
  panel->SetPanelBounds(bounds);
  EXPECT_NE(saved_restored_height, panel->GetRestoredBounds().height());
  EXPECT_EQ(0, panel_manager->minimized_panel_count());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MinimizeRestore) {
  // We'll simulate mouse movements for test.
  PanelMouseWatcher* mouse_watcher = new TestPanelMouseWatcher();
  PanelManager::GetInstance()->set_mouse_watcher(mouse_watcher);

  // Test with one panel.
  CreatePanelWithBounds("PanelTest1", gfx::Rect(0, 0, 100, 100));
  TestMinimizeRestore();

  PanelManager::GetInstance()->RemoveAll();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MinimizeRestoreTwoPanels) {
  // We'll simulate mouse movements for test.
  PanelMouseWatcher* mouse_watcher = new TestPanelMouseWatcher();
  PanelManager::GetInstance()->set_mouse_watcher(mouse_watcher);

  // Test with two panels.
  CreatePanelWithBounds("PanelTest1", gfx::Rect(0, 0, 100, 100));
  CreatePanelWithBounds("PanelTest2", gfx::Rect(0, 0, 110, 110));
  TestMinimizeRestore();

  PanelManager::GetInstance()->RemoveAll();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MinimizeRestoreThreePanels) {
  // We'll simulate mouse movements for test.
  PanelMouseWatcher* mouse_watcher = new TestPanelMouseWatcher();
  PanelManager::GetInstance()->set_mouse_watcher(mouse_watcher);

  // Test with three panels.
  CreatePanelWithBounds("PanelTest1", gfx::Rect(0, 0, 100, 100));
  CreatePanelWithBounds("PanelTest2", gfx::Rect(0, 0, 110, 110));
  CreatePanelWithBounds("PanelTest3", gfx::Rect(0, 0, 120, 120));
  TestMinimizeRestore();

  PanelManager::GetInstance()->RemoveAll();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, ActivatePanelOrTabbedWindow) {
  CreatePanelParams params1("Panel1", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel1 = CreatePanelWithParams(params1);
  CreatePanelParams params2("Panel2", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel2 = CreatePanelWithParams(params2);
  // Need tab contents in order to trigger deactivation upon close.
  CreateTestTabContents(panel2->browser());

  ASSERT_FALSE(panel1->IsActive());
  ASSERT_TRUE(panel2->IsActive());
  // Activate main tabbed window.
  browser()->window()->Activate();
  WaitForPanelActiveState(panel2, SHOW_AS_INACTIVE);

  // Activate a panel.
  panel2->Activate();
  WaitForPanelActiveState(panel2, SHOW_AS_ACTIVE);

  // Activate the main tabbed window back.
  browser()->window()->Activate();
  WaitForPanelActiveState(panel2, SHOW_AS_INACTIVE);
  // Close the main tabbed window. That should move focus back to panel.
  CloseWindowAndWait(browser());
  WaitForPanelActiveState(panel2, SHOW_AS_ACTIVE);

  // Activate another panel.
  panel1->Activate();
  WaitForPanelActiveState(panel1, SHOW_AS_ACTIVE);
  WaitForPanelActiveState(panel2, SHOW_AS_INACTIVE);

  // Switch focus between panels.
  panel2->Activate();
  WaitForPanelActiveState(panel2, SHOW_AS_ACTIVE);
  WaitForPanelActiveState(panel1, SHOW_AS_INACTIVE);

  // Close active panel, focus should move to the remaining one.
  CloseWindowAndWait(panel2->browser());
  WaitForPanelActiveState(panel1, SHOW_AS_ACTIVE);
  panel1->Close();
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
      NativePanelTesting::Create(panel->native_panel()));
  EXPECT_TRUE(panel->IsActive());
  EXPECT_TRUE(native_panel_testing->VerifyActiveState(true));

  // Deactivate the panel.
  panel->Deactivate();
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);
  EXPECT_FALSE(panel->IsActive());
  EXPECT_TRUE(native_panel_testing->VerifyActiveState(false));

  // Reactivate the panel.
  panel->Activate();
  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);
  EXPECT_TRUE(panel->IsActive());
  EXPECT_TRUE(native_panel_testing->VerifyActiveState(true));
}
// TODO(jianli): To be enabled for other platforms.
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_ActivateDeactivateMultiple ActivateDeactivateMultiple
#else
#define MAYBE_ActivateDeactivateMultiple DISABLED_ActivateDeactivateMultiple
#endif

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_ActivateDeactivateMultiple) {
  BrowserWindow* tabbed_window = BrowserList::GetLastActive()->window();

  // Create 4 panels in the following screen layout:
  //    P3  P2  P1  P0
  const int kNumPanels = 4;
  std::string panel_name_base("PanelTest");
  for (int i = 0; i < kNumPanels; ++i) {
    CreatePanelWithBounds(panel_name_base + base::IntToString(i),
                          gfx::Rect(0, 0, 100, 100));
  }
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

  // Minimizing active panel P1 should activate last active panel P3.
  panels[1]->SetExpansionState(Panel::MINIMIZED);
  last_active_states = expected_active_states;
  expected_active_states = ProduceExpectedActiveStates(3);
  WaitForPanelActiveStates(last_active_states, expected_active_states);
  EXPECT_EQ(expected_active_states, GetAllPanelActiveStates());
  EXPECT_FALSE(tabbed_window->IsActive());

  // Minimizing active panel P3 should activate last active panel P0.
  panels[3]->SetExpansionState(Panel::MINIMIZED);
  last_active_states = expected_active_states;
  expected_active_states = ProduceExpectedActiveStates(0);
  WaitForPanelActiveStates(last_active_states, expected_active_states);
  EXPECT_EQ(expected_active_states, GetAllPanelActiveStates());
  EXPECT_FALSE(tabbed_window->IsActive());

  // Minimizing active panel P0 should activate last active tabbed window.
  panels[0]->SetExpansionState(Panel::MINIMIZED);
  last_active_states = expected_active_states;
  expected_active_states = ProduceExpectedActiveStates(-1);  // -1 means none.
  WaitForPanelActiveStates(last_active_states, expected_active_states);
  EXPECT_EQ(expected_active_states, GetAllPanelActiveStates());
  EXPECT_TRUE(tabbed_window->IsActive());
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, DrawAttentionBasic) {
  CreatePanelParams params("Initially Inactive", gfx::Rect(), SHOW_AS_INACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  scoped_ptr<NativePanelTesting> native_panel_testing(
      NativePanelTesting::Create(panel->native_panel()));

  // Test that the attention is drawn when the expanded panel is not in focus.
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());
  EXPECT_FALSE(panel->IsActive());
  EXPECT_FALSE(panel->IsDrawingAttention());
  panel->FlashFrame();
  EXPECT_TRUE(panel->IsDrawingAttention());
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(native_panel_testing->VerifyDrawingAttention());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, DrawAttentionWhileMinimized) {
  // We'll simulate mouse movements for test.
  PanelMouseWatcher* mouse_watcher = new TestPanelMouseWatcher();
  PanelManager::GetInstance()->set_mouse_watcher(mouse_watcher);

  CreatePanelParams params("Initially Active", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  NativePanel* native_panel = panel->native_panel();
  scoped_ptr<NativePanelTesting> native_panel_testing(
      NativePanelTesting::Create(native_panel));

  // Test that the attention is drawn and the title-bar is brought up when the
  // minimized panel is drawing attention.
  panel->SetExpansionState(Panel::MINIMIZED);
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());
  panel->FlashFrame();
  EXPECT_TRUE(panel->IsDrawingAttention());
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(native_panel_testing->VerifyDrawingAttention());

  // Test that we cannot bring up other minimized panel if the mouse is over
  // the panel that draws attension.
  gfx::Point hover_point(panel->GetBounds().origin());
  MoveMouse(hover_point);
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

  // Test that we cannot bring down the panel that is drawing the attention.
  hover_point.set_y(hover_point.y() - 200);
  MoveMouse(hover_point);
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

  // Test that the attention is cleared when activated.
  panel->Activate();
  MessageLoop::current()->RunAllPending();
  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, DrawAttentionWhenActive) {
  CreatePanelParams params("Initially Active", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  scoped_ptr<NativePanelTesting> native_panel_testing(
      NativePanelTesting::Create(panel->native_panel()));

  // Test that the attention should not be drawn if the expanded panel is in
  // focus.
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());
  EXPECT_TRUE(panel->IsActive());
  EXPECT_FALSE(panel->IsDrawingAttention());
  panel->FlashFrame();
  EXPECT_FALSE(panel->IsDrawingAttention());
  MessageLoop::current()->RunAllPending();
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, DrawAttentionResetOnActivate) {
  CreatePanelParams params("Initially active", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  scoped_ptr<NativePanelTesting> native_panel_testing(
      NativePanelTesting::Create(panel->native_panel()));

  // Activate the panel.
  panel->Deactivate();
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);

  panel->FlashFrame();
  EXPECT_TRUE(panel->IsDrawingAttention());
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(native_panel_testing->VerifyDrawingAttention());

  // Test that the attention is cleared when panel gets focus.
  panel->Activate();
  MessageLoop::current()->RunAllPending();
  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  panel->Close();
}

// TODO(dimich): try/enable on other platforms.
#if defined(OS_MACOSX)
#define MAYBE_DrawAttentionResetOnClick DrawAttentionResetOnClick
#else
#define MAYBE_DrawAttentionResetOnClick DISABLED_DrawAttentionResetOnClick
#endif

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_DrawAttentionResetOnClick) {
  CreatePanelParams params("Initially Inactive", gfx::Rect(), SHOW_AS_INACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  scoped_ptr<NativePanelTesting> native_panel_testing(
      NativePanelTesting::Create(panel->native_panel()));

  panel->FlashFrame();
  EXPECT_TRUE(panel->IsDrawingAttention());
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(native_panel_testing->VerifyDrawingAttention());

  // Test that the attention is cleared when panel gets focus.
  native_panel_testing->PressLeftMouseButtonTitlebar(
      panel->GetBounds().origin());
  native_panel_testing->ReleaseMouseButtonTitlebar();

  MessageLoop::current()->RunAllPending();
  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_FALSE(native_panel_testing->VerifyDrawingAttention());

  panel->Close();
}

// There was a bug when it was not possible to minimize the panel by clicking
// on the titlebar right after it was restored and activated. This test verifies
// it's possible.
IN_PROC_BROWSER_TEST_F(PanelBrowserTest,
                       MinimizeImmediatelyAfterRestore) {
  CreatePanelParams params("Initially Inactive", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  scoped_ptr<NativePanelTesting> native_panel_testing(
      NativePanelTesting::Create(panel->native_panel()));

  panel->SetExpansionState(Panel::MINIMIZED);  // this should deactivate.
  MessageLoop::current()->RunAllPending();
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());

  panel->Activate();
  MessageLoop::current()->RunAllPending();
  WaitForPanelActiveState(panel, SHOW_AS_ACTIVE);
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());

  // Test that click on the titlebar right after expansion minimizes the Panel.
  native_panel_testing->PressLeftMouseButtonTitlebar(
      panel->GetBounds().origin());
  native_panel_testing->ReleaseMouseButtonTitlebar();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());
  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, FocusLostOnMinimize) {
  CreatePanelParams params("Initially Active", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());

  panel->SetExpansionState(Panel::MINIMIZED);
  MessageLoop::current()->RunAllPending();
  WaitForPanelActiveState(panel, SHOW_AS_INACTIVE);
  panel->Close();
}

// TODO(prasadt): Enable on Linux. This actually passes just fine on the bots.
// But disabling it on Linux because it fails on Gnome running compiz which is
// the typical linux dev machine configuration.
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_CreateInactiveSwitchToActive CreateInactiveSwitchToActive
#else
#define MAYBE_CreateInactiveSwitchToActive DISABLED_CreateInactiveSwitchToActive
#endif

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_CreateInactiveSwitchToActive) {
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
  CloseWindowAndWait(browser());

  EXPECT_EQ(Panel::EXPANDED, panel1->expansion_state());
  EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());
  panel1->Activate();
  WaitForPanelActiveState(panel1, SHOW_AS_ACTIVE);

  panel1->SetExpansionState(Panel::MINIMIZED);
  MessageLoop::current()->RunAllPending();
  WaitForPanelActiveState(panel1, SHOW_AS_INACTIVE);
  EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());

  panel2->SetExpansionState(Panel::MINIMIZED);
  MessageLoop::current()->RunAllPending();
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

IN_PROC_BROWSER_TEST_F(PanelBrowserTest,
                       NonExtensionDomainPanelsCloseOnUninstall) {
  // Create a test extension.
  DictionaryValue empty_value;
  scoped_refptr<Extension> extension =
      CreateExtension(FILE_PATH_LITERAL("TestExtension"),
      Extension::INVALID, empty_value);
  std::string extension_app_name =
      web_app::GenerateApplicationNameFromExtensionId(extension->id());

  PanelManager* panel_manager = PanelManager::GetInstance();
  EXPECT_EQ(0, panel_manager->num_panels());

  // Create a panel with the extension as host.
  CreatePanelParams params(extension_app_name, gfx::Rect(), SHOW_AS_INACTIVE);
  std::string extension_domain_url(chrome::kExtensionScheme);
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
  scoped_refptr<Extension> extension_other =
      CreateExtension(FILE_PATH_LITERAL("TestExtensionOther"),
      Extension::INVALID, empty_value);
  std::string extension_app_name_other =
      web_app::GenerateApplicationNameFromExtensionId(extension_other->id());
  Panel* panel_other = CreatePanel(extension_app_name_other);

  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(panel->browser()));
  ui_test_utils::WindowedNotificationObserver signal1(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(panel1->browser()));

  // Send unload notification on the first extension.
  UnloadedExtensionInfo details(extension,
                                extension_misc::UNLOAD_REASON_UNINSTALL);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::Source<Profile>(browser()->profile()),
      content::Details<UnloadedExtensionInfo>(&details));

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

  // Create a test panel with tab contents loaded.
  CreatePanelParams params("PanelTest1", gfx::Rect(0, 0, 300, 300),
                           SHOW_AS_ACTIVE);
  params.url = ui_test_utils::GetTestUrl(
      FilePath(kTestDir),
      FilePath(FILE_PATH_LITERAL("onbeforeunload.html")));
  Panel* panel = CreatePanelWithParams(params);
  EXPECT_EQ(1, panel_manager->num_panels());
  TabContents* tab_contents = panel->browser()->GetSelectedTabContents();

  // Close panel and respond to the onbeforeunload dialog with cancel. This is
  // equivalent to clicking "Stay on this page"
  scoped_ptr<ui_test_utils::TitleWatcher> title_watcher(
      new ui_test_utils::TitleWatcher(tab_contents, title_first_close));
  panel->Close();
  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  alert->native_dialog()->CancelAppModalDialog();
  EXPECT_EQ(title_first_close, title_watcher->WaitAndGetTitle());
  EXPECT_EQ(1, panel_manager->num_panels());

  // Close panel and respond to the onbeforeunload dialog with close. This is
  // equivalent to clicking the OS close button on the dialog.
  title_watcher.reset(
      new ui_test_utils::TitleWatcher(tab_contents, title_second_close));
  panel->Close();
  alert = ui_test_utils::WaitForAppModalDialog();
  alert->native_dialog()->CloseAppModalDialog();
  EXPECT_EQ(title_second_close, title_watcher->WaitAndGetTitle());
  EXPECT_EQ(1, panel_manager->num_panels());

  // Close panel and respond to the onbeforeunload dialog with accept. This is
  // equivalent to clicking "Leave this page".
  ui_test_utils::WindowedNotificationObserver browser_closed(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(panel->browser()));
  panel->Close();
  alert = ui_test_utils::WaitForAppModalDialog();
  alert->native_dialog()->AcceptAppModalDialog();
  browser_closed.Wait();
  EXPECT_EQ(0, panel_manager->num_panels());
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, CreateWithExistingContents) {
  PanelManager::GetInstance()->enable_auto_sizing(true);

  // Load contents into regular tabbed browser.
  GURL url(ui_test_utils::GetTestUrl(
      FilePath(kTestDir),
      FilePath(FILE_PATH_LITERAL("update-preferred-size.html"))));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(1, browser()->tab_count());

  Profile* profile = browser()->profile();
  CreatePanelParams params("PanelTest1", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  Browser* panel_browser = panel->browser();
  EXPECT_EQ(2U, BrowserList::size());

  // Swap tab contents over to the panel from the tabbed browser.
  TabContentsWrapper* contents =
      browser()->tabstrip_model()->DetachTabContentsAt(0);
  panel_browser->tabstrip_model()->InsertTabContentsAt(
      0, contents, TabStripModel::ADD_NONE);
  panel_browser->SelectNumberedTab(0);
  EXPECT_EQ(contents, panel_browser->GetSelectedTabContentsWrapper());
  EXPECT_EQ(1, PanelManager::GetInstance()->num_panels());

  // Ensure that the tab contents were noticed by the panel by
  // verifying that the panel auto resizes correctly. (Panel
  // enables auto resizing when tab contents are detected.)
  int initial_width = panel->GetBounds().width();
  ui_test_utils::WindowedNotificationObserver enlarge(
      chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
      content::Source<Panel>(panel));
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScript(
      panel_browser->GetSelectedTabContents()->render_view_host(),
      std::wstring(),
      L"changeSize(50);"));
  enlarge.Wait();
  EXPECT_GT(panel->GetBounds().width(), initial_width);

  // Swapping tab contents back to the browser should close the panel.
  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(panel_browser));
  panel_browser->ConvertPopupToTabbedBrowser();
  signal.Wait();
  EXPECT_EQ(0, PanelManager::GetInstance()->num_panels());

  Browser* tabbed_browser = BrowserList::FindTabbedBrowser(profile, false);
  EXPECT_EQ(contents, tabbed_browser->GetSelectedTabContentsWrapper());
  tabbed_browser->window()->Close();
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

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, TightAutosizeAroundSingleLine) {
  PanelManager::GetInstance()->enable_auto_sizing(true);
  // Using 0 sizes triggers auto-sizing.
  CreatePanelParams params("Panel", gfx::Rect(), SHOW_AS_ACTIVE);
  params.url = GURL("data:text/html;charset=utf-8,<!doctype html><body>");
  Panel* panel = CreatePanelWithParams(params);

  int initial_width = panel->GetBounds().width();
  int initial_height = panel->GetBounds().height();

  // Inject some HTML content into the panel.
  ui_test_utils::WindowedNotificationObserver enlarge(
      chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
      content::Source<Panel>(panel));
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScript(
      panel->browser()->GetSelectedTabContents()->render_view_host(),
      std::wstring(),
      L"document.body.innerHTML ="
      L"'<nobr>line of text and a <button>Button</button>';"));
  enlarge.Wait();

  // The panel should have become larger in both dimensions (the minimums
  // has to be set to be smaller then a simple 1-line content, so the autosize
  // can work correctly.
  EXPECT_GT(panel->GetBounds().width(), initial_width);
  EXPECT_GT(panel->GetBounds().height(), initial_height);

  panel->Close();
}

class PanelDownloadTest : public PanelBrowserTest {
 public:
  PanelDownloadTest() : PanelBrowserTest() { }

  // Creates a temporary directory for downloads that is auto-deleted
  // on destruction.
  bool CreateDownloadDirectory(Profile* profile) {
    bool created = downloads_directory_.CreateUniqueTempDir();
    if (!created)
      return false;
    profile->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory,
        downloads_directory_.path());
    return true;
  }

 protected:
  void SetUpOnMainThread() OVERRIDE {
    PanelBrowserTest::SetUpOnMainThread();

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

 private:
  // Location of the downloads directory for download tests.
  ScopedTempDir downloads_directory_;
};

class DownloadObserver : public DownloadManager::Observer {
 public:
  explicit DownloadObserver(Profile* profile)
      : download_manager_(
          DownloadServiceFactory::GetForProfile(profile)->GetDownloadManager()),
        saw_download_(false),
        waiting_(false) {
    download_manager_->AddObserver(this);
  }

  ~DownloadObserver() {
    download_manager_->RemoveObserver(this);
  }

  void WaitForDownload() {
    if (!saw_download_) {
      waiting_ = true;
      ui_test_utils::RunMessageLoop();
      EXPECT_TRUE(saw_download_);
      waiting_ = false;
    }
  }

  // DownloadManager::Observer
  virtual void ModelChanged() {
    std::vector<DownloadItem*> downloads;
    download_manager_->SearchDownloads(string16(), &downloads);
    if (downloads.empty())
      return;

    EXPECT_EQ(1U, downloads.size());
    downloads.front()->Cancel(false);  // Don't actually need to download it.

    saw_download_ = true;
    EXPECT_TRUE(waiting_);
    MessageLoopForUI::current()->Quit();
  }

 private:
  DownloadManager* download_manager_;
  bool saw_download_;
  bool waiting_;
};

// Verify that the download shelf is opened in the existing tabbed browser
// when a download is started in a Panel.
IN_PROC_BROWSER_TEST_F(PanelDownloadTest, Download) {
  Profile* profile = browser()->profile();
  ASSERT_TRUE(CreateDownloadDirectory(profile));
  Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                 "PanelTest",
                                                 gfx::Rect(),
                                                 profile);
  EXPECT_EQ(2U, BrowserList::size());
  ASSERT_FALSE(browser()->window()->IsDownloadShelfVisible());
  ASSERT_FALSE(panel_browser->window()->IsDownloadShelfVisible());

  scoped_ptr<DownloadObserver> observer(new DownloadObserver(profile));
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL download_url(URLRequestMockHTTPJob::GetMockUrl(file));
  ui_test_utils::NavigateToURLWithDisposition(
      panel_browser,
      download_url,
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  observer->WaitForDownload();

#if defined(OS_CHROMEOS)
  // ChromeOS uses a download panel instead of a download shelf.
  EXPECT_EQ(3U, BrowserList::size());
  ASSERT_FALSE(browser()->window()->IsDownloadShelfVisible());

  std::set<Browser*> original_browsers;
  original_browsers.insert(browser());
  original_browsers.insert(panel_browser);
  Browser* added = ui_test_utils::GetBrowserNotInSet(original_browsers);
  ASSERT_TRUE(added->is_type_panel());
  ASSERT_FALSE(added->window()->IsDownloadShelfVisible());
#else
  EXPECT_EQ(2U, BrowserList::size());
  ASSERT_TRUE(browser()->window()->IsDownloadShelfVisible());
#endif

  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, panel_browser->tab_count());
  ASSERT_FALSE(panel_browser->window()->IsDownloadShelfVisible());

  panel_browser->CloseWindow();
  browser()->CloseWindow();
}

// Verify that a new tabbed browser is created to display a download
// shelf when a download is started in a Panel and there is no existing
// tabbed browser.
IN_PROC_BROWSER_TEST_F(PanelDownloadTest, DownloadNoTabbedBrowser) {
  Profile* profile = browser()->profile();
  ASSERT_TRUE(CreateDownloadDirectory(profile));
  Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                 "PanelTest",
                                                 gfx::Rect(),
                                                 profile);
  EXPECT_EQ(2U, BrowserList::size());
  ASSERT_FALSE(browser()->window()->IsDownloadShelfVisible());
  ASSERT_FALSE(panel_browser->window()->IsDownloadShelfVisible());

  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser()));
  browser()->CloseWindow();
  signal.Wait();
  ASSERT_EQ(1U, BrowserList::size());
  ASSERT_EQ(NULL, Browser::GetTabbedBrowser(profile, false));

  scoped_ptr<DownloadObserver> observer(new DownloadObserver(profile));
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL download_url(URLRequestMockHTTPJob::GetMockUrl(file));
  ui_test_utils::NavigateToURLWithDisposition(
      panel_browser,
      download_url,
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  observer->WaitForDownload();

  EXPECT_EQ(2U, BrowserList::size());

#if defined(OS_CHROMEOS)
  // ChromeOS uses a download panel instead of a download shelf.
  std::set<Browser*> original_browsers;
  original_browsers.insert(panel_browser);
  Browser* added = ui_test_utils::GetBrowserNotInSet(original_browsers);
  ASSERT_TRUE(added->is_type_panel());
  ASSERT_FALSE(added->window()->IsDownloadShelfVisible());
#else
  Browser* tabbed_browser = Browser::GetTabbedBrowser(profile, false);
  EXPECT_EQ(1, tabbed_browser->tab_count());
  ASSERT_TRUE(tabbed_browser->window()->IsDownloadShelfVisible());
  tabbed_browser->CloseWindow();
#endif

  EXPECT_EQ(1, panel_browser->tab_count());
  ASSERT_FALSE(panel_browser->window()->IsDownloadShelfVisible());

  panel_browser->CloseWindow();
}

class PanelAndNotificationTest : public PanelBrowserTest {
 public:
  PanelAndNotificationTest() : PanelBrowserTest() {
    // Do not use our own testing work area since desktop notification code
    // does not have the hook up for testing work area.
    set_testing_work_area(gfx::Rect());
  }

  virtual ~PanelAndNotificationTest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    g_browser_process->local_state()->SetInteger(
        prefs::kDesktopNotificationPosition, BalloonCollection::LOWER_RIGHT);
    balloons_ = new BalloonCollectionImpl();
    ui_manager_.reset(NotificationUIManager::Create(
        g_browser_process->local_state(), balloons_));
    service_.reset(new DesktopNotificationService(browser()->profile(),
                   ui_manager_.get()));

    PanelBrowserTest::SetUpOnMainThread();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    balloons_->RemoveAll();
    MessageLoopForUI::current()->RunAllPending();

    service_.reset();
    ui_manager_.reset();

    PanelBrowserTest::CleanUpOnMainThread();
  }

  content::ShowDesktopNotificationHostMsgParams StandardTestNotification() {
    content::ShowDesktopNotificationHostMsgParams params;
    params.notification_id = 0;
    params.origin = GURL("http://www.google.com");
    params.is_html = false;
    params.icon_url = GURL("/icon.png");
    params.title = ASCIIToUTF16("Title");
    params.body = ASCIIToUTF16("Text");
    params.direction = WebKit::WebTextDirectionDefault;
    return params;
  }

  int GetBalloonBottomPosition(Balloon* balloon) const {
#if defined(OS_MACOSX)
    // The position returned by the notification balloon is based on Mac's
    // vertically inverted orientation. We need to flip it so that it can
    // be compared against the position returned by the panel.
    gfx::Size screen_size = gfx::Screen::GetPrimaryMonitorSize();
    return screen_size.height() - balloon->GetPosition().y();
#else
    return balloon->GetPosition().y() + balloon->GetViewSize().height();
#endif
  }

  DesktopNotificationService* service() const { return service_.get(); }
  const BalloonCollection::Balloons& balloons() const {
    return balloons_->GetActiveBalloons();
  }

 private:
  BalloonCollectionImpl* balloons_;  // Owned by NotificationUIManager.
  scoped_ptr<NotificationUIManager> ui_manager_;
  scoped_ptr<DesktopNotificationService> service_;
};

IN_PROC_BROWSER_TEST_F(PanelAndNotificationTest, DISABLED_NoOverlapping) {
  const int kPanelWidth = 200;
  const int kShortPanelHeight = 150;
  const int kTallPanelHeight = 200;

  content::ShowDesktopNotificationHostMsgParams params =
      StandardTestNotification();
  EXPECT_TRUE(service()->ShowDesktopNotification(
        params, 0, 0, DesktopNotificationService::PageNotification));
  MessageLoopForUI::current()->RunAllPending();
  Balloon* balloon = balloons().front();
  int original_balloon_bottom = GetBalloonBottomPosition(balloon);
  // Ensure that balloon width is greater than the panel width.
  EXPECT_GT(balloon->GetViewSize().width(), kPanelWidth);

  // Creating a short panel should move the notification balloon up.
  Panel* panel1 = CreatePanelWithBounds(
      "Panel1", gfx::Rect(0, 0, kPanelWidth, kShortPanelHeight));
  WaitForPanelAdded(panel1);
  int balloon_bottom_after_short_panel_created =
      GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_after_short_panel_created, panel1->GetBounds().y());
  EXPECT_LT(balloon_bottom_after_short_panel_created, original_balloon_bottom);

  // Creating another tall panel should move the notification balloon further
  // up.
  Panel* panel2 = CreatePanelWithBounds(
      "Panel2", gfx::Rect(0, 0, kPanelWidth, kTallPanelHeight));
  WaitForPanelAdded(panel2);
  int balloon_bottom_after_tall_panel_created =
      GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_after_tall_panel_created, panel2->GetBounds().y());
  EXPECT_LT(balloon_bottom_after_tall_panel_created,
            balloon_bottom_after_short_panel_created);

  // Minimizing tall panel should move the notification balloon down to the same
  // position when short panel is first created.
  panel2->SetExpansionState(Panel::MINIMIZED);
  WaitForBoundsAnimationFinished(panel2);
  int balloon_bottom_after_tall_panel_minimized =
      GetBalloonBottomPosition(balloon);
  EXPECT_EQ(balloon_bottom_after_short_panel_created,
            balloon_bottom_after_tall_panel_minimized);

  // Minimizing short panel should move the notification balloon further down.
  panel1->SetExpansionState(Panel::MINIMIZED);
  WaitForBoundsAnimationFinished(panel1);
  int balloon_bottom_after_both_panels_minimized =
      GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_after_both_panels_minimized,
            panel1->GetBounds().y());
  EXPECT_LT(balloon_bottom_after_both_panels_minimized,
            panel2->GetBounds().y());
  EXPECT_LT(balloon_bottom_after_short_panel_created,
            balloon_bottom_after_both_panels_minimized);
  EXPECT_LT(balloon_bottom_after_both_panels_minimized,
            original_balloon_bottom);

  // Bringing up the titlebar for tall panel should move the notification
  // balloon up a little bit.
  panel2->SetExpansionState(Panel::TITLE_ONLY);
  WaitForBoundsAnimationFinished(panel2);
  int balloon_bottom_after_tall_panel_titlebar_up =
      GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_after_tall_panel_titlebar_up,
            panel2->GetBounds().y());
  EXPECT_LT(balloon_bottom_after_tall_panel_titlebar_up,
            balloon_bottom_after_both_panels_minimized);
  EXPECT_LT(balloon_bottom_after_short_panel_created,
            balloon_bottom_after_tall_panel_titlebar_up);

  // Expanding short panel should move the notification balloon further up to
  // the same position when short panel is first created.
  panel1->SetExpansionState(Panel::EXPANDED);
  WaitForBoundsAnimationFinished(panel1);
  int balloon_bottom_after_short_panel_expanded =
      GetBalloonBottomPosition(balloon);
  EXPECT_EQ(balloon_bottom_after_short_panel_created,
            balloon_bottom_after_short_panel_expanded);

  // Closing short panel should move the notification balloon down to the same
  // position when tall panel brings up its titlebar.
  CloseWindowAndWait(panel1->browser());
  EXPECT_EQ(balloon_bottom_after_tall_panel_titlebar_up,
            GetBalloonBottomPosition(balloon));

  // Closing the remaining tall panel should move the notification balloon back
  // to its original position.
  CloseWindowAndWait(panel2->browser());
  EXPECT_EQ(original_balloon_bottom, GetBalloonBottomPosition(balloon));
}
