// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/task_manager_view.h"

#include <stddef.h>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/task_manager/task_manager_columns.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

#if defined(OS_CHROMEOS)
// gn check complains on Linux Ozone.
#include "ash/public/cpp/shelf_item.h"         // nogncheck
#include "ash/public/cpp/window_properties.h"  // nogncheck
#include "ash/resources/grit/ash_resources.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
#include "chrome/browser/shell_integration_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif  // defined(OS_WIN)

namespace task_manager {

namespace {

TaskManagerView* g_task_manager_view = nullptr;

}  // namespace

TaskManagerView::~TaskManagerView() {
  // Delete child views now, while our table model still exists.
  RemoveAllChildViews(true);
}

// static
task_manager::TaskManagerTableModel* TaskManagerView::Show(Browser* browser) {
  if (g_task_manager_view) {
    // If there's a Task manager window open already, just activate it.
    g_task_manager_view->SelectTaskOfActiveTab(browser);
    g_task_manager_view->GetWidget()->Activate();
    return g_task_manager_view->table_model_.get();
  }

  g_task_manager_view = new TaskManagerView();

  gfx::NativeWindow context =
      browser ? browser->window()->GetNativeWindow() : nullptr;
#if defined(OS_CHROMEOS)
  if (!ash_util::IsRunningInMash() && !context)
    context = ash::wm::GetActiveWindow();
#endif

  DialogDelegate::CreateDialogWidget(g_task_manager_view, context, nullptr);
  g_task_manager_view->InitAlwaysOnTopState();

#if defined(OS_WIN)
  // Set the app id for the task manager to the app id of its parent browser. If
  // no parent is specified, the app id will default to that of the initial
  // process.
  if (browser) {
    ui::win::SetAppIdForWindow(
        shell_integration::win::GetChromiumModelIdForProfile(
            browser->profile()->GetPath()),
        views::HWNDForWidget(g_task_manager_view->GetWidget()));
  }
#endif

  g_task_manager_view->SelectTaskOfActiveTab(browser);
  g_task_manager_view->GetWidget()->Show();

#if defined(OS_CHROMEOS)
  aura::Window* window = g_task_manager_view->GetWidget()->GetNativeWindow();
  // An app id for task manager windows, also used to identify the shelf item.
  // Generated as crx_file::id_util::GenerateId("org.chromium.taskmanager")
  static constexpr char kTaskManagerId[] = "ijaigheoohcacdnplfbdimmcfldnnhdi";
  const ash::ShelfID shelf_id(kTaskManagerId);
  window->SetProperty(ash::kShelfIDKey, new std::string(shelf_id.Serialize()));
  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_DIALOG);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* icon = rb.GetImageSkiaNamed(IDR_ASH_SHELF_ICON_TASK_MANAGER);
  // The new gfx::ImageSkia instance is owned by the window itself.
  window->SetProperty(aura::client::kWindowIconKey, new gfx::ImageSkia(*icon));
#endif
  return g_task_manager_view->table_model_.get();
}

// static
void TaskManagerView::Hide() {
  if (g_task_manager_view)
    g_task_manager_view->GetWidget()->Close();
}

bool TaskManagerView::IsColumnVisible(int column_id) const {
  return tab_table_->IsColumnVisible(column_id);
}

void TaskManagerView::SetColumnVisibility(int column_id, bool new_visibility) {
  tab_table_->SetColumnVisibility(column_id, new_visibility);
}

bool TaskManagerView::IsTableSorted() const {
  return tab_table_->is_sorted();
}

TableSortDescriptor TaskManagerView::GetSortDescriptor() const {
  if (!IsTableSorted())
    return TableSortDescriptor();

  const auto& descriptor = tab_table_->sort_descriptors().front();
  return TableSortDescriptor(descriptor.column_id, descriptor.ascending);
}

void TaskManagerView::SetSortDescriptor(const TableSortDescriptor& descriptor) {
  views::TableView::SortDescriptors descriptor_list;

  // If |sorted_column_id| is the default value, it means to clear the sort.
  if (descriptor.sorted_column_id != TableSortDescriptor().sorted_column_id) {
    descriptor_list.emplace_back(descriptor.sorted_column_id,
                                 descriptor.is_ascending);
  }

  tab_table_->SetSortDescriptors(descriptor_list);
}

gfx::Size TaskManagerView::CalculatePreferredSize() const {
  return gfx::Size(460, 270);
}

bool TaskManagerView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  DCHECK_EQ(ui::VKEY_W, accelerator.key_code());
  DCHECK_EQ(ui::EF_CONTROL_DOWN, accelerator.modifiers());
  GetWidget()->Close();
  return true;
}

views::View* TaskManagerView::GetInitiallyFocusedView() {
  return nullptr;
}

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

bool TaskManagerView::Accept() {
  using SelectedIndices = ui::ListSelectionModel::SelectedIndices;
  SelectedIndices selection(tab_table_->selection_model().selected_indices());
  for (SelectedIndices::const_reverse_iterator i = selection.rbegin();
       i != selection.rend(); ++i) {
    table_model_->KillTask(*i);
  }

  // Just kill the process, don't close the task manager dialog.
  return false;
}

