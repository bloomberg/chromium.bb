// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/metrics/stats_table.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/memory_purger.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/models/table_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/controls/table/table_view_row_background_painter.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_WIN)
#include "win8/util/win8_util.h"
#endif

// Yellow highlight used when highlighting background resources.
static const SkColor kBackgroundResourceHighlight =
    SkColorSetRGB(0xff, 0xf1, 0xcd);

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

  virtual ~TaskManagerTableModel() {
    model_->RemoveObserver(this);
  }

  // TableModel overrides:
  virtual int RowCount() OVERRIDE;
  virtual string16 GetText(int row, int column) OVERRIDE;
  virtual gfx::ImageSkia GetIcon(int row) OVERRIDE;
  virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;
  virtual int CompareValues(int row1, int row2, int column_id) OVERRIDE;

  // TableGrouper overrides:
  virtual void GetGroupRange(int model_index,
                             views::GroupRange* range) OVERRIDE;

  // TaskManagerModelObserver overrides:
  virtual void OnModelChanged() OVERRIDE;
  virtual void OnItemsChanged(int start, int length) OVERRIDE;
  virtual void OnItemsAdded(int start, int length) OVERRIDE;
  virtual void OnItemsRemoved(int start, int length) OVERRIDE;

  // Returns true if resource corresponding to |row| is a background resource.
  bool IsBackgroundResource(int row);

 private:
  TaskManagerModel* model_;
  ui::TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerTableModel);
};

int TaskManagerTableModel::RowCount() {
  return model_->ResourceCount();
}

string16 TaskManagerTableModel::GetText(int row, int col_id) {
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
  // There's a bug in the Windows ListView where inserting items with groups
  // enabled puts them in the wrong position, so we will need to rebuild the
  // list view in this case.
  // (see: http://connect.microsoft.com/VisualStudio/feedback/details/115345/).
  //
  // Turns out, forcing a list view rebuild causes http://crbug.com/69391
  // because items are added to the ListView one-at-a-time when initially
  // displaying the TaskManager, resulting in many ListView rebuilds. So we are
  // no longer forcing a rebuild for now because the current UI doesn't use
  // groups - if we are going to add groups in the upcoming TaskManager UI
  // revamp, we'll need to re-enable this call to OnModelChanged() and also add
  // code to avoid doing multiple rebuilds on startup (maybe just generate a
  // single OnModelChanged() call after the initial population).

  // OnModelChanged();
}

void TaskManagerTableModel::OnItemsRemoved(int start, int length) {
  if (observer_)
    observer_->OnItemsRemoved(start, length);

  // We may need to change the indentation of some items if the topmost item
  // in the group was removed, so update the view.
  OnModelChanged();
}

bool TaskManagerTableModel::IsBackgroundResource(int row) {
  return model_->IsBackgroundResource(row);
}

class BackgroundPainter : public views::TableViewRowBackgroundPainter {
 public:
  explicit BackgroundPainter(TaskManagerTableModel* model) : model_(model) {}
  virtual ~BackgroundPainter() {}

  virtual void PaintRowBackground(int model_index,
                                  const gfx::Rect& row_bounds,
                                  gfx::Canvas* canvas) OVERRIDE {
    if (model_->IsBackgroundResource(model_index))
      canvas->FillRect(row_bounds, kBackgroundResourceHighlight);
  }

 private:
  TaskManagerTableModel* model_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundPainter);
};

// The Task manager UI container.
class TaskManagerView : public views::ButtonListener,
                        public views::DialogDelegateView,
                        public views::TableViewObserver,
                        public views::LinkListener,
                        public views::ContextMenuController,
                        public ui::SimpleMenuModel::Delegate {
 public:
  TaskManagerView(bool highlight_background_resources,
                  chrome::HostDesktopType desktop_type);
  virtual ~TaskManagerView();

  // Shows the Task manager window, or re-activates an existing one. If
  // |highlight_background_resources| is true, highlights the background
  // resources in the resource display.
  static void Show(bool highlight_background_resources, Browser* browser);

  // views::View:
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::DialogDelegateView:
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual bool ExecuteWindowsCommand(int command_id) OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual std::string GetWindowName() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;

  // views::TableViewObserver:
  virtual void OnSelectionChanged() OVERRIDE;
  virtual void OnDoubleClick() OVERRIDE;
  virtual void OnKeyDown(ui::KeyboardCode keycode) OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Called by the column picker to pick up any new stat counters that
  // may have appeared since last time.
  void UpdateStatsCounters();

  // views::ContextMenuController:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& point) OVERRIDE;

  // ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int id) OVERRIDE;

 private:
  // Creates the child controls.
  void Init();

  // Initializes the state of the always-on-top setting as the window is shown.
  void InitAlwaysOnTopState();

  // Activates the tab associated with the focused row.
  void ActivateFocusedTab();

  // Adds an always on top item to the window's system menu.
  void AddAlwaysOnTopSystemMenuItem();

  // Restores saved always on top state from a previous session.
  bool GetSavedAlwaysOnTopState(bool* always_on_top) const;

  views::LabelButton* purge_memory_button_;
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

  // True when the Task Manager should highlight background resources.
  const bool highlight_background_resources_;

  // The host desktop type this task manager belongs to.
  const chrome::HostDesktopType desktop_type_;

  // We need to own the text of the menu, the Windows API does not copy it.
  string16 always_on_top_menu_text_;

  // An open Task manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static TaskManagerView* instance_;

  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerView);
};

