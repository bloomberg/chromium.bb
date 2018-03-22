// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/chrome_app_list_model_updater.h"

#include <unordered_map>
#include <utility>

#include "ash/app_list/model/search/search_model.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "extensions/common/constants.h"
#include "ui/base/models/menu_model.h"

ChromeAppListModelUpdater::ChromeAppListModelUpdater(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  // TODO(hejq): remove this when search migration is done.
  if (!ash_util::IsRunningInMash())
    search_model_ = AppListServiceImpl::GetInstance()->GetSearchModelFromAsh();
}

ChromeAppListModelUpdater::~ChromeAppListModelUpdater() {}

void ChromeAppListModelUpdater::SetActive(bool active) {
  const bool was_active = !!app_list_controller_;
  if (was_active == active)
    return;

  app_list_controller_ =
      active ? AppListServiceImpl::GetInstance()->GetAppListController()
             : nullptr;
  if (!app_list_controller_)
    return;

  // Activating this model updater should sync the cached model to Ash.
  std::vector<ash::mojom::AppListItemMetadataPtr> items_to_sync;
  for (auto const& item : items_)
    items_to_sync.push_back(item.second->CloneMetadata());
  app_list_controller_->SetModelData(std::move(items_to_sync),
                                     search_engine_is_google_);
}

void ChromeAppListModelUpdater::AddItem(
    std::unique_ptr<ChromeAppListItem> app_item) {
  ash::mojom::AppListItemMetadataPtr item_data = app_item->CloneMetadata();
  // Add to Chrome first leave all updates to observer methods.
  AddChromeItem(std::move(app_item));
  if (app_list_controller_)
    app_list_controller_->AddItem(std::move(item_data));
}

void ChromeAppListModelUpdater::AddItemToFolder(
    std::unique_ptr<ChromeAppListItem> app_item,
    const std::string& folder_id) {
  ash::mojom::AppListItemMetadataPtr item_data = app_item->CloneMetadata();
  // Add to Chrome first leave all updates to observer methods.
  ChromeAppListItem* item_added = AddChromeItem(std::move(app_item));
  item_added->SetChromeFolderId(folder_id);
  if (app_list_controller_) {
    app_list_controller_->AddItemToFolder(std::move(item_data), folder_id);
    // Set the item's default icon if it has one.
    if (!item_added->icon().isNull())
      app_list_controller_->SetItemIcon(item_added->id(), item_added->icon());
  }
}

void ChromeAppListModelUpdater::RemoveItem(const std::string& id) {
  if (app_list_controller_)
    app_list_controller_->RemoveItem(id);
  else
    RemoveChromeItem(id);
}

void ChromeAppListModelUpdater::RemoveUninstalledItem(const std::string& id) {
  if (app_list_controller_)
    app_list_controller_->RemoveUninstalledItem(id);
  else
    RemoveChromeItem(id);
}

void ChromeAppListModelUpdater::MoveItemToFolder(const std::string& id,
                                                 const std::string& folder_id) {
  if (app_list_controller_)
    app_list_controller_->MoveItemToFolder(id, folder_id);
  else
    MoveChromeItemToFolder(id, folder_id);
}

void ChromeAppListModelUpdater::SetStatus(ash::AppListModelStatus status) {
  if (!app_list_controller_)
    return;
  app_list_controller_->SetStatus(status);
}

void ChromeAppListModelUpdater::SetState(ash::AppListState state) {
  if (!app_list_controller_)
    return;
  app_list_controller_->SetState(state);
}

void ChromeAppListModelUpdater::HighlightItemInstalledFromUI(
    const std::string& id) {
  if (!app_list_controller_)
    return;
  app_list_controller_->HighlightItemInstalledFromUI(id);
}

void ChromeAppListModelUpdater::SetSearchEngineIsGoogle(bool is_google) {
  search_engine_is_google_ = is_google;
  if (app_list_controller_)
    app_list_controller_->SetSearchEngineIsGoogle(is_google);
}

void ChromeAppListModelUpdater::SetSearchTabletAndClamshellAccessibleName(
    const base::string16& tablet_accessible_name,
    const base::string16& clamshell_accessible_name) {
  if (!app_list_controller_)
    return;
  app_list_controller_->SetSearchTabletAndClamshellAccessibleName(
      tablet_accessible_name, clamshell_accessible_name);
}

void ChromeAppListModelUpdater::SetSearchHintText(
    const base::string16& hint_text) {
  if (!app_list_controller_)
    return;
  app_list_controller_->SetSearchHintText(hint_text);
}

