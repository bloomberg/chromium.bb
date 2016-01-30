// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/new_task_manager_view.h"

#include <stddef.h>

#include "base/prefs/pref_service.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/task_management/task_manager_observer.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/task_manager/task_manager_columns.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_ASH)
#include "ash/shelf/shelf_util.h"
#include "ash/wm/window_util.h"
#include "grit/ash_resources.h"
#endif  // defined(USE_ASH)

#if defined(OS_WIN)
#include "chrome/browser/shell_integration.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif  // defined(OS_WIN)

namespace task_management {

namespace {

NewTaskManagerView* g_task_manager_view = nullptr;

// Opens the "about:memory" for the "stats for nerds" link.
void OpenAboutMemory(chrome::HostDesktopType desktop_type) {
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  if (profile->IsGuestSession() &&
      !g_browser_process->local_state()->GetBoolean(
          prefs::kBrowserGuestModeEnabled)) {
    UserManager::Show(base::FilePath(),
                      profiles::USER_MANAGER_NO_TUTORIAL,
                      profiles::USER_MANAGER_SELECT_PROFILE_CHROME_MEMORY);
    return;
  }

  chrome::NavigateParams params(profile,
                                GURL(chrome::kChromeUIMemoryURL),
                                ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  params.host_desktop_type = desktop_type;
  chrome::Navigate(&params);
}

}  // namespace

NewTaskManagerView::~NewTaskManagerView() {
  // Delete child views now, while our table model still exists.
  RemoveAllChildViews(true);
}

// static
void NewTaskManagerView::Show(Browser* browser) {
  if (g_task_manager_view) {
    // If there's a Task manager window open already, just activate it.
    g_task_manager_view->GetWidget()->Activate();
    return;
  }

  // In ash we can come here through the ChromeShellDelegate. If there is no
  // browser window at that time of the call, browser could be passed as NULL.
  const chrome::HostDesktopType desktop_type =
      browser ? browser->host_desktop_type() : chrome::HOST_DESKTOP_TYPE_ASH;

  g_task_manager_view = new NewTaskManagerView(desktop_type);

  gfx::NativeWindow window = browser ? browser->window()->GetNativeWindow()
                                     : nullptr;
#if defined(USE_ASH)
  if (!window)
    window = ash::wm::GetActiveWindow();
#endif

  DialogDelegate::CreateDialogWidget(g_task_manager_view,
                                     window,
                                     nullptr);
  g_task_manager_view->InitAlwaysOnTopState();

#if defined(OS_WIN)
  // Set the app id for the task manager to the app id of its parent browser. If
  // no parent is specified, the app id will default to that of the initial
  // process.
  if (browser) {
    ui::win::SetAppIdForWindow(
        ShellIntegration::GetChromiumModelIdForProfile(
            browser->profile()->GetPath()),
        views::HWNDForWidget(g_task_manager_view->GetWidget()));
  }
#endif

  g_task_manager_view->GetWidget()->Show();

  // Set the initial focus to the list of tasks.
  views::FocusManager* focus_manager =
      g_task_manager_view->GetFocusManager();
  if (focus_manager)
    focus_manager->SetFocusedView(g_task_manager_view->tab_table_);

#if defined(USE_ASH)
  gfx::NativeWindow native_window =
      g_task_manager_view->GetWidget()->GetNativeWindow();
  ash::SetShelfItemDetailsForDialogWindow(native_window,
                                          IDR_ASH_SHELF_ICON_TASK_MANAGER,
                                          native_window->title());
#endif
}

// static
void NewTaskManagerView::Hide() {
  if (g_task_manager_view)
    g_task_manager_view->GetWidget()->Close();
}

bool NewTaskManagerView::IsColumnVisible(int column_id) const {
  return tab_table_->IsColumnVisible(column_id);
}

void NewTaskManagerView::SetColumnVisibility(int column_id,
                                             bool new_visibility) {
  tab_table_->SetColumnVisibility(column_id, new_visibility);
}

bool NewTaskManagerView::IsTableSorted() const {
  return tab_table_->is_sorted();
}

TableSortDescriptor
NewTaskManagerView::GetSortDescriptor() const {
  if (!IsTableSorted())
    return TableSortDescriptor();

  const auto& descriptor = tab_table_->sort_descriptors().front();
  return TableSortDescriptor(descriptor.column_id, descriptor.ascending);
}

void NewTaskManagerView::ToggleSortOrder(int visible_column_index) {
  tab_table_->ToggleSortOrder(visible_column_index);
}

void NewTaskManagerView::Layout() {
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

gfx::Size NewTaskManagerView::GetPreferredSize() const {
  return gfx::Size(460, 270);
}

bool NewTaskManagerView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK_EQ(ui::VKEY_W, accelerator.key_code());
  DCHECK_EQ(ui::EF_CONTROL_DOWN, accelerator.modifiers());
  GetWidget()->Close();
  return true;
}

void NewTaskManagerView::ViewHierarchyChanged(
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

void NewTaskManagerView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  DCHECK_EQ(kill_button_, sender);

  using SelectedIndices =  ui::ListSelectionModel::SelectedIndices;
  SelectedIndices selection(tab_table_->selection_model().selected_indices());
  for (SelectedIndices::const_reverse_iterator i = selection.rbegin();
       i != selection.rend();
       ++i) {
    table_model_->KillTask(*i);
  }
}

bool NewTaskManagerView::CanResize() const {
  return true;
}

bool NewTaskManagerView::CanMaximize() const {
  return true;
}

bool NewTaskManagerView::CanMinimize() const {
  return true;
}

bool NewTaskManagerView::ExecuteWindowsCommand(int command_id) {
  return false;
}

base::string16 NewTaskManagerView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_TITLE);
}

