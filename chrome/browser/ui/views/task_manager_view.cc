// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/new_task_manager_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/models/table_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(USE_ASH)
#include "ash/shelf/shelf_util.h"
#include "ash/wm/window_util.h"
#include "grit/ash_resources.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/shell_integration.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace {

////////////////////////////////////////////////////////////////////////////////
// TaskManagerTableModel class
////////////////////////////////////////////////////////////////////////////////

class TaskManagerTableModel
    : public ui::TableModel,
      public views::TableGrouper,
      public TaskManagerModelObserver {
 public:
  explicit TaskManagerTableModel(TaskManagerModel* model)
      : model_(model),
        observer_(NULL) {
    model_->AddObserver(this);
  }

  ~TaskManagerTableModel() override { model_->RemoveObserver(this); }

  // TableModel overrides:
  int RowCount() override;
  base::string16 GetText(int row, int column) override;
  gfx::ImageSkia GetIcon(int row) override;
  void SetObserver(ui::TableModelObserver* observer) override;
  int CompareValues(int row1, int row2, int column_id) override;

  // TableGrouper overrides:
  void GetGroupRange(int model_index, views::GroupRange* range) override;

  // TaskManagerModelObserver overrides:
  void OnModelChanged() override;
  void OnItemsChanged(int start, int length) override;
  void OnItemsAdded(int start, int length) override;
  void OnItemsRemoved(int start, int length) override;

 private:
  TaskManagerModel* model_;
  ui::TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerTableModel);
};

int TaskManagerTableModel::RowCount() {
  return model_->ResourceCount();
}

base::string16 TaskManagerTableModel::GetText(int row, int col_id) {
  return model_->GetResourceById(row, col_id);
}

gfx::ImageSkia TaskManagerTableModel::GetIcon(int row) {
  return model_->GetResourceIcon(row);
}

void TaskManagerTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

int TaskManagerTableModel::CompareValues(int row1, int row2, int column_id) {
  return model_->CompareValues(row1, row2, column_id);
}

void TaskManagerTableModel::GetGroupRange(int model_index,
                                          views::GroupRange* range) {
  TaskManagerModel::GroupRange range_pair =
      model_->GetGroupRangeForResource(model_index);
  range->start = range_pair.first;
  range->length = range_pair.second;
}

void TaskManagerTableModel::OnModelChanged() {
  if (observer_)
    observer_->OnModelChanged();
}

void TaskManagerTableModel::OnItemsChanged(int start, int length) {
  if (observer_)
    observer_->OnItemsChanged(start, length);
}

void TaskManagerTableModel::OnItemsAdded(int start, int length) {
  if (observer_)
    observer_->OnItemsAdded(start, length);
}

void TaskManagerTableModel::OnItemsRemoved(int start, int length) {
  if (observer_)
    observer_->OnItemsRemoved(start, length);
}

// The Task Manager UI container.
class TaskManagerView : public views::ButtonListener,
                        public views::DialogDelegateView,
                        public views::TableViewObserver,
                        public views::LinkListener,
                        public views::ContextMenuController,
                        public ui::SimpleMenuModel::Delegate {
 public:
  explicit TaskManagerView(chrome::HostDesktopType desktop_type);
  ~TaskManagerView() override;

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
  // Creates the child controls.
  void Init();

  // Initializes the state of the always-on-top setting as the window is shown.
  void InitAlwaysOnTopState();

  // Activates the tab associated with the focused row.
  void ActivateFocusedTab();

  // Restores saved always on top state from a previous session.
  bool GetSavedAlwaysOnTopState(bool* always_on_top) const;

  views::LabelButton* kill_button_;
  views::Link* about_memory_link_;
  views::TableView* tab_table_;
  views::View* tab_table_parent_;

  TaskManager* task_manager_;

  TaskManagerModel* model_;

  // all possible columns, not necessarily visible
  std::vector<ui::TableColumn> columns_;

  scoped_ptr<TaskManagerTableModel> table_model_;

  // True when the Task Manager window should be shown on top of other windows.
  bool is_always_on_top_;

  // The host desktop type this task manager belongs to.
  const chrome::HostDesktopType desktop_type_;

  // We need to own the text of the menu, the Windows API does not copy it.
  base::string16 always_on_top_menu_text_;

  // An open Task manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static TaskManagerView* instance_;

  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerView);
};

