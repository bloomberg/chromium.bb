// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/task_manager/task_manager_columns.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "chrome/browser/ui/views/new_task_manager_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/test_utils.h"
#include "ui/views/controls/table/table_view.h"

namespace task_management {

class NewTaskManagerViewTest : public InProcessBrowserTest {
 public:
  NewTaskManagerViewTest() {}
  ~NewTaskManagerViewTest() override {}

  void TearDownOnMainThread() override {
    // Make sure the task manager is closed (if any).
    chrome::HideTaskManager();
    content::RunAllPendingInMessageLoop();
    ASSERT_FALSE(GetView());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  NewTaskManagerView* GetView() const {
    return NewTaskManagerView::GetInstanceForTests();
  }

  views::TableView* GetTable() const {
    return GetView() ? GetView()->tab_table_ : nullptr;
  }

  void ClearStoredColumnSettings() const {
    PrefService* local_state = g_browser_process->local_state();
    if (!local_state)
      FAIL();

    DictionaryPrefUpdate dict_update(local_state,
                                     prefs::kTaskManagerColumnVisibility);
    dict_update->Clear();
  }

  void ToggleColumnVisibility(NewTaskManagerView* view, int col_id) {
    DCHECK(view);
    view->table_model_->ToggleColumnVisibility(col_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NewTaskManagerViewTest);
};

// Tests that all defined columns have a corresponding string IDs for keying
// into the user preferences dictionary.
IN_PROC_BROWSER_TEST_F(NewTaskManagerViewTest, AllColumnsHaveStringIds) {
  for (size_t i = 0; i < kColumnsSize; ++i)
    EXPECT_NE("", GetColumnIdAsString(kColumns[i].id));
}

// In the case of no settings stored in the user preferences local store, test
// that the task manager table starts with the default columns visibility as
// stored in |kColumns|.
IN_PROC_BROWSER_TEST_F(NewTaskManagerViewTest, TableStartsWithDefaultColumns) {
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
IN_PROC_BROWSER_TEST_F(NewTaskManagerViewTest, ColumnsSettingsAreRestored) {
  ASSERT_NO_FATAL_FAILURE(ClearStoredColumnSettings());

  chrome::ShowTaskManager(browser());
  NewTaskManagerView* view = GetView();
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

}  // namespace task_management

