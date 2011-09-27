// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "chrome/browser/ui/panels/panel_settings_menu_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/net/url_request_mock_http_job.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

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
        Source<Browser>(browser));
    browser->CloseWindow();
    signal.Wait();
    // Now we have one less browser instance.
    EXPECT_EQ(browser_count - 1, BrowserList::size());
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

    // Test closing the leftmost panel that is from same extension.
    ui_test_utils::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        Source<Browser>(panel2->browser()));
    Panel* panel4 = CreatePanelWithBounds(
        web_app::GenerateApplicationNameFromExtensionId(extension2->id()),
        gfx::Rect(0, 0, 280, 200));
    signal.Wait();
    ASSERT_EQ(3, panel_manager->num_panels());
    EXPECT_LT(panel4->GetBounds().right(), panel3->GetBounds().x());
    EXPECT_LT(panel3->GetBounds().right(), panel1->GetBounds().x());

    // Test closing the leftmost panel.
    ui_test_utils::WindowedNotificationObserver signal2(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        Source<Browser>(panel4->browser()));
    Panel* panel5 = CreatePanelWithBounds(
        web_app::GenerateApplicationNameFromExtensionId(extension3->id()),
        gfx::Rect(0, 0, 300, 200));
    signal2.Wait();
    ASSERT_EQ(3, panel_manager->num_panels());
    EXPECT_LT(panel5->GetBounds().right(), panel3->GetBounds().x());
    EXPECT_LT(panel3->GetBounds().right(), panel1->GetBounds().x());

    // Test closing 2 leftmost panels.
    ui_test_utils::WindowedNotificationObserver signal3(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        Source<Browser>(panel3->browser()));
    ui_test_utils::WindowedNotificationObserver signal4(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        Source<Browser>(panel5->browser()));
    Panel* panel6 = CreatePanelWithBounds(
        web_app::GenerateApplicationNameFromExtensionId(extension3->id()),
        gfx::Rect(0, 0, 500, 200));
    signal3.Wait();
    signal4.Wait();
    ASSERT_EQ(2, panel_manager->num_panels());
    EXPECT_LT(panel6->GetBounds().right(), panel1->GetBounds().x());

    panel1->Close();
    panel6->Close();
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

    std::vector<Panel*> panels = PanelManager::GetInstance()->panels();
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
      expected_bounds[index].set_height(
          PanelManager::minimized_panel_height());
      expected_bounds[index].set_y(
          test_begin_bounds[index].y() +
          test_begin_bounds[index].height() -
          PanelManager::minimized_panel_height());
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
          native_panels_testing[index]->TitleOnlyHeight());
      titlebar_exposed_bounds[index].set_y(
          test_begin_bounds[index].y() +
          test_begin_bounds[index].height() -
          native_panels_testing[index]->TitleOnlyHeight());
    }

    // Test hover.  All panels are currently in minimized state.
    EXPECT_EQ(minimized_states, GetAllPanelExpansionStates());
    for (size_t index = 0; index < panels.size(); ++index) {
      // Hover mouse on minimized panel.
      // Verify titlebar is exposed on all panels.
      gfx::Point hover_point(panels[index]->GetBounds().origin());
      native_panels_testing[index]->SetMousePositionForMinimizeRestore(
          hover_point);
      EXPECT_EQ(titlebar_exposed_bounds, GetAllPanelBounds());
      EXPECT_EQ(titlebar_exposed_states, GetAllPanelExpansionStates());

      // Hover mouse above the panel. Verify all panels are minimized.
      hover_point.set_y(
          panels[index]->GetBounds().y() - kFarEnoughFromHoverArea);
      native_panels_testing[index]->SetMousePositionForMinimizeRestore(
          hover_point);
      EXPECT_EQ(minimized_bounds, GetAllPanelBounds());
      EXPECT_EQ(minimized_states, GetAllPanelExpansionStates());

      // Hover mouse below minimized panel.
      // Verify titlebar is exposed on all panels.
      hover_point.set_y(panels[index]->GetBounds().y() +
                        panels[index]->GetBounds().height() + 5);
      native_panels_testing[index]->SetMousePositionForMinimizeRestore(
          hover_point);
      EXPECT_EQ(titlebar_exposed_bounds, GetAllPanelBounds());
      EXPECT_EQ(titlebar_exposed_states, GetAllPanelExpansionStates());

      // Hover below titlebar exposed panel.  Verify nothing changed.
      hover_point.set_y(panels[index]->GetBounds().y() +
                        panels[index]->GetBounds().height() + 6);
      native_panels_testing[index]->SetMousePositionForMinimizeRestore(
          hover_point);
      EXPECT_EQ(titlebar_exposed_bounds, GetAllPanelBounds());
      EXPECT_EQ(titlebar_exposed_states, GetAllPanelExpansionStates());

      // Hover mouse above panel.  Verify all panels are minimized.
      hover_point.set_y(
          panels[index]->GetBounds().y() - kFarEnoughFromHoverArea);
      native_panels_testing[index]->SetMousePositionForMinimizeRestore(
          hover_point);
      EXPECT_EQ(minimized_bounds, GetAllPanelBounds());
      EXPECT_EQ(minimized_states, GetAllPanelExpansionStates());
    }

    // Test restore.  All panels are currently in minimized state.
    for (size_t index = 0; index < panels.size(); ++index) {
      // Hover on the last panel.  This is to test the case of clicking on the
      // panel when it's in titlebar exposed state.
      if (index == panels.size() - 1) {
        native_panels_testing[index]->SetMousePositionForMinimizeRestore(
            minimized_bounds[index].origin());
      }

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
      if (index == panels.size() - 1) {
        native_panels_testing[index]->SetMousePositionForMinimizeRestore(
            minimized_bounds[index].origin());
      }
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

  CloseWindowAndWait(panel->browser());

  EXPECT_EQ(0, panel_manager->num_panels());
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, FindBar) {
  Panel* panel = CreatePanelWithBounds("PanelTest", gfx::Rect(0, 0, 400, 400));
  Browser* browser = panel->browser();
  browser->ShowFindBar();
  ASSERT_TRUE(browser->GetFindBarController()->find_bar()->IsFindBarVisible());
  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, CreatePanelOnOverflow) {
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

#if !defined(OS_CHROMEOS)
// This test fails on ChromeOS. See crbug.com/97541
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, AutoResize) {
  PanelManager::GetInstance()->enable_auto_sizing(true);

  // Create a test panel with tab contents loaded.
  CreatePanelParams params("PanelTest1", gfx::Rect(0, 0, 100, 100),
                           SHOW_AS_ACTIVE);
  params.url = GURL(chrome::kAboutBlankURL);
  Panel* panel = CreatePanelWithParams(params);

  // Load the test page.
  const FilePath::CharType* kUpdateSizeTestFile =
      FILE_PATH_LITERAL("update-preferred-size.html");
  ui_test_utils::NavigateToURL(panel->browser(),
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                FilePath(kUpdateSizeTestFile)));
  gfx::Rect initial_bounds = panel->GetBounds();
  EXPECT_LE(100, initial_bounds.width());
  EXPECT_LE(100, initial_bounds.height());

  // Expand the test page.
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScript(
      panel->browser()->GetSelectedTabContents()->render_view_host(),
      std::wstring(),
      L"changeSize(50);"));

  // Wait until the bounds get changed.
  gfx::Rect bounds_on_grow;
  while ((bounds_on_grow = panel->GetBounds()) == initial_bounds) {
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    MessageLoop::current()->RunAllPending();
  }
  EXPECT_GT(bounds_on_grow.width(), initial_bounds.width());
  EXPECT_EQ(bounds_on_grow.height(), initial_bounds.height());

  // Shrink the test page.
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScript(
      panel->browser()->GetSelectedTabContents()->render_view_host(),
      std::wstring(),
      L"changeSize(-30);"));

  // Wait until the bounds get changed.
  gfx::Rect bounds_on_shrink;
  while ((bounds_on_shrink = panel->GetBounds()) == bounds_on_grow) {
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    MessageLoop::current()->RunAllPending();
  }
  EXPECT_LT(bounds_on_shrink.width(), bounds_on_grow.width());
  EXPECT_GT(bounds_on_shrink.width(), initial_bounds.width());
  EXPECT_EQ(bounds_on_shrink.height(), initial_bounds.height());

  panel->Close();
}
#endif

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, RestoredBounds) {
  // Disable mouse watcher. We don't care about mouse movements in this test.
  NativePanelTesting::GetPanelMouseWatcherInstance()->EnableTestingMode();
  Panel* panel = CreatePanel("PanelTest");
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

  panel->SetExpansionState(Panel::EXPANDED);
  EXPECT_EQ(panel->GetBounds(), panel->GetRestoredBounds());

  panel->SetExpansionState(Panel::TITLE_ONLY);
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());
  bounds = panel->GetBounds();
  restored = panel->GetRestoredBounds();
  EXPECT_EQ(bounds.x(), restored.x());
  EXPECT_GT(bounds.y(), restored.y());
  EXPECT_EQ(bounds.width(), restored.width());
  EXPECT_LT(bounds.height(), restored.height());

  // Verify that changing the panel bounds only affects restored height
  // when panel is expanded.
  int restored_height = restored.height();
  bounds = gfx::Rect(10, 20, 300, 400);
  panel->SetPanelBounds(bounds);
  EXPECT_EQ(restored_height, panel->GetRestoredBounds().height());

  panel->SetExpansionState(Panel::MINIMIZED);
  bounds = gfx::Rect(20, 30, 100, 200);
  panel->SetPanelBounds(bounds);
  EXPECT_EQ(restored_height, panel->GetRestoredBounds().height());

  panel->SetExpansionState(Panel::EXPANDED);
  bounds = gfx::Rect(40, 60, 300, 400);
  panel->SetPanelBounds(bounds);
  EXPECT_NE(restored_height, panel->GetRestoredBounds().height());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MinimizeRestore) {
  // Disable mouse watcher.  We'll simulate mouse movements for test.
  NativePanelTesting::GetPanelMouseWatcherInstance()->EnableTestingMode();

  // Test with one panel.
  CreatePanelWithBounds("PanelTest1", gfx::Rect(0, 0, 100, 100));
  TestMinimizeRestore();

  // Test with two panels.
  CreatePanelWithBounds("PanelTest2", gfx::Rect(0, 0, 110, 110));
  TestMinimizeRestore();

  // Test with three panels.
  CreatePanelWithBounds("PanelTest3", gfx::Rect(0, 0, 120, 120));
  TestMinimizeRestore();

  PanelManager::GetInstance()->RemoveAll();
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
      : download_manager_(profile->GetDownloadManager()),
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
      Source<Browser>(browser()));
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
