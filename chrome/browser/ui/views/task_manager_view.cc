// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/metrics/stats_table.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/memory_purger.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"
#include "views/accelerator.h"
#include "views/background.h"
#include "views/controls/button/native_button.h"
#include "views/controls/link.h"
#include "views/controls/menu/menu.h"
#include "views/controls/table/group_table_view.h"
#include "views/controls/table/table_view_observer.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

// The task manager window default size.
static const int kDefaultWidth = 460;
static const int kDefaultHeight = 270;

// Yellow highlight used when highlighting background resources.
static const SkColor kBackgroundResourceHighlight =
    SkColorSetRGB(0xff,0xf1,0xcd);

namespace {

////////////////////////////////////////////////////////////////////////////////
// TaskManagerTableModel class
////////////////////////////////////////////////////////////////////////////////

class TaskManagerTableModel : public views::GroupTableModel,
                              public TaskManagerModelObserver {
 public:
  explicit TaskManagerTableModel(TaskManagerModel* model)
      : model_(model),
        observer_(NULL) {
    model_->AddObserver(this);
  }

  ~TaskManagerTableModel() {
    model_->RemoveObserver(this);
  }

  // GroupTableModel.
  int RowCount() OVERRIDE;
  string16 GetText(int row, int column) OVERRIDE;
  SkBitmap GetIcon(int row) OVERRIDE;
  void GetGroupRangeForItem(int item, views::GroupRange* range) OVERRIDE;
  void SetObserver(ui::TableModelObserver* observer) OVERRIDE;
  virtual int CompareValues(int row1, int row2, int column_id) OVERRIDE;

  // TaskManagerModelObserver.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

  // Returns true if resource corresponding to |row| is a background resource.
  bool IsBackgroundResource(int row);

 private:
  TaskManagerModel* model_;
  ui::TableModelObserver* observer_;
};

int TaskManagerTableModel::RowCount() {
  return model_->ResourceCount();
}

string16 TaskManagerTableModel::GetText(int row, int col_id) {
  switch (col_id) {
    case IDS_TASK_MANAGER_PAGE_COLUMN:  // Process
      return model_->GetResourceTitle(row);

    case IDS_TASK_MANAGER_NET_COLUMN:  // Net
      return model_->GetResourceNetworkUsage(row);

    case IDS_TASK_MANAGER_CPU_COLUMN:  // CPU
      if (!model_->IsResourceFirstInGroup(row))
        return string16();
      return model_->GetResourceCPUUsage(row);

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return string16();
      return model_->GetResourcePrivateMemory(row);

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return string16();
      return model_->GetResourceSharedMemory(row);

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return string16();
      return model_->GetResourcePhysicalMemory(row);

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return string16();
      return model_->GetResourceProcessId(row);

    case IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN:  // Goats Teleported!
      return model_->GetResourceGoatsTeleported(row);

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return string16();
      return model_->GetResourceWebCoreImageCacheSize(row);

    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return string16();
      return model_->GetResourceWebCoreScriptsCacheSize(row);

    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return string16();
      return model_->GetResourceWebCoreCSSCacheSize(row);

    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return string16();
      return model_->GetResourceSqliteMemoryUsed(row);

    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return string16();
      return model_->GetResourceV8MemoryAllocatedSize(row);

    default:
      NOTREACHED();
      return string16();
  }
}

SkBitmap TaskManagerTableModel::GetIcon(int row) {
  return model_->GetResourceIcon(row);
}


void TaskManagerTableModel::GetGroupRangeForItem(int item,
                                                 views::GroupRange* range) {
  std::pair<int, int> range_pair = model_->GetGroupRangeForResource(item);
  range->start = range_pair.first;
  range->length = range_pair.second;
}

void TaskManagerTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

int TaskManagerTableModel::CompareValues(int row1, int row2, int column_id) {
  return model_->CompareValues(row1, row2, column_id);
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

// Thin wrapper around GroupTableView to enable setting the background
// resource highlight color.
class BackgroundColorGroupTableView : public views::GroupTableView {
 public:
  BackgroundColorGroupTableView(TaskManagerTableModel* model,
                                std::vector<ui::TableColumn> columns,
                                bool highlight_background_resources)
      : views::GroupTableView(model, columns, views::ICON_AND_TEXT,
                              false, true, true, true),
        model_(model) {
    SetCustomColorsEnabled(highlight_background_resources);
  }

  virtual ~BackgroundColorGroupTableView() {}

 private:
  virtual bool GetCellColors(int model_row,
                             int column,
                             ItemColor* foreground,
                             ItemColor* background,
                             LOGFONT* logfont) {
    if (!model_->IsBackgroundResource(model_row))
      return false;

    // Render background resources with a yellow highlight.
    background->color_is_set = true;
    background->color = kBackgroundResourceHighlight;
    foreground->color_is_set = false;
    return true;
  }

  TaskManagerTableModel* model_;
};

// The Task manager UI container.
class TaskManagerView : public views::View,
                        public views::ButtonListener,
                        public views::DialogDelegate,
                        public views::TableViewObserver,
                        public views::LinkController,
                        public views::ContextMenuController,
                        public views::Menu::Delegate {
 public:
  explicit TaskManagerView(bool highlight_background_resources);
  virtual ~TaskManagerView();

  // Shows the Task manager window, or re-activates an existing one. If
  // |highlight_background_resources| is true, highlights the background
  // resources in the resource display.
  static void Show(bool highlight_background_resources);

  // views::View
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

  // ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::DialogDelegate
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool ExecuteWindowsCommand(int command_id);
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetWindowName() const;
  virtual int GetDialogButtons() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();

  // views::TableViewObserver implementation.
  virtual void OnSelectionChanged();
  virtual void OnDoubleClick();
  virtual void OnKeyDown(ui::KeyboardCode keycode);

  // views::LinkController implementation.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Called by the column picker to pick up any new stat counters that
  // may have appeared since last time.
  void UpdateStatsCounters();

  // Menu::Delegate
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& p,
                                      bool is_mouse_gesture);
  virtual bool IsItemChecked(int id) const;
  virtual void ExecuteCommand(int id);

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

  views::NativeButton* purge_memory_button_;
  views::NativeButton* kill_button_;
  views::Link* about_memory_link_;
  views::GroupTableView* tab_table_;

  TaskManager* task_manager_;

  TaskManagerModel* model_;

  // all possible columns, not necessarily visible
  std::vector<ui::TableColumn> columns_;

  scoped_ptr<TaskManagerTableModel> table_model_;

  // True when the Task Manager window should be shown on top of other windows.
  bool is_always_on_top_;

  // True when the Task Manager should highlight background resources.
  bool highlight_background_resources_;

  // We need to own the text of the menu, the Windows API does not copy it.
  std::wstring always_on_top_menu_text_;

  // An open Task manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static TaskManagerView* instance_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerView);
};

// static
TaskManagerView* TaskManagerView::instance_ = NULL;


TaskManagerView::TaskManagerView(bool highlight_background_resources)
    : purge_memory_button_(NULL),
      task_manager_(TaskManager::GetInstance()),
      model_(TaskManager::GetInstance()->model()),
      is_always_on_top_(false),
      highlight_background_resources_(highlight_background_resources) {
  Init();
}

TaskManagerView::~TaskManagerView() {
  // Delete child views now, while our table model still exists.
  RemoveAllChildViews(true);
}

void TaskManagerView::Init() {
  table_model_.reset(new TaskManagerTableModel(model_));

  // Page column has no header label.
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_PAGE_COLUMN,
                                     ui::TableColumn::LEFT, -1, 1));
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
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(
      ui::TableColumn(IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN,
                      ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;

  tab_table_ = new BackgroundColorGroupTableView(
      table_model_.get(), columns_, highlight_background_resources_);

  // Hide some columns by default
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PROCESS_ID_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_SHARED_MEM_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(
      IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN,
                                  false);

  UpdateStatsCounters();
  tab_table_->SetObserver(this);
  tab_table_->SetContextMenuController(this);
  SetContextMenuController(this);
  // If we're running with --purge-memory-button, add a "Purge memory" button.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPurgeMemoryButton)) {
    purge_memory_button_ = new views::NativeButton(this,
        UTF16ToWide(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_PURGE_MEMORY)));
  }
  kill_button_ = new views::NativeButton(
      this, UTF16ToWide(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_KILL)));
  kill_button_->AddAccelerator(views::Accelerator(ui::VKEY_E,
                                                  false, false, false));
  kill_button_->SetAccessibleKeyboardShortcut(L"E");
  about_memory_link_ = new views::Link(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_ABOUT_MEMORY_LINK)));
  about_memory_link_->SetController(this);

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
        // TODO(erikkay): Width is hard-coded right now, so many column
        // names are clipped.
        ui::TableColumn col(i, ASCIIToUTF16(row), ui::TableColumn::RIGHT, 90,
                            0);
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
      AddChildView(tab_table_);
    } else {
      parent->RemoveChildView(kill_button_);
      if (purge_memory_button_)
        parent->RemoveChildView(purge_memory_button_);
      parent->RemoveChildView(about_memory_link_);
    }
  }
}