// static
TaskManagerView* TaskManagerView::instance_ = NULL;


TaskManagerView::TaskManagerView(chrome::HostDesktopType desktop_type)
    : kill_button_(NULL),
      about_memory_link_(NULL),
      tab_table_(NULL),
      tab_table_parent_(NULL),
      task_manager_(TaskManager::GetInstance()),
      model_(TaskManager::GetInstance()->model()),
      is_always_on_top_(false),
      desktop_type_(desktop_type) {
  Init();
}

TaskManagerView::~TaskManagerView() {
  // Delete child views now, while our table model still exists.
  RemoveAllChildViews(true);
}

void TaskManagerView::Init() {
  table_model_.reset(new TaskManagerTableModel(model_));

  // Page column has no header label.
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_TASK_COLUMN,
                                     ui::TableColumn::LEFT, -1, 1));
  columns_.back().sortable = true;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_PROFILE_NAME_COLUMN,
                                     ui::TableColumn::LEFT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_SHARED_MEM_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_CPU_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_NET_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_PROCESS_ID_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
#if defined(OS_WIN)
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_GDI_HANDLES_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_USER_HANDLES_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
#endif
  columns_.push_back(ui::TableColumn(
      IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN,
      ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  columns_.push_back(ui::TableColumn(
      IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN,
      ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  columns_.push_back(ui::TableColumn(
      IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN,
      ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(
      ui::TableColumn(IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN,
                      ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  // TODO(port) http://crbug.com/120488 for non-Linux.
#if defined(OS_LINUX)
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
#endif

  tab_table_ = new views::TableView(
      table_model_.get(), columns_, views::ICON_AND_TEXT, false);
  tab_table_->SetGrouper(table_model_.get());

  // Hide some columns by default
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PROFILE_NAME_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_SHARED_MEM_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(
      IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_GDI_HANDLES_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_USER_HANDLES_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN, false);

  tab_table_->SetObserver(this);
  tab_table_->set_context_menu_controller(this);
  set_context_menu_controller(this);
  kill_button_ = new views::LabelButton(this,
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_KILL));
  kill_button_->SetStyle(views::Button::STYLE_BUTTON);
  about_memory_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_ABOUT_MEMORY_LINK));
  about_memory_link_->set_listener(this);

  // Makes sure our state is consistent.
  OnSelectionChanged();

  ui::Accelerator ctrl_w(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  AddAccelerator(ctrl_w);
}

void TaskManagerView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  views::DialogDelegateView::ViewHierarchyChanged(details);
  // Since we want the Kill button and the Memory Details link to show up in
  // the same visual row as the close button, which is provided by the
  // framework, we must add the buttons to the non-client view, which is the
  // parent of this view. Similarly, when we're removed from the view
  // hierarchy, we must take care to clean up those items as well.
  if (details.child == this) {
    if (details.is_add) {
      details.parent->AddChildView(about_memory_link_);
      details.parent->AddChildView(kill_button_);
      tab_table_parent_ = tab_table_->CreateParentIfNecessary();
      AddChildView(tab_table_parent_);
    } else {
      details.parent->RemoveChildView(kill_button_);
      details.parent->RemoveChildView(about_memory_link_);
    }
  }
}

void TaskManagerView::Layout() {
  gfx::Size size = kill_button_->GetPreferredSize();
  gfx::Rect parent_bounds = parent()->GetContentsBounds();
  const int horizontal_margin = views::kPanelHorizMargin;
  const int vertical_margin = views::kButtonVEdgeMargin;
  int x = width() - size.width() - horizontal_margin;
  int y_buttons = parent_bounds.bottom() - size.height() - vertical_margin;
  kill_button_->SetBounds(x, y_buttons, size.width(), size.height());

  size = about_memory_link_->GetPreferredSize();
  about_memory_link_->SetBounds(
      horizontal_margin,
      y_buttons + (kill_button_->height() - size.height()) / 2,
      size.width(), size.height());

  gfx::Rect rect = GetLocalBounds();
  rect.Inset(horizontal_margin, views::kPanelVertMargin);
  rect.Inset(0, 0, 0,
             kill_button_->height() + views::kUnrelatedControlVerticalSpacing);
  tab_table_parent_->SetBoundsRect(rect);
}

gfx::Size TaskManagerView::GetPreferredSize() const {
  return gfx::Size(460, 270);
}

bool TaskManagerView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  DCHECK_EQ(ui::VKEY_W, accelerator.key_code());
  DCHECK_EQ(ui::EF_CONTROL_DOWN, accelerator.modifiers());
  GetWidget()->Close();
  return true;
}

// static
void TaskManagerView::Show(Browser* browser) {
  // In ash we can come here through the ChromeShellDelegate. If there is no
  // browser window at that time of the call, browser could be passed as NULL.
  const chrome::HostDesktopType desktop_type =
      browser ? browser->host_desktop_type() : chrome::HOST_DESKTOP_TYPE_ASH;

  if (instance_) {
    // If there's a Task manager window open already, just activate it.
    instance_->GetWidget()->Activate();
    return;
  }
  instance_ = new TaskManagerView(desktop_type);
  gfx::NativeWindow window =
      browser ? browser->window()->GetNativeWindow() : NULL;
#if defined(USE_ASH)
  if (!window)
    window = ash::wm::GetActiveWindow();
#endif
  DialogDelegate::CreateDialogWidget(instance_, window, NULL);
  instance_->InitAlwaysOnTopState();
  instance_->model_->StartUpdating();
#if defined(OS_WIN)
  // Set the app id for the task manager to the app id of its parent browser. If
  // no parent is specified, the app id will default to that of the initial
  // process.
  if (browser) {
    ui::win::SetAppIdForWindow(
        ShellIntegration::GetChromiumModelIdForProfile(
            browser->profile()->GetPath()),
        views::HWNDForWidget(instance_->GetWidget()));
  }
#endif
  instance_->GetWidget()->Show();

  // Set the initial focus to the list of tasks.
  views::FocusManager* focus_manager = instance_->GetFocusManager();
  if (focus_manager)
    focus_manager->SetFocusedView(instance_->tab_table_);

#if defined(USE_ASH)
  gfx::NativeWindow native_window = instance_->GetWidget()->GetNativeWindow();
  ash::SetShelfItemDetailsForDialogWindow(
      native_window, IDR_ASH_SHELF_ICON_TASK_MANAGER, native_window->title());
#endif
}

// static
void TaskManagerView::Hide() {
  if (instance_)
    instance_->GetWidget()->Close();
}

// ButtonListener implementation.
void TaskManagerView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  typedef ui::ListSelectionModel::SelectedIndices SelectedIndices;
  DCHECK_EQ(kill_button_, sender);
  SelectedIndices selection(tab_table_->selection_model().selected_indices());
  for (SelectedIndices::const_reverse_iterator i = selection.rbegin();
        i != selection.rend(); ++i) {
    task_manager_->KillProcess(*i);
  }
}

// DialogDelegate implementation.
bool TaskManagerView::CanResize() const {
  return true;
}

bool TaskManagerView::CanMaximize() const {
  return true;
}

bool TaskManagerView::CanMinimize() const {
  return true;
}

bool TaskManagerView::ExecuteWindowsCommand(int command_id) {
  return false;
}

base::string16 TaskManagerView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_TITLE);
}

