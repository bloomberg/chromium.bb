// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NEW_TASK_MANAGER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NEW_TASK_MANAGER_VIEW_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/models/table_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

namespace views {
class LabelButton;
class Link;
class TableView;
class View;
}  // namespace views

namespace task_management {

class TaskManagerTableModel;

// The new task manager UI container.
class NewTaskManagerView
    : public TableViewDelegate,
      public views::ButtonListener,
      public views::DialogDelegateView,
      public views::TableGrouper,
      public views::TableViewObserver,
      public views::ContextMenuController,
      public ui::SimpleMenuModel::Delegate {
 public:
  ~NewTaskManagerView() override;

  // Shows the Task Manager window, or re-activates an existing one.
  static task_management::TaskManagerTableModel* Show(Browser* browser);

  // Hides the Task Manager if it is showing.
  static void Hide();

  // task_management::TableViewDelegate:
  bool IsColumnVisible(int column_id) const override;
  void SetColumnVisibility(int column_id, bool new_visibility) override;
  bool IsTableSorted() const override;
  TableSortDescriptor GetSortDescriptor() const override;
  void ToggleSortOrder(int visible_column_index) override;

  // views::View:
  void Layout() override;
  gfx::Size GetPreferredSize() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::DialogDelegateView:
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool ExecuteWindowsCommand(int command_id) override;
  base::string16 GetWindowTitle() const override;
  std::string GetWindowName() const override;
  int GetDialogButtons() const override;
  void WindowClosing() override;
  bool UseNewStyleForThisDialog() const override;

  // views::TableGrouper:
  void GetGroupRange(int model_index, views::GroupRange* range) override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;
  void OnDoubleClick() override;
  void OnKeyDown(ui::KeyboardCode keycode) override;

  // views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int id) const override;
  bool IsCommandIdEnabled(int id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;
  void ExecuteCommand(int id, int event_flags) override;

 private:
  friend class NewTaskManagerViewTest;

  NewTaskManagerView();

  static NewTaskManagerView* GetInstanceForTests();

  // Creates the child controls.
  void Init();

  // Initializes the state of the always-on-top setting as the window is shown.
  void InitAlwaysOnTopState();

  // Activates the tab associated with the focused row.
  void ActivateFocusedTab();

  // Restores saved "always on top" state from a previous session.
  void RetriveSavedAlwaysOnTopState();

  std::unique_ptr<TaskManagerTableModel> table_model_;

  std::unique_ptr<views::MenuRunner> menu_runner_;

  // We need to own the text of the menu, the Windows API does not copy it.
  base::string16 always_on_top_menu_text_;

  views::LabelButton* kill_button_;
  views::TableView* tab_table_;
  views::View* tab_table_parent_;

  // all possible columns, not necessarily visible
  std::vector<ui::TableColumn> columns_;

  // True when the Task Manager window should be shown on top of other windows.
  bool is_always_on_top_;

  DISALLOW_COPY_AND_ASSIGN(NewTaskManagerView);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_UI_VIEWS_NEW_TASK_MANAGER_VIEW_H_
