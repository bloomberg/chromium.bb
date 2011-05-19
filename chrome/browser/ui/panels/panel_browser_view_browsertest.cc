// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_frame_view.h"
#include "chrome/browser/ui/panels/panel_browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/label.h"
#include "views/controls/menu/menu_2.h"


class PanelBrowserViewTest : public InProcessBrowserTest {
 public:
  PanelBrowserViewTest() : InProcessBrowserTest() { }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnablePanels);
  }

 protected:
  PanelBrowserView* CreatePanelBrowserView(const std::string& panel_name) {
    Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                   panel_name,
                                                   gfx::Size(),
                                                   browser()->profile());
    panel_browser->window()->Show();
    return static_cast<PanelBrowserView*>(
        static_cast<Panel*>(panel_browser->window())->browser_window());
  }

  void ValidateOptionsMenuItems(
      ui::SimpleMenuModel* options_menu_contents, size_t count, int* ids) {
    ASSERT_TRUE(options_menu_contents);
    EXPECT_EQ(static_cast<int>(count), options_menu_contents->GetItemCount());
    for (size_t i = 0; i < count; ++i) {
      if (ids[i] == -1) {
        EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
                  options_menu_contents->GetTypeAt(i));
      } else {
        EXPECT_EQ(ids[i] , options_menu_contents->GetCommandIdAt(i));
      }
    }
  }

  void ValidateDragging(PanelBrowserView** browser_views,
                        size_t num_browser_views,
                        size_t index_to_drag,
                        int delta_x,
                        int delta_y,
                        int* expected_delta_x_after_drag,
                        int* expected_delta_x_after_release,
                        bool cancel_dragging) {
    // Keep track of the initial bounds for comparison.
    scoped_array<gfx::Rect> initial_bounds(new gfx::Rect[num_browser_views]);
    for (size_t i = 0; i < num_browser_views; ++i)
      initial_bounds[i] = browser_views[i]->panel()->GetRestoredBounds();

    // Trigger the mouse-pressed event.
    // All panels should remain in their original positions.
    views::MouseEvent pressed(ui::ET_MOUSE_PRESSED,
                              initial_bounds[index_to_drag].x(),
                              initial_bounds[index_to_drag].y(),
                              ui::EF_LEFT_BUTTON_DOWN);
    browser_views[index_to_drag]->OnTitleBarMousePressed(pressed);
    gfx::Rect bounds_after_press =
        browser_views[index_to_drag]->panel()->GetRestoredBounds();
    for (size_t i = 0; i < num_browser_views; ++i) {
      EXPECT_EQ(initial_bounds[i],
                browser_views[i]->panel()->GetRestoredBounds());
    }

    // Trigger the mouse_dragged event if delta_x or delta_y is not 0.
    views::MouseEvent dragged(ui::ET_MOUSE_DRAGGED,
                              initial_bounds[index_to_drag].x() + delta_x,
                              initial_bounds[index_to_drag].y() + delta_y,
                              ui::EF_LEFT_BUTTON_DOWN);
    if (delta_x || delta_y) {
      browser_views[index_to_drag]->OnTitleBarMouseDragged(dragged);
      for (size_t i = 0; i < num_browser_views; ++i) {
        gfx::Rect expected_bounds = initial_bounds[i];
        expected_bounds.Offset(expected_delta_x_after_drag[i], 0);
        EXPECT_EQ(expected_bounds,
                  browser_views[i]->panel()->GetRestoredBounds());
      }
    }

    // Cancel the dragging if asked.
    // All panels should stay in their original positions.
    if (cancel_dragging) {
      browser_views[index_to_drag]->OnTitleBarMouseCaptureLost();
      for (size_t i = 0; i < num_browser_views; ++i) {
        EXPECT_EQ(initial_bounds[i],
                  browser_views[i]->panel()->GetRestoredBounds());
      }
      return;
    }

    // Otherwise, trigger the mouse-released event.
    views::MouseEvent released(ui::ET_MOUSE_RELEASED, 0, 0, 0);
    browser_views[index_to_drag]->OnTitleBarMouseReleased(released);
    for (size_t i = 0; i < num_browser_views; ++i) {
      gfx::Rect expected_bounds = initial_bounds[i];
       expected_bounds.Offset(expected_delta_x_after_release[i], 0);
      EXPECT_EQ(expected_bounds,
                browser_views[i]->panel()->GetRestoredBounds());
    }

    // If releasing the drag causes the panels to swap, move these panels back
    // so that the next test will not be affected.
    if (initial_bounds[index_to_drag] !=
        browser_views[index_to_drag]->panel()->GetRestoredBounds()) {
      // Find the index of the panel to swap with the dragging panel.
      size_t swapped_index = 0;
      for (; swapped_index < num_browser_views; ++swapped_index) {
        if (initial_bounds[index_to_drag] ==
            browser_views[swapped_index]->panel()->GetRestoredBounds())
          break;
      }
      ASSERT_NE(swapped_index, index_to_drag);

      // Repeat the mouse events to this panel.
      browser_views[swapped_index]->OnTitleBarMousePressed(pressed);
      browser_views[swapped_index]->OnTitleBarMouseDragged(dragged);
      browser_views[swapped_index]->OnTitleBarMouseReleased(released);
      for (size_t i = 0; i < num_browser_views; ++i) {
        EXPECT_EQ(initial_bounds[i],
                  browser_views[i]->panel()->GetRestoredBounds());
      }
    }
  }
};