std::string TaskManagerView::GetWindowName() const {
  return prefs::kTaskManagerWindowPlacement;
}

int TaskManagerView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void TaskManagerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  // (WindowClosing comes in asynchronously from the call to Close() and we
  // may have already opened a new instance).
  if (instance_ == this)
    instance_ = NULL;
  task_manager_->OnWindowClosed();
}

bool TaskManagerView::UseNewStyleForThisDialog() const {
  return false;
}

// views::TableViewObserver implementation.
void TaskManagerView::OnSelectionChanged() {
  const ui::ListSelectionModel::SelectedIndices& selection(
      tab_table_->selection_model().selected_indices());
  bool selection_contains_browser_process = false;
  for (size_t i = 0; i < selection.size(); ++i) {
    if (task_manager_->IsBrowserProcess(selection[i])) {
      selection_contains_browser_process = true;
      break;
    }
  }
  kill_button_->SetEnabled(!selection_contains_browser_process &&
                           !selection.empty());
}

void TaskManagerView::OnDoubleClick() {
  ActivateFocusedTab();
}

void TaskManagerView::OnKeyDown(ui::KeyboardCode keycode) {
  if (keycode == ui::VKEY_RETURN)
    ActivateFocusedTab();
}

void TaskManagerView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK_EQ(about_memory_link_, source);
  task_manager_->OpenAboutMemory(desktop_type_);
}