// static
TaskManagerView* TaskManagerView::instance_ = NULL;


TaskManagerView::TaskManagerView(bool highlight_background_resources,
                                 chrome::HostDesktopType desktop_type)
    : purge_memory_button_(NULL),
      kill_button_(NULL),
      about_memory_link_(NULL),
      tab_table_(NULL),
      tab_table_parent_(NULL),
      task_manager_(TaskManager::GetInstance()),
      model_(TaskManager::GetInstance()->model()),
      is_always_on_top_(false),
      highlight_background_resources_(highlight_background_resources),
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
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_SHARED_MEM_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_CPU_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_NET_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_PROCESS_ID_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(ui::TableColumn(
      IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN,
      ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(ui::TableColumn(
      IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN,
      ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_FPS_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(
      ui::TableColumn(IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN,
                      ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;

  tab_table_ = new views::TableView(
      table_model_.get(), columns_, views::ICON_AND_TEXT, false, true, true);
  tab_table_->SetGrouper(table_model_.get());
  if (highlight_background_resources_) {
    scoped_ptr<BackgroundPainter> painter(
        new BackgroundPainter(table_model_.get()));
    tab_table_->SetRowBackgroundPainter(
        painter.PassAs<views::TableViewRowBackgroundPainter>());
  }

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
  tab_table_->SetColumnVisibility(
      IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN,
                                  false);

  UpdateStatsCounters();
  tab_table_->SetObserver(this);
  tab_table_->set_context_menu_controller(this);
  set_context_menu_controller(this);
  // If we're running with --purge-memory-button, add a "Purge memory" button.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPurgeMemoryButton)) {
    purge_memory_button_ = new views::LabelButton(this,
        l10n_util::GetStringUTF16(IDS_TASK_MANAGER_PURGE_MEMORY));
    purge_memory_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  }
  kill_button_ = new views::LabelButton(this,
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_KILL));
  kill_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  about_memory_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_ABOUT_MEMORY_LINK));
  about_memory_link_->set_listener(this);

  // Makes sure our state is consistent.
  OnSelectionChanged();
}

void TaskManagerView::UpdateStatsCounters() {
  base::StatsTable* stats = base::StatsTable::current();
  if (stats != NULL) {
    int max = stats->GetMaxCounters();
    // skip the first row (it's header data)
    for (int i = 1; i < max; i++) {
      const char* row = stats->GetRowName(i);
      if (row != NULL && row[0] != '\0' && !tab_table_->HasColumn(i)) {
        // TODO(erikkay): Use l10n to get display names for stats.  Right
        // now we're just displaying the internal counter name.  Perhaps
        // stat names not in the string table would be filtered out.
        ui::TableColumn col;
        col.id = i;
        col.title = ASCIIToUTF16(row);
        col.alignment = ui::TableColumn::RIGHT;
        // TODO(erikkay): Width is hard-coded right now, so many column
        // names are clipped.
        col.width = 90;
        col.sortable = true;
        columns_.push_back(col);
        tab_table_->AddColumn(col);
      }
    }
  }
}

void TaskManagerView::ViewHierarchyChanged(bool is_add,
                                           views::View* parent,
                                           views::View* child) {
  // Since we want the Kill button and the Memory Details link to show up in
  // the same visual row as the close button, which is provided by the
  // framework, we must add the buttons to the non-client view, which is the
  // parent of this view. Similarly, when we're removed from the view
  // hierarchy, we must take care to clean up those items as well.
  if (child == this) {
    if (is_add) {
      parent->AddChildView(about_memory_link_);
      if (purge_memory_button_)
        parent->AddChildView(purge_memory_button_);
      parent->AddChildView(kill_button_);
      tab_table_parent_ = tab_table_->CreateParentIfNecessary();
      AddChildView(tab_table_parent_);
    } else {
      parent->RemoveChildView(kill_button_);
      if (purge_memory_button_)
        parent->RemoveChildView(purge_memory_button_);
      parent->RemoveChildView(about_memory_link_);
    }
  }
}

void TaskManagerView::Layout() {
  bool new_style = views::DialogDelegate::UseNewStyle();
  gfx::Size size = kill_button_->GetPreferredSize();
  gfx::Rect parent_bounds = parent()->GetContentsBounds();
  int x = width() - size.width() - (new_style ? 0 : views::kPanelHorizMargin);
  int y_buttons = new_style ? GetLocalBounds().bottom() - size.height() :
      parent_bounds.bottom() - size.height() - views::kButtonVEdgeMargin;
  kill_button_->SetBounds(x, y_buttons, size.width(), size.height());

  if (purge_memory_button_) {
    size = purge_memory_button_->GetPreferredSize();
    purge_memory_button_->SetBounds(
        kill_button_->x() - size.width() -
            views::kUnrelatedControlHorizontalSpacing,
        y_buttons, size.width(), size.height());
  }

  size = about_memory_link_->GetPreferredSize();
  about_memory_link_->SetBounds(new_style ? 0 : views::kPanelHorizMargin,
      y_buttons + (kill_button_->height() - size.height()) / 2,
      size.width(), size.height());

  gfx::Rect rect = GetLocalBounds();
  if (!new_style)
    rect.Inset(views::kPanelHorizMargin, views::kPanelVertMargin);
  rect.Inset(0, 0, 0,
             kill_button_->height() + views::kUnrelatedControlVerticalSpacing);
  tab_table_parent_->SetBoundsRect(rect);
}

gfx::Size TaskManagerView::GetPreferredSize() {
  return gfx::Size(460, 270);
}

// static
void TaskManagerView::Show(bool highlight_background_resources,
                           Browser* browser) {
#if defined(OS_WIN)
  // In Windows Metro it's not good to open this native window.
  DCHECK(!win8::IsSingleWindowMetroMode());
#endif
  const chrome::HostDesktopType desktop_type = browser->host_desktop_type();

  if (instance_) {
    if (instance_->highlight_background_resources_ !=
        highlight_background_resources ||
        instance_->desktop_type_ != desktop_type) {
      instance_->GetWidget()->Close();
    } else {
      // If there's a Task manager window open already, just activate it.
      instance_->GetWidget()->Activate();
      return;
    }
  }
  instance_ = new TaskManagerView(highlight_background_resources, desktop_type);
  DialogDelegateView::CreateDialogWidget(instance_,
      browser->window()->GetNativeWindow(), NULL);
  instance_->InitAlwaysOnTopState();
  instance_->model_->StartUpdating();
  instance_->GetWidget()->Show();

  // Set the initial focus to the list of tasks.
  views::FocusManager* focus_manager = instance_->GetFocusManager();
  if (focus_manager)
    focus_manager->SetFocusedView(instance_->tab_table_);
}

// ButtonListener implementation.
void TaskManagerView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  if (purge_memory_button_ && (sender == purge_memory_button_)) {
    MemoryPurger::PurgeAll();
  } else {
    typedef ui::ListSelectionModel::SelectedIndices SelectedIndices;
    DCHECK_EQ(kill_button_, sender);
    SelectedIndices selection(tab_table_->selection_model().selected_indices());
    for (SelectedIndices::const_reverse_iterator i = selection.rbegin();
         i != selection.rend(); ++i) {
      task_manager_->KillProcess(*i);
    }
  }
}