// Panel is not supported for Linux view yet.
#if !defined(OS_LINUX) || !defined(TOOLKIT_VIEWS)
IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, CreatePanel) {
  PanelBrowserFrameView* frame_view =
      CreatePanelBrowserView("PanelTest")->GetFrameView();

  // We should have icon, text, options button and close button.
  EXPECT_EQ(4, frame_view->child_count());
  EXPECT_TRUE(frame_view->Contains(frame_view->title_icon_));
  EXPECT_TRUE(frame_view->Contains(frame_view->title_label_));
  EXPECT_TRUE(frame_view->Contains(frame_view->info_button_));
  EXPECT_TRUE(frame_view->Contains(frame_view->close_button_));

  // These controls should be visible.
  EXPECT_TRUE(frame_view->title_icon_->IsVisible());
  EXPECT_TRUE(frame_view->title_label_->IsVisible());
  EXPECT_TRUE(frame_view->info_button_->IsVisible());
  EXPECT_TRUE(frame_view->close_button_->IsVisible());

  // Validate their layouts.
  int title_bar_height = frame_view->NonClientTopBorderHeight() -
      frame_view->NonClientBorderThickness();
  EXPECT_GT(frame_view->title_icon_->width(), 0);
  EXPECT_GT(frame_view->title_icon_->height(), 0);
  EXPECT_LT(frame_view->title_icon_->height(), title_bar_height);
  EXPECT_GT(frame_view->title_label_->width(), 0);
  EXPECT_GT(frame_view->title_label_->height(), 0);
  EXPECT_LT(frame_view->title_label_->height(), title_bar_height);
  EXPECT_GT(frame_view->info_button_->width(), 0);
  EXPECT_GT(frame_view->info_button_->height(), 0);
  EXPECT_LT(frame_view->info_button_->height(), title_bar_height);
  EXPECT_GT(frame_view->close_button_->width(), 0);
  EXPECT_GT(frame_view->close_button_->height(), 0);
  EXPECT_LT(frame_view->close_button_->height(), title_bar_height);
  EXPECT_LT(frame_view->title_icon_->x() + frame_view->title_icon_->width(),
      frame_view->title_label_->x());
  EXPECT_LT(frame_view->title_label_->x() + frame_view->title_label_->width(),
      frame_view->info_button_->x());
  EXPECT_LT(
      frame_view->info_button_->x() + frame_view->info_button_->width(),
      frame_view->close_button_->x());

  // Validate that the controls should be updated when the activation state is
  // changed.
  frame_view->UpdateControlStyles(PanelBrowserFrameView::PAINT_AS_ACTIVE);
  gfx::Font title_label_font1 = frame_view->title_label_->font();
  frame_view->UpdateControlStyles(PanelBrowserFrameView::PAINT_AS_INACTIVE);
  gfx::Font title_label_font2 = frame_view->title_label_->font();
  EXPECT_NE(title_label_font1.GetStyle(), title_label_font2.GetStyle());
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, TitleBarMouseEvent) {
  // TODO(jianli): Move the test to platform-independent PanelManager unittest.

  // Creates the 1st panel.
  PanelBrowserView* browser_view1 = CreatePanelBrowserView("PanelTest1");

  // The delta is from the pressed location which is the left-top corner of the
  // panel to drag.
  int small_negative_delta = -5;
  int small_positive_delta = 5;
  int big_negative_delta =
      -(browser_view1->panel()->GetRestoredBounds().width() * 0.5 + 5);
  int big_positive_delta =
      browser_view1->panel()->GetRestoredBounds().width() * 1.5 + 5;

  // Tests dragging a panel in single-panel environment.
  // We should get back to the original position after the dragging is ended.
  int expected_single_delta_x_after_drag[] = { big_negative_delta };
  int expected_single_delta_x_after_release[] = { 0 };
  ValidateDragging(&browser_view1,
                   1,
                   0,
                   big_negative_delta,
                   big_negative_delta,
                   expected_single_delta_x_after_drag,
                   expected_single_delta_x_after_release,
                   false);

  expected_single_delta_x_after_drag[0] = big_positive_delta;
  ValidateDragging(&browser_view1,
                   1,
                   0,
                   big_positive_delta,
                   big_positive_delta,
                   expected_single_delta_x_after_drag,
                   expected_single_delta_x_after_release,
                   false);

  // Creates 2 more panels to test dragging a panel in multi-panel environment.
  PanelBrowserView* browser_view2 = CreatePanelBrowserView("PanelTest2");
  PanelBrowserView* browser_view3 = CreatePanelBrowserView("PanelTest3");
  PanelBrowserView* browser_views[] =
    { browser_view1, browser_view2, browser_view3 };
  size_t browser_view_count = arraysize(browser_views);
  int expected_delta_x_without_change[] = { 0, 0, 0 };

  // Tests dragging a panel with small delta.
  // We should get back to the original position after the dragging is ended.
  for (size_t i = 0; i < browser_view_count; ++i) {
    int expected_delta_x_after_drag[] = { 0, 0, 0 };

    expected_delta_x_after_drag[i] = small_negative_delta;
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     small_negative_delta,
                     small_negative_delta,
                     expected_delta_x_after_drag,
                     expected_delta_x_without_change,
                     false);

    expected_delta_x_after_drag[i] = small_positive_delta;
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     small_positive_delta,
                     small_positive_delta,
                     expected_delta_x_after_drag,
                     expected_delta_x_without_change,
                     false);
  }

  // Tests dragging a panel with big delta and then cancelling it.
  // We should get back to the original position.
  for (size_t i = 0; i < browser_view_count; ++i) {
    int expected_delta_x_after_drag[] = { 0, 0, 0 };
    expected_delta_x_after_drag[i] = big_negative_delta;
    if (i + 1 < browser_view_count) {
      expected_delta_x_after_drag[i + 1] =
          browser_views[i]->panel()->GetRestoredBounds().x() -
          browser_views[i + 1]->panel()->GetRestoredBounds().x();
    }
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     big_negative_delta,
                     big_negative_delta,
                     expected_delta_x_after_drag,
                     expected_delta_x_without_change,
                     true);

    int expected_delta_x_after_drag2[] = { 0, 0, 0 };
    expected_delta_x_after_drag2[i] = big_positive_delta;
    if (static_cast<int>(i) - 1 >= 0) {
      expected_delta_x_after_drag2[i - 1] =
          browser_views[i]->panel()->GetRestoredBounds().x() -
          browser_views[i - 1]->panel()->GetRestoredBounds().x();
    }
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     big_positive_delta,
                     big_positive_delta,
                     expected_delta_x_after_drag2,
                     expected_delta_x_without_change,
                     true);
  }

  // Tests dragging a panel with big delta.
  // We should move to the new position after the dragging is ended.
  for (size_t i = 0; i < browser_view_count; ++i) {
    int expected_delta_x_after_drag[] = { 0, 0, 0 };
    int expected_delta_x_after_release[] = { 0, 0, 0 };
    expected_delta_x_after_drag[i] = big_negative_delta;
    if (i + 1 < browser_view_count) {
      expected_delta_x_after_drag[i + 1] =
          browser_views[i]->panel()->GetRestoredBounds().x() -
          browser_views[i + 1]->panel()->GetRestoredBounds().x();
      expected_delta_x_after_release[i + 1] =
          expected_delta_x_after_drag[i + 1];
      expected_delta_x_after_release[i] =
          browser_views[i + 1]->panel()->GetRestoredBounds().x() -
          browser_views[i]->panel()->GetRestoredBounds().x();
    }
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     big_negative_delta,
                     big_negative_delta,
                     expected_delta_x_after_drag,
                     expected_delta_x_after_release,
                     false);

    int expected_delta_x_after_drag2[] = { 0, 0, 0 };
    int expected_delta_x_after_release2[] = { 0, 0, 0 };
    expected_delta_x_after_drag2[i] = big_positive_delta;
    if (static_cast<int>(i) - 1 >= 0) {
      expected_delta_x_after_drag2[i - 1] =
          browser_views[i]->panel()->GetRestoredBounds().x() -
          browser_views[i - 1]->panel()->GetRestoredBounds().x();
      expected_delta_x_after_release2[i - 1] =
          expected_delta_x_after_drag2[i - 1];
      expected_delta_x_after_release2[i] =
          browser_views[i - 1]->panel()->GetRestoredBounds().x() -
          browser_views[i]->panel()->GetRestoredBounds().x();
    }
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     big_positive_delta,
                     big_positive_delta,
                     expected_delta_x_after_drag2,
                     expected_delta_x_after_release2,
                     false);
  }

  // Tests that no dragging is involved.
  for (size_t i = 0; i < browser_view_count; ++i) {
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     0,
                     0,
                     expected_delta_x_without_change,
                     expected_delta_x_without_change,
                     false);
  }

  // Closes all panels.
  for (size_t i = 0; i < browser_view_count; ++i) {
    browser_views[i]->panel()->Close();
    EXPECT_FALSE(browser_views[i]->panel());
  }
}
#endif
