// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"

#include <unordered_map>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_presenter_delegate_factory.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/presenter/app_list_presenter_delegate_factory.h"
#include "ash/app_list/presenter/app_list_view_delegate_factory.h"
#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "extensions/common/constants.h"
#include "ui/app_list/answer_card_contents_registry.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/display/screen.h"

namespace {

int64_t GetDisplayIdToShowAppListOn() {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(ash::Shell::GetRootWindowForNewWindows())
      .id();
}

class ViewDelegateFactoryImpl : public app_list::AppListViewDelegateFactory {
 public:
  explicit ViewDelegateFactoryImpl(app_list::AppListViewDelegate* delegate)
      : delegate_(delegate) {}
  ~ViewDelegateFactoryImpl() override {}

  // app_list::AppListViewDelegateFactory:
  app_list::AppListViewDelegate* GetDelegate() override { return delegate_; }

 private:
  app_list::AppListViewDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ViewDelegateFactoryImpl);
};

}  // namespace

namespace ash {

// TODO(hejq): Get rid of AppListPresenterDelegateFactory and pass in
// ash::AppListPresenterDelegate directly.
AppListControllerImpl::AppListControllerImpl()
    : view_delegate_(this),
      presenter_(
          std::make_unique<AppListPresenterDelegateFactory>(
              std::make_unique<ViewDelegateFactoryImpl>(&view_delegate_)),
          this),
      keyboard_observer_(this) {
  model_.AddObserver(this);

  // Create only for non-mash. Mash uses window tree embed API to get a
  // token to map answer card contents.
  if (Shell::GetAshConfig() != Config::MASH) {
    answer_card_contents_registry_ =
        std::make_unique<app_list::AnswerCardContentsRegistry>();
  }

  ash::Shell::Get()->AddShellObserver(this);
}

AppListControllerImpl::~AppListControllerImpl() {
  ash::Shell::Get()->RemoveShellObserver(this);
  model_.RemoveObserver(this);
}

void AppListControllerImpl::SetClient(mojom::AppListClientPtr client_ptr) {
  client_ = std::move(client_ptr);
}

void AppListControllerImpl::BindRequest(
    mojom::AppListControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AppListControllerImpl::AddItem(AppListItemMetadataPtr item_data) {
  const std::string folder_id = item_data->folder_id;
  if (folder_id.empty()) {
    model_.AddItem(CreateAppListItem(std::move(item_data)));
  } else {
    // When we're setting a whole model of a profile, each item may have its
    // folder id set properly. However, |AppListModel::AddItemToFolder| requires
    // the item to add is not in the target folder yet, and sets its folder id
    // later. So we should clear the folder id here to avoid breaking checks.
    item_data->folder_id.clear();
    AddItemToFolder(std::move(item_data), folder_id);
  }
}

void AppListControllerImpl::AddItemToFolder(AppListItemMetadataPtr item_data,
                                            const std::string& folder_id) {
  model_.AddItemToFolder(CreateAppListItem(std::move(item_data)), folder_id);
}

void AppListControllerImpl::RemoveItem(const std::string& id) {
  model_.DeleteItem(id);
}

void AppListControllerImpl::RemoveUninstalledItem(const std::string& id) {
  model_.DeleteUninstalledItem(id);
}

void AppListControllerImpl::MoveItemToFolder(const std::string& id,
                                             const std::string& folder_id) {
  app_list::AppListItem* item = model_.FindItem(id);
  model_.MoveItemToFolder(item, folder_id);
}

void AppListControllerImpl::SetStatus(ash::AppListModelStatus status) {
  model_.SetStatus(status);
}

void AppListControllerImpl::SetState(ash::AppListState state) {
  model_.SetState(state);
}

void AppListControllerImpl::HighlightItemInstalledFromUI(
    const std::string& id) {
  model_.top_level_item_list()->HighlightItemInstalledFromUI(id);
}

void AppListControllerImpl::SetSearchEngineIsGoogle(bool is_google) {
  search_model_.SetSearchEngineIsGoogle(is_google);
}

void AppListControllerImpl::SetSearchTabletAndClamshellAccessibleName(
    const base::string16& tablet_accessible_name,
    const base::string16& clamshell_accessible_name) {
  search_model_.search_box()->SetTabletAndClamshellAccessibleName(
      tablet_accessible_name, clamshell_accessible_name);
}

void AppListControllerImpl::SetSearchHintText(const base::string16& hint_text) {
  search_model_.search_box()->SetHintText(hint_text);
}

void AppListControllerImpl::UpdateSearchBox(const base::string16& text,
                                            bool initiated_by_user) {
  search_model_.search_box()->Update(text, initiated_by_user);
}

void AppListControllerImpl::SetItemMetadata(const std::string& id,
                                            AppListItemMetadataPtr data) {
  app_list::AppListItem* item = model_.FindItem(id);
  if (item)
    item->SetMetadata(std::move(data));
}

void AppListControllerImpl::SetItemIcon(const std::string& id,
                                        const gfx::ImageSkia& icon) {
  app_list::AppListItem* item = model_.FindItem(id);
  if (item)
    item->SetIcon(icon);
}

void AppListControllerImpl::SetItemIsInstalling(const std::string& id,
                                                bool is_installing) {
  app_list::AppListItem* item = model_.FindItem(id);
  if (item)
    item->SetIsInstalling(is_installing);
}

void AppListControllerImpl::SetItemPercentDownloaded(
    const std::string& id,
    int32_t percent_downloaded) {
  app_list::AppListItem* item = model_.FindItem(id);
  if (item)
    item->SetPercentDownloaded(percent_downloaded);
}

void AppListControllerImpl::SetModelData(
    std::vector<AppListItemMetadataPtr> apps,
    bool is_search_engine_google) {
  // Clear old model data.
  model_.DeleteAllItems();
  search_model_.DeleteAllResults();

  // Populate new models. First populate folders and then other items to avoid
  // automatically creating folder items in |AddItemToFolder|.
  for (auto& app : apps) {
    if (!app->is_folder)
      continue;
    DCHECK(app->folder_id.empty());
    AddItem(std::move(app));
  }
  for (auto& app : apps) {
    if (!app)
      continue;
    AddItem(std::move(app));
  }
  search_model_.SetSearchEngineIsGoogle(is_search_engine_google);
}

void AppListControllerImpl::GetIdToAppListIndexMap(
    GetIdToAppListIndexMapCallback callback) {
  std::unordered_map<std::string, uint16_t> id_to_app_list_index;
  for (size_t i = 0; i < model_.top_level_item_list()->item_count(); ++i)
    id_to_app_list_index[model_.top_level_item_list()->item_at(i)->id()] = i;
  std::move(callback).Run(id_to_app_list_index);
}

void AppListControllerImpl::FindOrCreateOemFolder(
    const std::string& oem_folder_id,
    const std::string& oem_folder_name,
    const syncer::StringOrdinal& preferred_oem_position,
    FindOrCreateOemFolderCallback callback) {
  app_list::AppListFolderItem* oem_folder =
      model_.FindFolderItem(oem_folder_id);
  if (!oem_folder) {
    std::unique_ptr<app_list::AppListFolderItem> new_folder(
        new app_list::AppListFolderItem(
            oem_folder_id, app_list::AppListFolderItem::FOLDER_TYPE_OEM));
    syncer::StringOrdinal oem_position = preferred_oem_position.IsValid()
                                             ? preferred_oem_position
                                             : GetOemFolderPos();
    // Do not create a sync item for the OEM folder here, do it in
    // ResolveFolderPositions() when the item position is finalized.
    oem_folder = static_cast<app_list::AppListFolderItem*>(
        model_.AddItem(std::move(new_folder)));
    model_.SetItemPosition(oem_folder, oem_position);
  }
  model_.SetItemName(oem_folder, oem_folder_name);
  std::move(callback).Run(oem_folder->CloneMetadata());
}

void AppListControllerImpl::ResolveOemFolderPosition(
    const std::string& oem_folder_id,
    const syncer::StringOrdinal& preferred_oem_position,
    ResolveOemFolderPositionCallback callback) {
  // In ash:
  app_list::AppListFolderItem* ash_oem_folder = FindFolderItem(oem_folder_id);
  ash::mojom::AppListItemMetadataPtr metadata = nullptr;
  if (ash_oem_folder) {
    const syncer::StringOrdinal& oem_folder_pos =
        preferred_oem_position.IsValid() ? preferred_oem_position
                                         : GetOemFolderPos();
    model_.SetItemPosition(ash_oem_folder, oem_folder_pos);
    metadata = ash_oem_folder->CloneMetadata();
  }
  std::move(callback).Run(std::move(metadata));
}

void AppListControllerImpl::DismissAppList() {
  presenter_.Dismiss(base::TimeTicks());
}

void AppListControllerImpl::GetAppInfoDialogBounds(
    GetAppInfoDialogBoundsCallback callback) {
  app_list::AppListView* app_list_view = presenter_.GetView();
  gfx::Rect bounds = gfx::Rect();
  if (app_list_view)
    bounds = app_list_view->GetAppInfoDialogBounds();
  std::move(callback).Run(bounds);
}

void AppListControllerImpl::ShowAppListAndSwitchToState(
    ash::AppListState state) {
  bool app_list_was_open = true;
  app_list::AppListView* app_list_view = presenter_.GetView();
  if (!app_list_view) {
    // TODO(calamity): This may cause the app list to show briefly before the
    // state change. If this becomes an issue, add the ability to ash::Shell to
    // load the app list without showing it.
    presenter_.Show(GetDisplayIdToShowAppListOn(), base::TimeTicks());
    app_list_was_open = false;
    app_list_view = presenter_.GetView();
    DCHECK(app_list_view);
  }

  if (state == ash::AppListState::kInvalidState)
    return;

  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();
  contents_view->SetActiveState(state, app_list_was_open /* animate */);
}

void AppListControllerImpl::ShowAppList() {
  presenter_.Show(GetDisplayIdToShowAppListOn(), base::TimeTicks());
}

////////////////////////////////////////////////////////////////////////////////
// app_list::AppListModelObserver:

void AppListControllerImpl::OnAppListItemAdded(app_list::AppListItem* item) {
  if (item->is_folder())
    client_->OnFolderCreated(item->CloneMetadata());
}

void AppListControllerImpl::OnAppListItemWillBeDeleted(
    app_list::AppListItem* item) {
  if (item->is_folder())
    client_->OnFolderDeleted(item->CloneMetadata());
}

void AppListControllerImpl::OnAppListItemUpdated(app_list::AppListItem* item) {
  client_->OnItemUpdated(item->CloneMetadata());
}

////////////////////////////////////////////////////////////////////////////////
// Methods used in Ash

bool AppListControllerImpl::GetTargetVisibility() const {
  return presenter_.GetTargetVisibility();
}

bool AppListControllerImpl::IsVisible() const {
  return presenter_.IsVisible();
}

void AppListControllerImpl::Show(int64_t display_id,
                                 app_list::AppListShowSource show_source,
                                 base::TimeTicks event_time_stamp) {
  UMA_HISTOGRAM_ENUMERATION(app_list::kAppListToggleMethodHistogram,
                            show_source, app_list::kMaxAppListToggleMethod);
  presenter_.Show(display_id, event_time_stamp);
}

void AppListControllerImpl::UpdateYPositionAndOpacity(
    int y_position_in_screen,
    float background_opacity) {
  presenter_.UpdateYPositionAndOpacity(y_position_in_screen,
                                       background_opacity);
}

void AppListControllerImpl::EndDragFromShelf(
    app_list::AppListViewState app_list_state) {
  presenter_.EndDragFromShelf(app_list_state);
}

void AppListControllerImpl::ProcessMouseWheelEvent(
    const ui::MouseWheelEvent& event) {
  presenter_.ProcessMouseWheelOffset(event.offset().y());
}

void AppListControllerImpl::ToggleAppList(
    int64_t display_id,
    app_list::AppListShowSource show_source,
    base::TimeTicks event_time_stamp) {
  if (!IsVisible()) {
    UMA_HISTOGRAM_ENUMERATION(app_list::kAppListToggleMethodHistogram,
                              show_source, app_list::kMaxAppListToggleMethod);
  }
  presenter_.ToggleAppList(display_id, event_time_stamp);
}

app_list::AppListViewState AppListControllerImpl::GetAppListViewState() {
  return model_.state_fullscreen();
}

void AppListControllerImpl::FlushForTesting() {
  bindings_.FlushForTesting();
}

void AppListControllerImpl::OnVirtualKeyboardStateChanged(
    bool activated,
    aura::Window* root_window) {
  auto* keyboard_controller = keyboard::KeyboardController::GetInstance();
  if (!keyboard_controller)
    return;
  if (activated && !keyboard_observer_.IsObserving(keyboard_controller))
    keyboard_observer_.Add(keyboard_controller);
  else if (!activated && keyboard_observer_.IsObserving(keyboard_controller))
    keyboard_observer_.Remove(keyboard_controller);
}

void AppListControllerImpl::OnKeyboardAvailabilityChanged(
    const bool is_available) {
  onscreen_keyboard_shown_ = is_available;
  app_list::AppListView* app_list_view = presenter_.GetView();
  if (app_list_view)
    app_list_view->OnScreenKeyboardShown(is_available);
}

////////////////////////////////////////////////////////////////////////////////
// Methods of |client_|:

void AppListControllerImpl::StartSearch(const base::string16& raw_query) {
  client_->StartSearch(raw_query);
}

void AppListControllerImpl::OpenSearchResult(const std::string& result_id,
                                             int event_flags) {
  client_->OpenSearchResult(result_id, event_flags);
}

void AppListControllerImpl::InvokeSearchResultAction(
    const std::string& result_id,
    int action_index,
    int event_flags) {
  client_->InvokeSearchResultAction(result_id, action_index, event_flags);
}

void AppListControllerImpl::ViewShown(int64_t display_id) {
  client_->ViewShown(display_id);
}

void AppListControllerImpl::ViewClosing() {
  client_->ViewClosing();
}

void AppListControllerImpl::ActivateItem(const std::string& id,
                                         int event_flags) {
  client_->ActivateItem(id, event_flags);
}

void AppListControllerImpl::GetContextMenuModel(
    const std::string& id,
    GetContextMenuModelCallback callback) {
  client_->GetContextMenuModel(id, std::move(callback));
}

void AppListControllerImpl::ContextMenuItemSelected(const std::string& id,
                                                    int command_id,
                                                    int event_flags) {
  client_->ContextMenuItemSelected(id, command_id, event_flags);
}

void AppListControllerImpl::OnVisibilityChanged(bool visible) {
  client_->OnAppListVisibilityChanged(visible);
}

void AppListControllerImpl::OnTargetVisibilityChanged(bool visible) {
  client_->OnAppListTargetVisibilityChanged(visible);
}

void AppListControllerImpl::StartVoiceInteractionSession() {
  client_->StartVoiceInteractionSession();
}

void AppListControllerImpl::ToggleVoiceInteractionSession() {
  client_->ToggleVoiceInteractionSession();
}

////////////////////////////////////////////////////////////////////////////////
// Private used only:

syncer::StringOrdinal AppListControllerImpl::GetOemFolderPos() {
  // Place the OEM folder just after the web store, which should always be
  // followed by a pre-installed app (e.g. Search), so the poosition should be
  // stable. TODO(stevenjb): consider explicitly setting the OEM folder location
  // along with the name in ServicesCustomizationDocument::SetOemFolderName().
  app_list::AppListItemList* item_list = model_.top_level_item_list();
  if (!item_list->item_count()) {
    LOG(ERROR) << "No top level item was found. "
               << "Placing OEM folder at the beginning.";
    return syncer::StringOrdinal::CreateInitialOrdinal();
  }

  size_t web_store_app_index;
  if (!item_list->FindItemIndex(extensions::kWebStoreAppId,
                                &web_store_app_index)) {
    LOG(ERROR) << "Web store position is not found it top items. "
               << "Placing OEM folder at the end.";
    return item_list->item_at(item_list->item_count() - 1)
        ->position()
        .CreateAfter();
  }

  // Skip items with the same position.
  const app_list::AppListItem* web_store_app_item =
      item_list->item_at(web_store_app_index);
  for (size_t j = web_store_app_index + 1; j < item_list->item_count(); ++j) {
    const app_list::AppListItem* next_item = item_list->item_at(j);
    DCHECK(next_item->position().IsValid());
    if (!next_item->position().Equals(web_store_app_item->position())) {
      const syncer::StringOrdinal oem_ordinal =
          web_store_app_item->position().CreateBetween(next_item->position());
      VLOG(1) << "Placing OEM Folder at: " << j
              << " position: " << oem_ordinal.ToDebugString();
      return oem_ordinal;
    }
  }

  const syncer::StringOrdinal oem_ordinal =
      web_store_app_item->position().CreateAfter();
  VLOG(1) << "Placing OEM Folder at: " << item_list->item_count()
          << " position: " << oem_ordinal.ToDebugString();
  return oem_ordinal;
}

std::unique_ptr<app_list::AppListItem> AppListControllerImpl::CreateAppListItem(
    AppListItemMetadataPtr metadata) {
  std::unique_ptr<app_list::AppListItem> app_list_item =
      std::make_unique<app_list::AppListItem>(metadata->id);
  app_list_item->SetMetadata(std::move(metadata));
  return app_list_item;
}

app_list::AppListFolderItem* AppListControllerImpl::FindFolderItem(
    const std::string& folder_id) {
  return model_.FindFolderItem(folder_id);
}

}  // namespace ash
