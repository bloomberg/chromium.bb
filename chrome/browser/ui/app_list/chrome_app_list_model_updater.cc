// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/chrome_app_list_model_updater.h"

#include <unordered_map>
#include <utility>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/app_list/model/search/search_model.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "extensions/common/constants.h"
#include "ui/base/models/menu_model.h"

ChromeAppListModelUpdater::ChromeAppListModelUpdater(Profile* profile)
    : model_(std::make_unique<app_list::AppListModel>()),
      search_model_(std::make_unique<app_list::SearchModel>()),
      profile_(profile) {
  model_->AddObserver(this);
}

ChromeAppListModelUpdater::~ChromeAppListModelUpdater() {
  model_->RemoveObserver(this);
}

app_list::AppListModel* ChromeAppListModelUpdater::GetModel() {
  return model_.get();
}

app_list::SearchModel* ChromeAppListModelUpdater::GetSearchModel() {
  return search_model_.get();
}

void ChromeAppListModelUpdater::AddItem(
    std::unique_ptr<ChromeAppListItem> app_item) {
  ash::mojom::AppListItemMetadataPtr item_data = app_item->CloneMetadata();
  // Add to Chrome first leave all updates to observer methods.
  AddChromeItem(std::move(app_item));
  model_->AddItem(CreateAppListItem(std::move(item_data)));
}

void ChromeAppListModelUpdater::AddItemToFolder(
    std::unique_ptr<ChromeAppListItem> app_item,
    const std::string& folder_id) {
  ash::mojom::AppListItemMetadataPtr item_data = app_item->CloneMetadata();
  // Add to Chrome first leave all updates to observer methods.
  AddChromeItem(std::move(app_item));
  model_->AddItemToFolder(CreateAppListItem(std::move(item_data)), folder_id);
}

void ChromeAppListModelUpdater::RemoveItem(const std::string& id) {
  model_->DeleteItem(id);
}

void ChromeAppListModelUpdater::RemoveUninstalledItem(const std::string& id) {
  model_->DeleteUninstalledItem(id);
}

void ChromeAppListModelUpdater::MoveItemToFolder(const std::string& id,
                                                 const std::string& folder_id) {
  app_list::AppListItem* item = model_->FindItem(id);
  model_->MoveItemToFolder(item, folder_id);
}

void ChromeAppListModelUpdater::SetStatus(ash::AppListModelStatus status) {
  model_->SetStatus(status);
}

void ChromeAppListModelUpdater::SetState(ash::AppListState state) {
  model_->SetState(state);
}

void ChromeAppListModelUpdater::HighlightItemInstalledFromUI(
    const std::string& id) {
  model_->top_level_item_list()->HighlightItemInstalledFromUI(id);
}

void ChromeAppListModelUpdater::SetSearchEngineIsGoogle(bool is_google) {
  search_model_->SetSearchEngineIsGoogle(is_google);
}

void ChromeAppListModelUpdater::SetSearchTabletAndClamshellAccessibleName(
    const base::string16& tablet_accessible_name,
    const base::string16& clamshell_accessible_name) {
  search_model_->search_box()->SetTabletAndClamshellAccessibleName(
      tablet_accessible_name, clamshell_accessible_name);
}

void ChromeAppListModelUpdater::SetSearchHintText(
    const base::string16& hint_text) {
  search_model_->search_box()->SetHintText(hint_text);
}

void ChromeAppListModelUpdater::UpdateSearchBox(const base::string16& text,
                                                bool initiated_by_user) {
  search_model_->search_box()->Update(text, initiated_by_user);
}

void ChromeAppListModelUpdater::PublishSearchResults(
    std::vector<std::unique_ptr<app_list::SearchResult>> results) {
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

////////////////////////////////////////////////////////////////////////////////
// Methods only used by ChromeAppListItem that talk to ash directly.

void ChromeAppListModelUpdater::SetItemIcon(const std::string& id,
                                            const gfx::ImageSkia& icon) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    item->SetIcon(icon);
}

void ChromeAppListModelUpdater::SetItemName(const std::string& id,
                                            const std::string& name) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    model_->SetItemName(item, name);
}

void ChromeAppListModelUpdater::SetItemNameAndShortName(
    const std::string& id,
    const std::string& name,
    const std::string& short_name) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    item->SetNameAndShortName(name, short_name);
}

void ChromeAppListModelUpdater::SetItemPosition(
    const std::string& id,
    const syncer::StringOrdinal& new_position) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    model_->SetItemPosition(item, new_position);
}

void ChromeAppListModelUpdater::SetItemFolderId(const std::string& id,
                                                const std::string& folder_id) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    item->set_folder_id(folder_id);
}

void ChromeAppListModelUpdater::SetItemIsInstalling(const std::string& id,
                                                    bool is_installing) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    item->SetIsInstalling(is_installing);
}

void ChromeAppListModelUpdater::SetItemPercentDownloaded(
    const std::string& id,
    int32_t percent_downloaded) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    item->SetPercentDownloaded(percent_downloaded);
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
  return search_model_->search_engine_is_google();
}

void ChromeAppListModelUpdater::GetIdToAppListIndexMap(
    GetIdToAppListIndexMapCallback callback) {
  std::unordered_map<std::string, size_t> id_to_app_list_index;
  for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i)
    id_to_app_list_index[model_->top_level_item_list()->item_at(i)->id()] = i;
  std::move(callback).Run(id_to_app_list_index);
}

