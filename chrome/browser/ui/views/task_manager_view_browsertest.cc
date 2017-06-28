// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/task_manager/task_manager_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/task_manager/task_manager_columns.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "chrome/browser/ui/views/task_manager_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/controls/table/table_view.h"

namespace task_manager {

using browsertest_util::WaitForTaskManagerRows;

class TaskManagerViewTest : public InProcessBrowserTest {
 public:
  TaskManagerViewTest() {}
  ~TaskManagerViewTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Make sure the task manager is closed (if any).
    chrome::HideTaskManager();
    content::RunAllPendingInMessageLoop();
    ASSERT_FALSE(GetView());
  }

  TaskManagerView* GetView() const {
    return TaskManagerView::GetInstanceForTests();
  }

  views::TableView* GetTable() const {
    return GetView() ? GetView()->tab_table_ : nullptr;
  }

  void PressKillButton() { GetView()->Accept(); }

  void ClearStoredColumnSettings() const {
    PrefService* local_state = g_browser_process->local_state();
    if (!local_state)
      FAIL();

    DictionaryPrefUpdate dict_update(local_state,
                                     prefs::kTaskManagerColumnVisibility);
    dict_update->Clear();
  }

  void ToggleColumnVisibility(TaskManagerView* view, int col_id) {
    DCHECK(view);
    view->table_model_->ToggleColumnVisibility(col_id);
  }

  // Looks up a tab based on its tab ID.
  content::WebContents* FindWebContentsByTabId(SessionID::id_type tab_id) {
    for (TabContentsIterator it; !it.done(); it.Next()) {
      if (SessionTabHelper::IdForTab(*it) == tab_id)
        return *it;
    }
    return nullptr;
  }

  // Returns the current TaskManagerTableModel index for a particular tab. Don't
  // cache this value, since it can change whenever the message loop runs.
  int FindRowForTab(content::WebContents* tab) {
    int32_t tab_id = SessionTabHelper::IdForTab(tab);
    std::unique_ptr<TaskManagerTester> tester =
        TaskManagerTester::Create(base::Closure());
    for (int i = 0; i < tester->GetRowCount(); ++i) {
      if (tester->GetTabId(i) == tab_id)
        return i;
    }
    return -1;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskManagerViewTest);
};

// Tests that all defined columns have a corresponding string IDs for keying
// into the user preferences dictionary.
IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, AllColumnsHaveStringIds) {
  for (size_t i = 0; i < kColumnsSize; ++i)
    EXPECT_NE("", GetColumnIdAsString(kColumns[i].id));
}

// In the case of no settings stored in the user preferences local store, test
// that the task manager table starts with the default columns visibility as
// stored in |kColumns|.
IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, TableStartsWithDefaultColumns) {
  ASSERT_NO_FATAL_FAILURE(ClearStoredColumnSettings());

  chrome::ShowTaskManager(browser());
  views::TableView* table = GetTable();
  ASSERT_TRUE(table);

  EXPECT_FALSE(table->is_sorted());
  for (size_t i = 0; i < kColumnsSize; ++i) {
    EXPECT_EQ(kColumns[i].default_visibility,
              table->IsColumnVisible(kColumns[i].id));
  }
}

// Tests that changing columns visibility and sort order will be stored upon
// closing the task manager view and restored when re-opened.
IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, ColumnsSettingsAreRestored) {
  ASSERT_NO_FATAL_FAILURE(ClearStoredColumnSettings());

  chrome::ShowTaskManager(browser());
  TaskManagerView* view = GetView();
  ASSERT_TRUE(view);
  views::TableView* table = GetTable();
  ASSERT_TRUE(table);

  // Toggle the visibility of all columns.
  EXPECT_FALSE(table->is_sorted());
  for (size_t i = 0; i < kColumnsSize; ++i) {
    EXPECT_EQ(kColumns[i].default_visibility,
              table->IsColumnVisible(kColumns[i].id));
    ToggleColumnVisibility(view, kColumns[i].id);
  }

  // Sort by the first visible and initially ascending sortable column.
  bool is_sorted = false;
  int sorted_col_id = -1;
  for (size_t i = 0; i < table->visible_columns().size(); ++i) {
    const ui::TableColumn& column = table->visible_columns()[i].column;
    if (column.sortable && column.initial_sort_is_ascending) {
      // Toggle the sort twice for a descending sort.
      table->ToggleSortOrder(static_cast<int>(i));
      table->ToggleSortOrder(static_cast<int>(i));
      is_sorted = true;
      sorted_col_id = column.id;
      break;
    }
  }

  if (is_sorted) {
    EXPECT_TRUE(table->is_sorted());
    EXPECT_FALSE(table->sort_descriptors().front().ascending);
    EXPECT_EQ(table->sort_descriptors().front().column_id, sorted_col_id);
  }

  // Close the task manager view and re-open. Expect the inverse of the default
  // visibility, and the last sort order.
  chrome::HideTaskManager();
  content::RunAllPendingInMessageLoop();
  ASSERT_FALSE(GetView());
  chrome::ShowTaskManager(browser());
  view = GetView();
  ASSERT_TRUE(view);
  table = GetTable();
  ASSERT_TRUE(table);

  if (is_sorted) {
    EXPECT_TRUE(table->is_sorted());
    EXPECT_FALSE(table->sort_descriptors().front().ascending);
    EXPECT_EQ(table->sort_descriptors().front().column_id, sorted_col_id);
  }
  for (size_t i = 0; i < kColumnsSize; ++i) {
    EXPECT_EQ(!kColumns[i].default_visibility,
              table->IsColumnVisible(kColumns[i].id));
  }
}

IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, InitialSelection) {
  // Navigate the first tab.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title2.html"));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("b.com", "/title3.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // When the task manager is initially shown, the row for the active tab should
  // be selected.
  chrome::ShowTaskManager(browser());

  EXPECT_EQ(1UL, GetTable()->selection_model().size());
  EXPECT_EQ(GetTable()->FirstSelectedRow(),
            FindRowForTab(browser()->tab_strip_model()->GetWebContentsAt(1)));

  // Activate tab 0. The selection should not change.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_EQ(1UL, GetTable()->selection_model().size());
  EXPECT_EQ(GetTable()->FirstSelectedRow(),
            FindRowForTab(browser()->tab_strip_model()->GetWebContentsAt(1)));

  // If the user re-triggers chrome::ShowTaskManager (e.g. via shift-esc), this
  // should set the TaskManager selection to the active tab.
  chrome::ShowTaskManager(browser());

  EXPECT_EQ(1UL, GetTable()->selection_model().size());
  EXPECT_EQ(GetTable()->FirstSelectedRow(),
            FindRowForTab(browser()->tab_strip_model()->GetWebContentsAt(0)));
}

IN_PROC_BROWSER_TEST_F(TaskManagerViewTest, SelectionConsistency) {
  ASSERT_NO_FATAL_FAILURE(ClearStoredColumnSettings());

  chrome::ShowTaskManager(browser());

  // Set up a total of three tabs in different processes.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title2.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("b.com", "/title2.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("c.com", "/title2.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Wait for their titles to appear in the TaskManager. There should be three
  // rows.
  auto pattern = browsertest_util::MatchTab("Title *");
  int rows = 3;
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(rows, pattern));

  // Find the three tabs we set up, in TaskManager model order. Because we have
  // not sorted the table yet, this should also be their UI display order.
  std::unique_ptr<TaskManagerTester> tester =
      TaskManagerTester::Create(base::Closure());
  std::vector<content::WebContents*> tabs;
  for (int i = 0; i < tester->GetRowCount(); ++i) {
    // Filter based on our title.
    if (!base::MatchPattern(tester->GetRowTitle(i), pattern))
      continue;
    content::WebContents* tab = FindWebContentsByTabId(tester->GetTabId(i));
    EXPECT_NE(nullptr, tab);
    tabs.push_back(tab);
  }
  EXPECT_EQ(3U, tabs.size());

  // Select the middle row, and store its tab id.
  GetTable()->Select(FindRowForTab(tabs[1]));
  EXPECT_EQ(GetTable()->FirstSelectedRow(), FindRowForTab(tabs[1]));
  EXPECT_EQ(1UL, GetTable()->selection_model().size());

  // Add 3 rows above the selection. The selected tab should not change.
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(content::ExecuteScript(tabs[0], "window.open('title3.html');"));
    EXPECT_EQ(GetTable()->FirstSelectedRow(), FindRowForTab(tabs[1]));
  }
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows += 3), pattern));
  EXPECT_EQ(GetTable()->FirstSelectedRow(), FindRowForTab(tabs[1]));
  EXPECT_EQ(1UL, GetTable()->selection_model().size());

  // Add 2 rows below the selection. The selected tab should not change.
  for (int i = 0; i < 2; ++i) {
    ASSERT_TRUE(content::ExecuteScript(tabs[2], "window.open('title3.html');"));
    EXPECT_EQ(GetTable()->FirstSelectedRow(), FindRowForTab(tabs[1]));
  }
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows += 2), pattern));
  EXPECT_EQ(GetTable()->FirstSelectedRow(), FindRowForTab(tabs[1]));
  EXPECT_EQ(1UL, GetTable()->selection_model().size());

  // Add a new row in the same process as the selection. The selected tab should
  // not change.
  ASSERT_TRUE(content::ExecuteScript(tabs[1], "window.open('title3.html');"));
  EXPECT_EQ(GetTable()->FirstSelectedRow(), FindRowForTab(tabs[1]));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows += 1), pattern));
  EXPECT_EQ(GetTable()->FirstSelectedRow(), FindRowForTab(tabs[1]));
  EXPECT_EQ(1UL, GetTable()->selection_model().size());

  // Press the button, which kills the process of the selected row.
  PressKillButton();

  // Two rows should disappear.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows -= 2), pattern));

  // A later row should now be selected. The selection should be after the 4
  // rows sharing the tabs[0] process, and it should be at or before
  // the tabs[2] row.
  ASSERT_LT(FindRowForTab(tabs[0]) + 3, GetTable()->FirstSelectedRow());
  ASSERT_LE(GetTable()->FirstSelectedRow(), FindRowForTab(tabs[2]));

  // Now select tabs[2].
  GetTable()->Select(FindRowForTab(tabs[2]));

  // Focus and reload one of the sad tabs. It should reappear in the TM. The
  // other sad tab should not reappear.
  tabs[1]->GetDelegate()->ActivateContents(tabs[1]);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows += 1), pattern));

  // tabs[2] should still be selected.
  EXPECT_EQ(GetTable()->FirstSelectedRow(), FindRowForTab(tabs[2]));

  // Close tabs[0]. The selection should not change.
  chrome::CloseWebContents(browser(), tabs[0], false);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows((rows -= 1), pattern));
  EXPECT_EQ(GetTable()->FirstSelectedRow(), FindRowForTab(tabs[2]));
}

}  // namespace task_manager