void TaskManagerView::Layout() {
  // views::kPanelHorizMargin is too big.
  const int kTableButtonSpacing = 12;

  gfx::Size size = kill_button_->GetPreferredSize();
  int prefered_width = size.width();
  int prefered_height = size.height();

  tab_table_->SetBounds(
      x() + views::kPanelHorizMargin,
      y() + views::kPanelVertMargin,
      width() - 2 * views::kPanelHorizMargin,
      height() - 2 * views::kPanelVertMargin - prefered_height);

  // y-coordinate of button top left.
  gfx::Rect parent_bounds = parent()->GetContentsBounds();
  int y_buttons =
      parent_bounds.bottom() - prefered_height - views::kButtonVEdgeMargin;

  kill_button_->SetBounds(
      x() + width() - prefered_width - views::kPanelHorizMargin,
      y_buttons,
      prefered_width,
      prefered_height);

  if (purge_memory_button_) {
    size = purge_memory_button_->GetPreferredSize();
    purge_memory_button_->SetBounds(
        kill_button_->x() - size.width() -
            views::kUnrelatedControlHorizontalSpacing,
        y_buttons, size.width(), size.height());
  }

  size = about_memory_link_->GetPreferredSize();
  int link_prefered_width = size.width();
  int link_prefered_height = size.height();
  // center between the two buttons horizontally, and line up with
  // bottom of buttons vertically.
  int link_y_offset = std::max(0, prefered_height - link_prefered_height) / 2;
  about_memory_link_->SetBounds(
      x() + views::kPanelHorizMargin,
      y_buttons + prefered_height - link_prefered_height - link_y_offset,
      link_prefered_width,
      link_prefered_height);
}