void ChromeAppListModelUpdater::UpdateSearchBox(const base::string16& text,
                                                bool initiated_by_user) {
  if (!ash_util::IsRunningInMash())
    search_model_->search_box()->Update(text, initiated_by_user);
  else if (app_list_controller_)
    app_list_controller_->UpdateSearchBox(text, initiated_by_user);
}

void ChromeAppListModelUpdater::PublishSearchResults(
    std::vector<std::unique_ptr<app_list::SearchResult>> results) {
  if (!ash_util::IsRunningInMash())
    search_model_->PublishResults(std::move(results));
}

void ChromeAppListModelUpdater::ActivateChromeItem(const std::string& id,
                                                   int event_flags) {
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;
  DCHECK(!item->is_folder());
  item->Activate(event_flags);
}

////////////////////////////////////////////////////////////////////////////////
// Methods for updating Chrome items that never talk to ash.

ChromeAppListItem* ChromeAppListModelUpdater::AddChromeItem(
    std::unique_ptr<ChromeAppListItem> app_item) {
  ChromeAppListItem* item = app_item.get();
  items_[app_item->id()] = std::move(app_item);
  return item;
}

void ChromeAppListModelUpdater::RemoveChromeItem(const std::string& id) {
  items_.erase(id);
}

void ChromeAppListModelUpdater::MoveChromeItemToFolder(
    const std::string& id,
    const std::string& folder_id) {
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;
  item->SetChromeFolderId(folder_id);
}

////////////////////////////////////////////////////////////////////////////////
// Methods only used by ChromeAppListItem that talk to ash directly.

void ChromeAppListModelUpdater::SetItemIcon(const std::string& id,
                                            const gfx::ImageSkia& icon) {
  if (!app_list_controller_)
    return;
  app_list_controller_->SetItemIcon(id, icon);
}