size_t ChromeAppListModelUpdater::BadgedItemCount() {
  size_t count = 0u;
  for (auto& key_val : items_) {
    if (key_val.second->IsBadged())
      ++count;
  }
  return count;
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
  return search_model_->FindSearchResult(result_id);
}

////////////////////////////////////////////////////////////////////////////////
// Methods for AppListSyncableService

void ChromeAppListModelUpdater::ResolveOemFolderPosition(
    const std::string& oem_folder_id,
    const syncer::StringOrdinal& preferred_oem_position,
    ResolveOemFolderPositionCallback callback) {
  // In ash:
  app_list::AppListFolderItem* ash_oem_folder =
      FindAshFolderItem(oem_folder_id);
  ash::mojom::AppListItemMetadataPtr metadata = nullptr;
  if (ash_oem_folder) {
    const syncer::StringOrdinal& oem_folder_pos =
        preferred_oem_position.IsValid() ? preferred_oem_position
                                         : GetOemFolderPos();
    model_->SetItemPosition(ash_oem_folder, oem_folder_pos);
    metadata = ash_oem_folder->CloneMetadata();
  }
  // In chrome, metadata returned from ash:
  ChromeAppListItem* chrome_oem_folder = FindFolderItem(oem_folder_id);
  if (ash_oem_folder) {
    DCHECK(metadata);
    chrome_oem_folder->SetMetadata(std::move(metadata));
  }
  std::move(callback).Run(chrome_oem_folder);
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

////////////////////////////////////////////////////////////////////////////////
// Methods for AppListSyncableService

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
  // In ash:
  FindOrCreateOemFolder(oem_folder_id, oem_folder_name, position_to_try);
  // In chrome, after oem folder is created:
  AddItemToFolder(std::move(item), oem_folder_id);
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
      !chrome_item->position().Equals(sync_item->item_ordinal)) {
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

void ChromeAppListModelUpdater::OnAppListItemAdded(
    app_list::AppListItem* item) {
  ChromeAppListItem* chrome_item = FindItem(item->id());
  if (!chrome_item) {
    // We detect an item is created in Ash which is not added into our Chrome
    // list yet. This will only happen when a folder is created.
    DCHECK(item->is_folder());
    std::unique_ptr<ChromeAppListItem> new_item =
        std::make_unique<ChromeAppListItem>(profile_, item->id());
    chrome_item = AddChromeItem(std::move(new_item));
  }
  chrome_item->SetMetadata(item->CloneMetadata());
  if (delegate_)
    delegate_->OnAppListItemAdded(chrome_item);
}

void ChromeAppListModelUpdater::OnAppListItemWillBeDeleted(
    app_list::AppListItem* item) {
  ChromeAppListItem* chrome_item = FindItem(item->id());
  DCHECK(chrome_item);
  if (delegate_)
    delegate_->OnAppListItemWillBeDeleted(chrome_item);
}

void ChromeAppListModelUpdater::OnAppListItemDeleted(const std::string& id) {
  items_.erase(id);
}

void ChromeAppListModelUpdater::OnAppListItemUpdated(
    app_list::AppListItem* item) {
  ChromeAppListItem* chrome_item = FindItem(item->id());
  DCHECK(chrome_item);
  chrome_item->SetMetadata(item->CloneMetadata());
  if (delegate_)
    delegate_->OnAppListItemUpdated(chrome_item);
}

////////////////////////////////////////////////////////////////////////////////
// TODO(hejq): Move the following methods to ash.

ash::mojom::AppListItemMetadataPtr
ChromeAppListModelUpdater::FindOrCreateOemFolder(
    const std::string& oem_folder_id,
    const std::string& oem_folder_name,
    const syncer::StringOrdinal& preferred_oem_position) {
  app_list::AppListFolderItem* oem_folder =
      model_->FindFolderItem(oem_folder_id);
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
        model_->AddItem(std::move(new_folder)));
    model_->SetItemPosition(oem_folder, oem_position);
  }
  model_->SetItemName(oem_folder, oem_folder_name);
  return oem_folder->CloneMetadata();
}

syncer::StringOrdinal ChromeAppListModelUpdater::GetOemFolderPos() {
  // Place the OEM folder just after the web store, which should always be
  // followed by a pre-installed app (e.g. Search), so the poosition should be
  // stable. TODO(stevenjb): consider explicitly setting the OEM folder location
  // along with the name in ServicesCustomizationDocument::SetOemFolderName().
  app_list::AppListItemList* item_list = model_->top_level_item_list();
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

app_list::AppListFolderItem* ChromeAppListModelUpdater::FindAshFolderItem(
    const std::string& folder_id) {
  return model_->FindFolderItem(folder_id);
}

std::unique_ptr<app_list::AppListItem>
ChromeAppListModelUpdater::CreateAppListItem(
    ash::mojom::AppListItemMetadataPtr metadata) {
  std::unique_ptr<app_list::AppListItem> ash_app_list_item =
      std::make_unique<app_list::AppListItem>(metadata->id);
  ash_app_list_item->SetMetadata(std::move(metadata));

  ChromeAppListItem* chrome_app_item = FindItem(ash_app_list_item->id());
  if (chrome_app_item && !chrome_app_item->icon().isNull())
    ash_app_list_item->SetIcon(chrome_app_item->icon());

  return ash_app_list_item;
}
