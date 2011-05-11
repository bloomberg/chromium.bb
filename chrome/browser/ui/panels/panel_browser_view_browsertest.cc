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
  EXPECT_TRUE(frame_view->Contains(frame_view->options_button_));
  EXPECT_TRUE(frame_view->Contains(frame_view->close_button_));

  // These controls should be visible.
  EXPECT_TRUE(frame_view->title_icon_->IsVisible());
  EXPECT_TRUE(frame_view->title_label_->IsVisible());
  EXPECT_TRUE(frame_view->options_button_->IsVisible());
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
  EXPECT_GT(frame_view->options_button_->width(), 0);
  EXPECT_GT(frame_view->options_button_->height(), 0);
  EXPECT_LT(frame_view->options_button_->height(), title_bar_height);
  EXPECT_GT(frame_view->close_button_->width(), 0);
  EXPECT_GT(frame_view->close_button_->height(), 0);
  EXPECT_LT(frame_view->close_button_->height(), title_bar_height);
  EXPECT_LT(frame_view->title_icon_->x() + frame_view->title_icon_->width(),
      frame_view->title_label_->x());
  EXPECT_LT(frame_view->title_label_->x() + frame_view->title_label_->width(),
      frame_view->options_button_->x());
  EXPECT_LT(
      frame_view->options_button_->x() + frame_view->options_button_->width(),
      frame_view->close_button_->x());

  // Validate that the controls should be updated when the activation state is
  // changed.
  frame_view->UpdateControlStyles(PanelBrowserFrameView::PAINT_AS_ACTIVE);
  SkColor title_label_color1 = frame_view->title_label_->GetColor();
  frame_view->UpdateControlStyles(PanelBrowserFrameView::PAINT_AS_INACTIVE);
  SkColor title_label_color2 = frame_view->title_label_->GetColor();
  EXPECT_NE(title_label_color1, title_label_color2);
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, CreateOrUpdateOptionsMenu) {
  int single_panel_menu[] = { PanelBrowserFrameView::COMMAND_ABOUT };
  size_t single_panel_menu_count = arraysize(single_panel_menu);
  int multi_panel_menu_for_minimize[] = {
      PanelBrowserFrameView::COMMAND_MINIMIZE_ALL,
      PanelBrowserFrameView::COMMAND_CLOSE_ALL,
      -1,  // Separator
      PanelBrowserFrameView::COMMAND_ABOUT };
  size_t multi_panel_menu_for_minimize_count =
      arraysize(multi_panel_menu_for_minimize);
  int multi_panel_menu_for_restore[] = {
      PanelBrowserFrameView::COMMAND_RESTORE_ALL,
      PanelBrowserFrameView::COMMAND_CLOSE_ALL,
      -1,  // Separator
      PanelBrowserFrameView::COMMAND_ABOUT };
  size_t multi_panel_menu_for_restore_count =
      arraysize(multi_panel_menu_for_restore);

  // With only one panel, we should only have 1 menu item: "About this panel".
  PanelBrowserFrameView* frame_view1 =
      CreatePanelBrowserView("PanelTest1")->GetFrameView();

  frame_view1->CreateOrUpdateOptionsMenu();
  ASSERT_TRUE(frame_view1->options_menu_.get());
  ValidateOptionsMenuItems(frame_view1->options_menu_contents_.get(),
                           single_panel_menu_count,
                           single_panel_menu);

  // With another panel, we should have 4 menu items, including separator.
  PanelBrowserFrameView* frame_view2 =
      CreatePanelBrowserView("PanelTest2")->GetFrameView();

  frame_view1->CreateOrUpdateOptionsMenu();
  ValidateOptionsMenuItems(frame_view1->options_menu_contents_.get(),
                           multi_panel_menu_for_minimize_count,
                           multi_panel_menu_for_minimize);

  frame_view2->CreateOrUpdateOptionsMenu();
  ValidateOptionsMenuItems(frame_view2->options_menu_contents_.get(),
                           multi_panel_menu_for_minimize_count,
                           multi_panel_menu_for_minimize);

  // When we minimize one panel, "Minimize all" remains intact.
  frame_view1->browser_view_->panel_->Minimize();

  frame_view1->CreateOrUpdateOptionsMenu();
  ValidateOptionsMenuItems(frame_view2->options_menu_contents_.get(),
                           multi_panel_menu_for_minimize_count,
                           multi_panel_menu_for_minimize);

  frame_view2->CreateOrUpdateOptionsMenu();
  ValidateOptionsMenuItems(frame_view2->options_menu_contents_.get(),
                           multi_panel_menu_for_minimize_count,
                           multi_panel_menu_for_minimize);

  // When we minimize the remaining panel, "Minimize all" should become
  // "Restore all".
  frame_view2->browser_view_->panel_->Minimize();

  frame_view1->CreateOrUpdateOptionsMenu();
  ValidateOptionsMenuItems(frame_view1->options_menu_contents_.get(),
                           multi_panel_menu_for_restore_count,
                           multi_panel_menu_for_restore);

  frame_view2->CreateOrUpdateOptionsMenu();
  ValidateOptionsMenuItems(frame_view2->options_menu_contents_.get(),
                           multi_panel_menu_for_restore_count,
                           multi_panel_menu_for_restore);

  // When we close one panel, we should be back to have only 1 menu item.
  frame_view1->browser_view_->panel_->Close();

  frame_view2->CreateOrUpdateOptionsMenu();
  ValidateOptionsMenuItems(frame_view2->options_menu_contents_.get(),
                           single_panel_menu_count,
                           single_panel_menu);
}
#endif