// DialogDelegate implementation.
bool TaskManagerView::CanResize() const {
  return true;
}

bool TaskManagerView::CanMaximize() const {
  return true;
}

bool TaskManagerView::ExecuteWindowsCommand(int command_id) {
#if defined(OS_WIN) && !defined(USE_AURA)
  if (command_id == IDC_ALWAYS_ON_TOP) {
    is_always_on_top_ = !is_always_on_top_;

    // Change the menu check state.
    HMENU system_menu = GetSystemMenu(GetWidget()->GetNativeWindow(), FALSE);
    MENUITEMINFO menu_info;
    memset(&menu_info, 0, sizeof(MENUITEMINFO));
    menu_info.cbSize = sizeof(MENUITEMINFO);
    BOOL r = GetMenuItemInfo(system_menu, IDC_ALWAYS_ON_TOP,
                             FALSE, &menu_info);
    DCHECK(r);
    menu_info.fMask = MIIM_STATE;
    if (is_always_on_top_)
      menu_info.fState = MFS_CHECKED;
    r = SetMenuItemInfo(system_menu, IDC_ALWAYS_ON_TOP, FALSE, &menu_info);

    // Now change the actual window's behavior.
    GetWidget()->SetAlwaysOnTop(is_always_on_top_);

    // Save the state.
    if (g_browser_process->local_state()) {
      DictionaryPrefUpdate update(g_browser_process->local_state(),
                                  GetWindowName().c_str());
      DictionaryValue* window_preferences = update.Get();
      window_preferences->SetBoolean("always_on_top", is_always_on_top_);
    }
    return true;
  }
#endif
  return false;
}