gfx::Size TaskManagerView::GetPreferredSize() {
  return gfx::Size(kDefaultWidth, kDefaultHeight);
}

// static
void TaskManagerView::Show(bool highlight_background_resources) {
  if (instance_) {
    if (instance_->highlight_background_resources_ !=
        highlight_background_resources) {
      instance_->window()->CloseWindow();
    } else {
      // If there's a Task manager window open already, just activate it.
      instance_->window()->Activate();
      return;
    }
  }
  instance_ = new TaskManagerView(highlight_background_resources);
  views::Window::CreateChromeWindow(NULL, gfx::Rect(), instance_);
  instance_->InitAlwaysOnTopState();
  instance_->model_->StartUpdating();
  instance_->window()->Show();

  // Set the initial focus to the list of tasks.
  views::FocusManager* focus_manager = instance_->GetFocusManager();
  if (focus_manager)
    focus_manager->SetFocusedView(instance_->tab_table_);
}

// ButtonListener implementation.
void TaskManagerView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (purge_memory_button_ && (sender == purge_memory_button_)) {
    MemoryPurger::PurgeAll();
  } else {
    DCHECK_EQ(sender, kill_button_);
    for (views::TableSelectionIterator iter  = tab_table_->SelectionBegin();
         iter != tab_table_->SelectionEnd(); ++iter)
      task_manager_->KillProcess(*iter);
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
  if (command_id == IDC_ALWAYS_ON_TOP) {
    is_always_on_top_ = !is_always_on_top_;

    // Change the menu check state.
    HMENU system_menu = GetSystemMenu(GetWindow()->GetNativeWindow(), FALSE);
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
    window()->SetIsAlwaysOnTop(is_always_on_top_);

    // Save the state.
    if (g_browser_process->local_state()) {
      DictionaryValue* window_preferences =
          g_browser_process->local_state()->GetMutableDictionary(
              WideToUTF8(GetWindowName()).c_str());
      window_preferences->SetBoolean("always_on_top", is_always_on_top_);
    }
    return true;
  }
  return false;
}