bool TaskManagerView::Close() {
  return true;
}

int TaskManagerView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 TaskManagerView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_KILL);
}

bool TaskManagerView::IsDialogButtonEnabled(ui::DialogButton button) const {
  const ui::ListSelectionModel::SelectedIndices& selections(
      tab_table_->selection_model().selected_indices());
  for (const auto& selection : selections) {
    if (!table_model_->IsTaskKillable(selection))
      return false;
  }

  return !selections.empty() && TaskManagerInterface::IsEndProcessEnabled();
}

void TaskManagerView::WindowClosing() {
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

bool TaskManagerView::ShouldUseCustomFrame() const {
  return false;
}

void TaskManagerView::GetGroupRange(int model_index, views::GroupRange* range) {
  table_model_->GetRowsGroupRange(model_index, &range->start, &range->length);
}

void TaskManagerView::OnSelectionChanged() {
  DialogModelChanged();
}

void TaskManagerView::OnDoubleClick() {
  ActivateSelectedTab();
}

void TaskManagerView::OnKeyDown(ui::KeyboardCode keycode) {
  if (keycode == ui::VKEY_RETURN)
    ActivateSelectedTab();
}

void TaskManagerView::ShowContextMenuForView(views::View* source,
                                             const gfx::Point& point,
                                             ui::MenuSourceType source_type) {
  menu_model_.reset(new ui::SimpleMenuModel(this));

  for (const auto& table_column : columns_) {
    menu_model_->AddCheckItem(table_column.id,
                              l10n_util::GetStringUTF16(table_column.id));
  }

  menu_runner_.reset(new views::MenuRunner(menu_model_.get(),
                                           views::MenuRunner::CONTEXT_MENU));

  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MENU_ANCHOR_TOPLEFT, source_type);
}

bool TaskManagerView::IsCommandIdChecked(int id) const {
  return tab_table_->IsColumnVisible(id);
}

bool TaskManagerView::IsCommandIdEnabled(int id) const {
  return true;
}

void TaskManagerView::ExecuteCommand(int id, int event_flags) {
  table_model_->ToggleColumnVisibility(id);
}

void TaskManagerView::MenuClosed(ui::SimpleMenuModel* source) {
  menu_model_.reset();
  menu_runner_.reset();
}

TaskManagerView::TaskManagerView()
    : tab_table_(nullptr),
      tab_table_parent_(nullptr),
      is_always_on_top_(false) {
  Init();
  chrome::RecordDialogCreation(chrome::DialogIdentifier::TASK_MANAGER);
}

// static
TaskManagerView* TaskManagerView::GetInstanceForTests() {
  return g_task_manager_view;
}

void TaskManagerView::Init() {
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
  tab_table_ =
      new views::TableView(nullptr, columns_, views::ICON_AND_TEXT, false);
  table_model_.reset(new TaskManagerTableModel(this));
  tab_table_->SetModel(table_model_.get());
  tab_table_->SetGrouper(this);
  tab_table_->set_observer(this);
  tab_table_->set_context_menu_controller(this);
  set_context_menu_controller(this);

  tab_table_parent_ = tab_table_->CreateParentIfNecessary();
  AddChildView(tab_table_parent_);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const gfx::Insets dialog_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);
  // We don't use ChromeLayoutProvider::GetDialogInsetsForContentType because we
  // don't have a title.
  const gfx::Insets content_insets(
      dialog_insets.top(), dialog_insets.left(),
      provider->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
      dialog_insets.right());
  SetBorder(views::CreateEmptyBorder(content_insets));

  table_model_->RetrieveSavedColumnsSettingsAndUpdateTable();

  AddAccelerator(ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN));
}

void TaskManagerView::InitAlwaysOnTopState() {
  RetrieveSavedAlwaysOnTopState();
  GetWidget()->SetAlwaysOnTop(is_always_on_top_);
}

void TaskManagerView::ActivateSelectedTab() {
  const int active_row = tab_table_->selection_model().active();
  if (active_row != ui::ListSelectionModel::kUnselectedIndex)
    table_model_->ActivateTask(active_row);
}

void TaskManagerView::SelectTaskOfActiveTab(Browser* browser) {
  if (browser) {
    tab_table_->Select(table_model_->GetRowForWebContents(
        browser->tab_strip_model()->GetActiveWebContents()));
  }
}

void TaskManagerView::RetrieveSavedAlwaysOnTopState() {
  is_always_on_top_ = false;

  if (!g_browser_process->local_state())
    return;

  const base::DictionaryValue* dictionary =
      g_browser_process->local_state()->GetDictionary(GetWindowName());
  if (dictionary)
    dictionary->GetBoolean("always_on_top", &is_always_on_top_);
}

}  // namespace task_manager

namespace chrome {

#if defined(OS_MACOSX)
// These are used by the Mac versions of |ShowTaskManager| and |HideTaskManager|
// if they decide to show the Views task manager instead of the Cocoa one.
task_manager::TaskManagerTableModel* ShowTaskManagerViews(Browser* browser) {
  return task_manager::TaskManagerView::Show(browser);
}

void HideTaskManagerViews() {
  task_manager::TaskManagerView::Hide();
}
#endif

}  // namespace chrome
