// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"

#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/test/button_test_api.h"

class TabGroupEditorBubbleViewDialogBrowserTest : public DialogBrowserTest {
 protected:
  void ShowUi(const std::string& name) override {
    tab_groups::TabGroupId group =
        browser()->tab_strip_model()->AddToNewGroup({0});

    BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
    TabGroupHeader* header = browser_view->tabstrip()->group_header(group);
    ASSERT_NE(nullptr, header);
    ASSERT_FALSE(header->editor_bubble_tracker_.is_open());

    ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, gfx::PointF(),
                                 gfx::PointF(), base::TimeTicks(), 0, 0);
    header->OnMousePressed(pressed_event);

    ASSERT_FALSE(header->editor_bubble_tracker_.is_open());

    ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::PointF(),
                                  gfx::PointF(), base::TimeTicks(), 0, 0);
    header->OnMouseReleased(released_event);

    ASSERT_TRUE(header->editor_bubble_tracker_.is_open());
  }

  static views::Widget* GetEditorBubbleWidget(const TabGroupHeader* header) {
    return header->editor_bubble_tracker_.is_open()
               ? header->editor_bubble_tracker_.widget()
               : nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       NewTabInGroup) {
  ShowUi("SetUp");

  TabGroupModel* group_model = browser()->tab_strip_model()->group_model();
  std::vector<tab_groups::TabGroupId> group_list = group_model->ListTabGroups();
  ASSERT_EQ(1u, group_list.size());
  ASSERT_EQ(1u, group_model->GetTabGroup(group_list[0])->ListTabs().size());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  TabGroupHeader* header =
      browser_view->tabstrip()->group_header(group_list[0]);
  views::Widget* editor_bubble = GetEditorBubbleWidget(header);
  ASSERT_NE(nullptr, editor_bubble);

  views::Button* const new_tab_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP));
  EXPECT_NE(nullptr, new_tab_button);

  ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(new_tab_button).NotifyClick(released_event);

  EXPECT_EQ(2u, group_model->GetTabGroup(group_list[0])->ListTabs().size());
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest, Ungroup) {
  ShowUi("SetUp");

  TabStripModel* tsm = browser()->tab_strip_model();
  ASSERT_EQ(1, tsm->count());
  TabGroupModel* group_model = tsm->group_model();
  std::vector<tab_groups::TabGroupId> group_list = group_model->ListTabGroups();
  ASSERT_EQ(1u, group_list.size());
  ASSERT_EQ(1u, group_model->GetTabGroup(group_list[0])->ListTabs().size());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  TabGroupHeader* header =
      browser_view->tabstrip()->group_header(group_list[0]);
  views::Widget* editor_bubble = GetEditorBubbleWidget(header);
  ASSERT_NE(nullptr, editor_bubble);

  views::Button* const ungroup_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::TAB_GROUP_HEADER_CXMENU_UNGROUP));
  EXPECT_NE(nullptr, ungroup_button);

  ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(ungroup_button).NotifyClick(released_event);

  EXPECT_EQ(0u, group_model->ListTabGroups().size());
  EXPECT_FALSE(group_model->ContainsTabGroup(group_list[0]));
  EXPECT_EQ(1, tsm->count());
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       CloseGroupClosesBrowser) {
  ShowUi("SetUp");

  TabGroupModel* group_model = browser()->tab_strip_model()->group_model();
  std::vector<tab_groups::TabGroupId> group_list = group_model->ListTabGroups();
  ASSERT_EQ(1u, group_list.size());
  ASSERT_EQ(1u, group_model->GetTabGroup(group_list[0])->ListTabs().size());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  TabGroupHeader* header =
      browser_view->tabstrip()->group_header(group_list[0]);
  views::Widget* editor_bubble = GetEditorBubbleWidget(header);
  ASSERT_NE(nullptr, editor_bubble);

  views::Button* const close_group_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP));
  EXPECT_NE(nullptr, close_group_button);

  ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(close_group_button).NotifyClick(released_event);

  EXPECT_EQ(0u, group_model->ListTabGroups().size());
  EXPECT_FALSE(group_model->ContainsTabGroup(group_list[0]));
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_TRUE(browser()->IsAttemptingToCloseBrowser());
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       MoveGroupToNewWindow) {
  ShowUi("SetUp");

  TabGroupModel* group_model = browser()->tab_strip_model()->group_model();
  std::vector<tab_groups::TabGroupId> group_list = group_model->ListTabGroups();
  ASSERT_EQ(1u, group_list.size());
  ASSERT_EQ(1u, group_model->GetTabGroup(group_list[0])->ListTabs().size());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  TabGroupHeader* header =
      browser_view->tabstrip()->group_header(group_list[0]);
  views::Widget* editor_bubble = GetEditorBubbleWidget(header);
  ASSERT_NE(nullptr, editor_bubble);

  views::Button* const move_group_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::
              TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW));
  EXPECT_NE(nullptr, move_group_button);

  ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(move_group_button).NotifyClick(released_event);

  EXPECT_EQ(0u, group_model->ListTabGroups().size());
  EXPECT_FALSE(group_model->ContainsTabGroup(group_list[0]));
  EXPECT_EQ(0, browser()->tab_strip_model()->count());

  BrowserList* browser_list = BrowserList::GetInstance();
  Browser* active_browser = browser_list->GetLastActive();
  ASSERT_NE(active_browser, browser());
  EXPECT_EQ(1, active_browser->tab_strip_model()->count());
  EXPECT_EQ(
      1u,
      active_browser->tab_strip_model()->group_model()->ListTabGroups().size());
}