void TaskManagerView::ShowContextMenuForView(views::View* source,
                                             const gfx::Point& point,
                                             ui::MenuSourceType source_type) {
  ui::SimpleMenuModel menu_model(this);
  for (std::vector<ui::TableColumn>::iterator i(columns_.begin());
       i != columns_.end(); ++i) {
    menu_model.AddCheckItem(i->id, l10n_util::GetStringUTF16(i->id));
  }
  menu_runner_.reset(
      new views::MenuRunner(&menu_model, views::MenuRunner::CONTEXT_MENU));
  if (menu_runner_->RunMenuAt(GetWidget(),
                              NULL,
                              gfx::Rect(point, gfx::Size()),
                              views::MENU_ANCHOR_TOPLEFT,
                              source_type) == views::MenuRunner::MENU_DELETED) {
    return;
  }
}

bool TaskManagerView::IsCommandIdChecked(int id) const {
  return tab_table_->IsColumnVisible(id);
}

bool TaskManagerView::IsCommandIdEnabled(int id) const {
  return true;
}

bool TaskManagerView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void TaskManagerView::ExecuteCommand(int id, int event_flags) {
  tab_table_->SetColumnVisibility(id, !tab_table_->IsColumnVisible(id));
}

void TaskManagerView::InitAlwaysOnTopState() {
  is_always_on_top_ = false;
  if (GetSavedAlwaysOnTopState(&is_always_on_top_))
    GetWidget()->SetAlwaysOnTop(is_always_on_top_);
}

void TaskManagerView::ActivateFocusedTab() {
  const int active_row = tab_table_->selection_model().active();
  if (active_row != -1)
    task_manager_->ActivateProcess(active_row);
}

bool TaskManagerView::GetSavedAlwaysOnTopState(bool* always_on_top) const {
  if (!g_browser_process->local_state())
    return false;

  const base::DictionaryValue* dictionary =
      g_browser_process->local_state()->GetDictionary(GetWindowName());
  return dictionary &&
      dictionary->GetBoolean("always_on_top", always_on_top) && always_on_top;
}

}  // namespace

namespace chrome {

// Declared in browser_dialogs.h so others don't need to depend on our header.
void ShowTaskManager(Browser* browser) {
  if (switches::NewTaskManagerEnabled()) {
    task_management::NewTaskManagerView::Show(browser);
    return;
  }

  TaskManagerView::Show(browser);
}

void HideTaskManager() {
  if (switches::NewTaskManagerEnabled()) {
    task_management::NewTaskManagerView::Hide();
    return;
  }

  TaskManagerView::Hide();
}

}  // namespace chrome