string16 TaskManagerView::GetWindowTitle() const {
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
                                             const gfx::Point& point) {
  UpdateStatsCounters();
  ui::SimpleMenuModel menu_model(this);
  for (std::vector<ui::TableColumn>::iterator i(columns_.begin());
       i != columns_.end(); ++i) {
    menu_model.AddCheckItem(i->id, l10n_util::GetStringUTF16(i->id));
  }
  views::MenuModelAdapter menu_adapter(&menu_model);
  menu_runner_.reset(new views::MenuRunner(menu_adapter.CreateMenu()));
  if (menu_runner_->RunMenuAt(GetWidget(), NULL, gfx::Rect(point, gfx::Size()),
                              views::MenuItemView::TOPLEFT,
                              views::MenuRunner::CONTEXT_MENU) ==
      views::MenuRunner::MENU_DELETED)
    return;
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

void TaskManagerView::ExecuteCommand(int id) {
  tab_table_->SetColumnVisibility(id, !tab_table_->IsColumnVisible(id));
}

void TaskManagerView::InitAlwaysOnTopState() {
  is_always_on_top_ = false;
  if (GetSavedAlwaysOnTopState(&is_always_on_top_))
    GetWidget()->SetAlwaysOnTop(is_always_on_top_);
  AddAlwaysOnTopSystemMenuItem();
}

void TaskManagerView::ActivateFocusedTab() {
  const int active_row = tab_table_->selection_model().active();
  if (active_row != -1)
    task_manager_->ActivateProcess(active_row);
}

void TaskManagerView::AddAlwaysOnTopSystemMenuItem() {
#if defined(OS_WIN) && !defined(USE_AURA)
  // The Win32 API requires that we own the text.
  always_on_top_menu_text_ = l10n_util::GetStringUTF16(IDS_ALWAYS_ON_TOP);

  // Let's insert a menu to the window.
  HMENU system_menu = ::GetSystemMenu(GetWidget()->GetNativeWindow(), FALSE);
  int index = ::GetMenuItemCount(system_menu) - 1;
  if (index < 0) {
    // Paranoia check.
    NOTREACHED();
    index = 0;
  }
  // First we add the separator.
  MENUITEMINFO menu_info;
  memset(&menu_info, 0, sizeof(MENUITEMINFO));
  menu_info.cbSize = sizeof(MENUITEMINFO);
  menu_info.fMask = MIIM_FTYPE;
  menu_info.fType = MFT_SEPARATOR;
  ::InsertMenuItem(system_menu, index, TRUE, &menu_info);

  // Then the actual menu.
  menu_info.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_STATE;
  menu_info.fType = MFT_STRING;
  menu_info.fState = MFS_ENABLED;
  if (is_always_on_top_)
    menu_info.fState |= MFS_CHECKED;
  menu_info.wID = IDC_ALWAYS_ON_TOP;
  menu_info.dwTypeData = const_cast<wchar_t*>(always_on_top_menu_text_.c_str());
  ::InsertMenuItem(system_menu, index, TRUE, &menu_info);
#endif
}

bool TaskManagerView::GetSavedAlwaysOnTopState(bool* always_on_top) const {
  if (!g_browser_process->local_state())
    return false;

  const DictionaryValue* dictionary =
      g_browser_process->local_state()->GetDictionary(GetWindowName().c_str());
  return dictionary &&
      dictionary->GetBoolean("always_on_top", always_on_top) && always_on_top;
}

}  // namespace

namespace chrome {

// Declared in browser_dialogs.h so others don't need to depend on our header.
void ShowTaskManager(Browser* browser) {
  TaskManagerView::Show(false, browser);
}

void ShowBackgroundPages(Browser* browser) {
  TaskManagerView::Show(true, browser);
}

}  // namespace chrome
