// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NEW_TASK_MANAGER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NEW_TASK_MANAGER_VIEW_H_

#include <vector>

#include "chrome/browser/ui/host_desktop.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/models/table_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class LabelButton;
class Link;
class TableView;
class View;
}  // namespace views

namespace task_management {

// A collection of data to be used in the construction of a task manager table
// column.
struct TableColumnData {
  // The generated ID of the column. These can change from one build to another.
  // Their values are controlled by the generation from generated_resources.grd.
  int id;

  // The alignment of the text displayed in this column.
  ui::TableColumn::Alignment align;

  // |width| and |percent| used to define the size of the column. See
  // ui::TableColumn::width and ui::TableColumn::percent for details.
  int width;
  float percent;

  // Is the column sortable.
  bool sortable;

  // Is the initial sort order ascending?
  bool initial_sort_is_ascending;

  // The default visibility of this column at startup of the table if no
  // visibility is stored for it in the prefs.
  bool default_visibility;
};

// The task manager table columns and their properties.
extern const TableColumnData kColumns[];
extern const size_t kColumnsSize;

// Session Restore Keys.
extern const char kSortColumnIdKey[];
extern const char kSortIsAscendingKey[];

// Returns the |column_id| as a string value to be used as keys in the user
// preferences.
std::string GetColumnIdAsString(int column_id);

// The new task manager UI container.
class NewTaskManagerView
    : public views::ButtonListener,
      public views::DialogDelegateView,
      public views::TableViewObserver,
      public views::LinkListener,
      public views::ContextMenuController,
      public ui::SimpleMenuModel::Delegate {
 public:
  ~NewTaskManagerView() override;

  // Shows the Task Manager window, or re-activates an existing one.
  static void Show(Browser* browser);

  // Hides the Task Manager if it is showing.
  static void Hide();

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

  // views::TableViewObserver:
  void OnSelectionChanged() override;
  void OnDoubleClick() override;
  void OnKeyDown(ui::KeyboardCode keycode) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

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
  class TableModel;

  explicit NewTaskManagerView(chrome::HostDesktopType desktop_type);

  static NewTaskManagerView* GetInstanceForTests();

  // Creates the child controls.
  void Init();

  // Initializes the state of the always-on-top setting as the window is shown.
  void InitAlwaysOnTopState();

  // Activates the tab associated with the focused row.
  void ActivateFocusedTab();

  // Restores saved "always on top" state from a previous session.
  void RetriveSavedAlwaysOnTopState();

  // Restores the saved columns settings from a previous session into
  // |columns_settings_| and updates the table view.
  void RetrieveSavedColumnsSettingsAndUpdateTable();

  // Stores the current values in |column_settings_| to the user prefs so that
  // it can be restored later next time the task manager view is opened.
  void StoreColumnsSettings();

  void ToggleColumnVisibility(int column_id);

  scoped_ptr<NewTaskManagerView::TableModel> table_model_;

  scoped_ptr<views::MenuRunner> menu_runner_;

  // Contains either the column settings retrieved from user preferences if it
  // exists, or the default column settings.
  // The columns settings are the visible columns and the last sorted column
  // and the direction of the sort.
  scoped_ptr<base::DictionaryValue> columns_settings_;

  // We need to own the text of the menu, the Windows API does not copy it.
  base::string16 always_on_top_menu_text_;

  views::LabelButton* kill_button_;
  views::Link* about_memory_link_;
  views::TableView* tab_table_;
  views::View* tab_table_parent_;

  // all possible columns, not necessarily visible
  std::vector<ui::TableColumn> columns_;

  // The host desktop type this task manager belongs to.
  const chrome::HostDesktopType desktop_type_;

  // True when the Task Manager window should be shown on top of other windows.
  bool is_always_on_top_;

  DISALLOW_COPY_AND_ASSIGN(NewTaskManagerView);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_UI_VIEWS_NEW_TASK_MANAGER_VIEW_H_