std::wstring TaskManagerView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_TITLE));
}

std::wstring TaskManagerView::GetWindowName() const {
  return UTF8ToWide(prefs::kTaskManagerWindowPlacement);
}

int TaskManagerView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_NONE;
}

void TaskManagerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  instance_ = NULL;
  task_manager_->OnWindowClosed();
}

views::View* TaskManagerView::GetContentsView() {
  return this;
}

// views::TableViewObserver implementation.
void TaskManagerView::OnSelectionChanged() {
  bool selection_contains_browser_process = false;
  for (views::TableSelectionIterator iter  = tab_table_->SelectionBegin();
       iter != tab_table_->SelectionEnd(); ++iter) {
    if (task_manager_->IsBrowserProcess(*iter)) {
      selection_contains_browser_process = true;
      break;
    }
  }
  kill_button_->SetEnabled(!selection_contains_browser_process &&
                           tab_table_->SelectedRowCount() > 0);
}

void TaskManagerView::OnDoubleClick() {
  ActivateFocusedTab();
}

void TaskManagerView::OnKeyDown(ui::KeyboardCode keycode) {
  if (keycode == ui::VKEY_RETURN)
    ActivateFocusedTab();
}

// views::LinkController implementation
void TaskManagerView::LinkActivated(views::Link* source, int event_flags) {
  DCHECK(source == about_memory_link_);
  task_manager_->OpenAboutMemory();
}

void TaskManagerView::ShowContextMenuForView(views::View* source,
                                             const gfx::Point& p,
                                             bool is_mouse_gesture) {
  UpdateStatsCounters();
  scoped_ptr<views::Menu> menu(views::Menu::Create(
      this, views::Menu::TOPLEFT, source->GetWidget()->GetNativeView()));
  for (std::vector<ui::TableColumn>::iterator i =
       columns_.begin(); i != columns_.end(); ++i) {
    menu->AppendMenuItem(i->id, l10n_util::GetStringUTF16(i->id),
        views::Menu::CHECKBOX);
  }
  menu->RunMenuAt(p.x(), p.y());
}

bool TaskManagerView::IsItemChecked(int id) const {
  return tab_table_->IsColumnVisible(id);
}

void TaskManagerView::ExecuteCommand(int id) {
  tab_table_->SetColumnVisibility(id, !tab_table_->IsColumnVisible(id));
}

void TaskManagerView::InitAlwaysOnTopState() {
  is_always_on_top_ = false;
  if (GetSavedAlwaysOnTopState(&is_always_on_top_))
    window()->SetIsAlwaysOnTop(is_always_on_top_);
  AddAlwaysOnTopSystemMenuItem();
}

void TaskManagerView::ActivateFocusedTab() {
  int row_count = tab_table_->RowCount();
  for (int i = 0; i < row_count; ++i) {
    if (tab_table_->ItemHasTheFocus(i)) {
      task_manager_->ActivateProcess(i);
      break;
    }
  }
}

void TaskManagerView::AddAlwaysOnTopSystemMenuItem() {
  // The Win32 API requires that we own the text.
  always_on_top_menu_text_ =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ALWAYS_ON_TOP));

  // Let's insert a menu to the window.
  HMENU system_menu = ::GetSystemMenu(GetWindow()->GetNativeWindow(), FALSE);
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
}

bool TaskManagerView::GetSavedAlwaysOnTopState(bool* always_on_top) const {
  if (!g_browser_process->local_state())
    return false;

  const DictionaryValue* dictionary =
      g_browser_process->local_state()->GetDictionary(
          WideToUTF8(GetWindowName()).c_str());
  return dictionary &&
      dictionary->GetBoolean("always_on_top", always_on_top) && always_on_top;
}

}  // namespace

namespace browser {

// Declared in browser_dialogs.h so others don't need to depend on our header.
void ShowTaskManager() {
  TaskManagerView::Show(false);
}

void ShowBackgroundPages() {
  TaskManagerView::Show(true);
}

}  // namespace browser