std::string NewTaskManagerView::GetWindowName() const {
  return prefs::kTaskManagerWindowPlacement;
}

int NewTaskManagerView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void NewTaskManagerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  // (WindowClosing comes in asynchronously from the call to Close() and we
  // may have already opened a new instance).
  if (g_task_manager_view == this) {
    // We don't have to delete |g_task_manager_view| as we don't own it. It's
    // owned by the Views hierarchy.
    g_task_manager_view = nullptr;
  }
  table_model_->StoreColumnsSettings();
}

bool NewTaskManagerView::UseNewStyleForThisDialog() const {
  return false;
}

void NewTaskManagerView::GetGroupRange(int model_index,
                                       views::GroupRange* range) {
  table_model_->GetRowsGroupRange(model_index, &range->start, &range->length);
}

void NewTaskManagerView::OnSelectionChanged() {
  const ui::ListSelectionModel::SelectedIndices& selections(
      tab_table_->selection_model().selected_indices());
  bool selection_contains_browser_process = false;
  for (const auto& selection : selections) {
    if (table_model_->IsBrowserProcess(selection)) {
      selection_contains_browser_process = true;
      break;
    }
  }

  kill_button_->SetEnabled(!selection_contains_browser_process &&
                           !selections.empty());
}

void NewTaskManagerView::OnDoubleClick() {
  ActivateFocusedTab();
}

void NewTaskManagerView::OnKeyDown(ui::KeyboardCode keycode) {
  if (keycode == ui::VKEY_RETURN)
    ActivateFocusedTab();
}

void NewTaskManagerView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK_EQ(about_memory_link_, source);
  OpenAboutMemory(desktop_type_);
}

void NewTaskManagerView::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  ui::SimpleMenuModel menu_model(this);

  for (const auto& table_column : columns_) {
    menu_model.AddCheckItem(table_column.id,
                            l10n_util::GetStringUTF16(table_column.id));
  }

  menu_runner_.reset(
      new views::MenuRunner(&menu_model, views::MenuRunner::CONTEXT_MENU));

  if (menu_runner_->RunMenuAt(GetWidget(),
                              nullptr,
                              gfx::Rect(point, gfx::Size()),
                              views::MENU_ANCHOR_TOPLEFT,
                              source_type) == views::MenuRunner::MENU_DELETED) {
    return;
  }
}

bool NewTaskManagerView::IsCommandIdChecked(int id) const {
  return tab_table_->IsColumnVisible(id);
}

bool NewTaskManagerView::IsCommandIdEnabled(int id) const {
  return true;
}

bool NewTaskManagerView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void NewTaskManagerView::ExecuteCommand(int id, int event_flags) {
  table_model_->ToggleColumnVisibility(id);
}

NewTaskManagerView::NewTaskManagerView(chrome::HostDesktopType desktop_type)
    : kill_button_(nullptr),
      about_memory_link_(nullptr),
      tab_table_(nullptr),
      tab_table_parent_(nullptr),
      desktop_type_(desktop_type),
      is_always_on_top_(false) {
  Init();
}

// static
NewTaskManagerView* NewTaskManagerView::GetInstanceForTests() {
  return g_task_manager_view;
}

void NewTaskManagerView::Init() {
  // Create the table columns.
  for (size_t i = 0; i < kColumnsSize; ++i) {
    const auto& col_data = kColumns[i];
    columns_.push_back(ui::TableColumn(col_data.id, col_data.align,
                                       col_data.width, col_data.percent));
    columns_.back().sortable = col_data.sortable;
    columns_.back().initial_sort_is_ascending =
        col_data.initial_sort_is_ascending;
  }

  // Create the table view.
  tab_table_ = new views::TableView(nullptr, columns_, views::ICON_AND_TEXT,
                                    false);
  table_model_.reset(new TaskManagerTableModel(REFRESH_TYPE_CPU |
                                               REFRESH_TYPE_MEMORY |
                                               REFRESH_TYPE_NETWORK_USAGE,
                                               this));
  tab_table_->SetModel(table_model_.get());
  tab_table_->SetGrouper(this);
  tab_table_->SetObserver(this);
  tab_table_->set_context_menu_controller(this);
  set_context_menu_controller(this);

  table_model_->RetrieveSavedColumnsSettingsAndUpdateTable();

  kill_button_ = new views::LabelButton(this,
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_KILL));
  kill_button_->SetStyle(views::Button::STYLE_BUTTON);

  about_memory_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_ABOUT_MEMORY_LINK));
  about_memory_link_->set_listener(this);

  // Makes sure our state is consistent.
  OnSelectionChanged();

  AddAccelerator(ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN));
}

void NewTaskManagerView::InitAlwaysOnTopState() {
  RetriveSavedAlwaysOnTopState();
  GetWidget()->SetAlwaysOnTop(is_always_on_top_);
}

void NewTaskManagerView::ActivateFocusedTab() {
  const int active_row = tab_table_->selection_model().active();
  if (active_row != ui::ListSelectionModel::kUnselectedIndex)
    table_model_->ActivateTask(active_row);
}

void NewTaskManagerView::RetriveSavedAlwaysOnTopState() {
  is_always_on_top_ = false;

  if (!g_browser_process->local_state())
    return;

  const base::DictionaryValue* dictionary =
    g_browser_process->local_state()->GetDictionary(GetWindowName());
  if (dictionary)
    dictionary->GetBoolean("always_on_top", &is_always_on_top_);
}

}  // namespace task_management