void ChromeAppListModelUpdater::SetItemName(const std::string& id,
                                            const std::string& name) {
  if (!app_list_controller_)
    return;
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;
  ash::mojom::AppListItemMetadataPtr data = item->CloneMetadata();
  data->name = name;
  app_list_controller_->SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetItemNameAndShortName(
    const std::string& id,
    const std::string& name,
    const std::string& short_name) {
  if (!app_list_controller_)
    return;
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;
  ash::mojom::AppListItemMetadataPtr data = item->CloneMetadata();
  data->name = name;
  data->short_name = short_name;
  app_list_controller_->SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetItemPosition(
    const std::string& id,
    const syncer::StringOrdinal& new_position) {
  if (!app_list_controller_)
    return;
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;
  ash::mojom::AppListItemMetadataPtr data = item->CloneMetadata();
  data->position = new_position;
  app_list_controller_->SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetItemFolderId(const std::string& id,
                                                const std::string& folder_id) {
  if (!app_list_controller_)
    return;
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;
  ash::mojom::AppListItemMetadataPtr data = item->CloneMetadata();
  data->folder_id = folder_id;
  app_list_controller_->SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetItemIsInstalling(const std::string& id,
                                                    bool is_installing) {
  if (!app_list_controller_)
    return;
  app_list_controller_->SetItemIsInstalling(id, is_installing);
}

void ChromeAppListModelUpdater::SetItemPercentDownloaded(
    const std::string& id,
    int32_t percent_downloaded) {
  if (!app_list_controller_)
    return;
  app_list_controller_->SetItemPercentDownloaded(id, percent_downloaded);
}

////////////////////////////////////////////////////////////////////////////////
// Methods for item querying

ChromeAppListItem* ChromeAppListModelUpdater::FindItem(const std::string& id) {
  return items_.count(id) ? items_[id].get() : nullptr;
}

size_t ChromeAppListModelUpdater::ItemCount() {
  return items_.size();
}

ChromeAppListItem* ChromeAppListModelUpdater::ItemAtForTest(size_t index) {
  DCHECK_LT(index, items_.size());
  DCHECK_LE(0u, index);
  auto it = items_.begin();
  for (size_t i = 0; i < index; ++i)
    ++it;
  return it->second.get();
}

ChromeAppListItem* ChromeAppListModelUpdater::FindFolderItem(
    const std::string& folder_id) {
  ChromeAppListItem* item = FindItem(folder_id);
  return (item && item->is_folder()) ? item : nullptr;
}

bool ChromeAppListModelUpdater::FindItemIndexForTest(const std::string& id,
                                                     size_t* index) {
  *index = 0;
  for (auto it = items_.begin(); it != items_.end(); ++it) {
    if (it->second->id() == id)
      return true;
    ++(*index);
  }
  return false;
}

bool ChromeAppListModelUpdater::SearchEngineIsGoogle() {
  if (!ash_util::IsRunningInMash())
    return search_model_->search_engine_is_google();
  return false;
}

void ChromeAppListModelUpdater::GetIdToAppListIndexMap(
    GetIdToAppListIndexMapCallback callback) {
  if (!app_list_controller_)
    return;
  app_list_controller_->GetIdToAppListIndexMap(base::BindOnce(
      [](GetIdToAppListIndexMapCallback callback,
         const std::unordered_map<std::string, uint16_t>& indexes) {
        std::move(callback).Run(indexes);
      },
      std::move(callback)));
}

size_t ChromeAppListModelUpdater::BadgedItemCount() {
  size_t count = 0u;
  for (auto& key_val : items_) {
    if (key_val.second->IsBadged())
      ++count;
  }
  return count;
}

ui::MenuModel* ChromeAppListModelUpdater::GetContextMenuModel(
    const std::string& id) {
  ChromeAppListItem* item = FindItem(id);
  // TODO(stevenjb/jennyz): Implement this for folder items.
  // TODO(newcomer): Add histograms for folder items.
  if (!item || item->is_folder())
    return nullptr;
  return item->GetContextMenuModel();
}

void ChromeAppListModelUpdater::ContextMenuItemSelected(const std::string& id,
                                                        int command_id,
                                                        int event_flags) {
  ChromeAppListItem* chrome_item = FindItem(id);
  if (chrome_item)
    chrome_item->ContextMenuItemSelected(command_id, event_flags);
}

app_list::SearchResult* ChromeAppListModelUpdater::FindSearchResult(
    const std::string& result_id) {
  return search_model_ ? search_model_->FindSearchResult(result_id) : nullptr;
}

app_list::SearchResult* ChromeAppListModelUpdater::GetResultByTitle(
    const std::string& title) {
  if (!search_model_)
    return nullptr;

  base::string16 target_title = base::ASCIIToUTF16(title);
  // TODO(hejq): Currently we use a search result's type and diaplay type to
  //             check whether it's a result of uninstalled result. We might
  //             have an attribute to do this when we refactor SearchResult.
  for (const auto& result : *search_model_->results()) {
    if (result->title() == target_title &&
        result->result_type() == ash::SearchResultType::kInstalledApp &&
        result->display_type() !=
            ash::SearchResultDisplayType::kRecommendation) {
      return result.get();
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Methods for AppListSyncableService

void ChromeAppListModelUpdater::ResolveOemFolderPosition(
    const std::string& oem_folder_id,
    const syncer::StringOrdinal& preferred_oem_position,
    ResolveOemFolderPositionCallback callback) {
  if (!app_list_controller_)
    return;
  app_list_controller_->ResolveOemFolderPosition(
      oem_folder_id, preferred_oem_position,
      base::BindOnce(
          [](base::WeakPtr<ChromeAppListModelUpdater> self,
             const std::string& oem_folder_id,
             ResolveOemFolderPositionCallback callback,
             ash::mojom::AppListItemMetadataPtr folder_data) {
            if (!self)
              return;
            ChromeAppListItem* chrome_oem_folder = nullptr;
            if (folder_data) {
              chrome_oem_folder = self->FindFolderItem(oem_folder_id);
              chrome_oem_folder->SetMetadata(std::move(folder_data));
            }
            std::move(callback).Run(chrome_oem_folder);
          },
          weak_ptr_factory_.GetWeakPtr(), oem_folder_id, std::move(callback)));
}

void ChromeAppListModelUpdater::AddItemToOemFolder(
    std::unique_ptr<ChromeAppListItem> item,
    app_list::AppListSyncableService::SyncItem* oem_sync_item,
    const std::string& oem_folder_id,
    const std::string& oem_folder_name,
    const syncer::StringOrdinal& preferred_oem_position) {
  syncer::StringOrdinal position_to_try = preferred_oem_position;
  // If we find a valid postion in the sync item, then we'll try it.
  if (oem_sync_item && oem_sync_item->item_ordinal.IsValid())
    position_to_try = oem_sync_item->item_ordinal;

  if (app_list_controller_) {
    app_list_controller_->FindOrCreateOemFolder(
        oem_folder_id, oem_folder_name, position_to_try,
        base::BindOnce(
            [](base::WeakPtr<ChromeAppListModelUpdater> self,
               std::unique_ptr<ChromeAppListItem> item,
               const std::string& oem_folder_id,
               ash::mojom::AppListItemMetadataPtr /* oem_folder */) {
              if (!self)
                return;
              self->AddItemToFolder(std::move(item), oem_folder_id);
            },
            weak_ptr_factory_.GetWeakPtr(), std::move(item), oem_folder_id));
  } else {
    ChromeAppListItem* item_added = AddChromeItem(std::move(item));
    item_added->SetChromeFolderId(oem_folder_id);
    // If we don't have an OEM folder in Chrome, create one first.
    ChromeAppListItem* oem_folder = FindFolderItem(oem_folder_id);
    if (!oem_folder) {
      std::unique_ptr<ChromeAppListItem> new_oem_folder =
          std::make_unique<ChromeAppListItem>(profile_, oem_folder_id);
      oem_folder = AddChromeItem(std::move(new_oem_folder));
      oem_folder->SetChromeIsFolder(true);
    }
    oem_folder->SetChromeName(oem_folder_name);
    oem_folder->SetChromePosition(position_to_try);
  }
}

void ChromeAppListModelUpdater::UpdateAppItemFromSyncItem(
    app_list::AppListSyncableService::SyncItem* sync_item,
    bool update_name,
    bool update_folder) {
  // In chrome & ash:
  ChromeAppListItem* chrome_item = FindItem(sync_item->item_id);
  if (!chrome_item)
    return;

  VLOG(2) << this << " UpdateAppItemFromSyncItem: " << sync_item->ToString();
  if (sync_item->item_ordinal.IsValid() &&
      (!chrome_item->position().IsValid() ||
       !chrome_item->position().Equals(sync_item->item_ordinal))) {
    // This updates the position in both chrome and ash:
    chrome_item->SetPosition(sync_item->item_ordinal);
  }
  // Only update the item name if it is a Folder or the name is empty.
  if (update_name && sync_item->item_name != chrome_item->name() &&
      (chrome_item->is_folder() || chrome_item->name().empty())) {
    // This updates the name in both chrome and ash:
    chrome_item->SetName(sync_item->item_name);
  }
  if (update_folder && chrome_item->folder_id() != sync_item->parent_id) {
    VLOG(2) << " Moving Item To Folder: " << sync_item->parent_id;
    // This updates the folder in both chrome and ash:
    MoveItemToFolder(chrome_item->id(), sync_item->parent_id);
  }
}

void ChromeAppListModelUpdater::SetDelegate(
    AppListModelUpdaterDelegate* delegate) {
  delegate_ = delegate;
}

////////////////////////////////////////////////////////////////////////////////
// Methods called from Ash:

void ChromeAppListModelUpdater::OnFolderCreated(
    ash::mojom::AppListItemMetadataPtr item) {
  DCHECK(item->is_folder);
  ChromeAppListItem* chrome_item = FindItem(item->id);
  // If the item already exists, we should have set its information properly.
  if (chrome_item)
    return;
  // Otherwise, we detect an item is created in Ash which is not added into our
  // Chrome list yet. This only happens when a folder is created.
  std::unique_ptr<ChromeAppListItem> new_item =
      std::make_unique<ChromeAppListItem>(profile_, item->id);
  chrome_item = AddChromeItem(std::move(new_item));
  chrome_item->SetMetadata(std::move(item));
  if (delegate_)
    delegate_->OnAppListItemAdded(chrome_item);
}

void ChromeAppListModelUpdater::OnFolderDeleted(
    ash::mojom::AppListItemMetadataPtr item) {
  DCHECK(item->is_folder);
  items_.erase(item->id);
  // We don't need to notify |delegate_| here. Currently |delegate_| sends this
  // event to AppListSyncableService only, and AppListSyncableService doesn't
  // do anything when a folder is deleted. For more details, refer to
  // AppListSyncableService::ModelUpdaterDelegate::OnAppListItemWillBeDeleted.
}

void ChromeAppListModelUpdater::OnItemUpdated(
    ash::mojom::AppListItemMetadataPtr item) {
  ChromeAppListItem* chrome_item = FindItem(item->id);
  DCHECK(chrome_item);
  chrome_item->SetMetadata(std::move(item));
  if (delegate_)
    delegate_->OnAppListItemUpdated(chrome_item);
}
