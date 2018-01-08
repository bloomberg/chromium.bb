// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/chrome_app_list_model_updater.h"

#include <utility>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/app_list/model/search/search_model.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "extensions/common/constants.h"
#include "ui/base/models/menu_model.h"

ChromeAppListModelUpdater::ChromeAppListModelUpdater()
    : model_(std::make_unique<app_list::AppListModel>()),
      search_model_(std::make_unique<app_list::SearchModel>()) {}

ChromeAppListModelUpdater::~ChromeAppListModelUpdater() = default;

void ChromeAppListModelUpdater::AddItem(
    std::unique_ptr<ChromeAppListItem> app_item) {
  model_->AddItem(std::move(app_item));
}

void ChromeAppListModelUpdater::AddItemToFolder(
    std::unique_ptr<ChromeAppListItem> app_item,
    const std::string& folder_id) {
  model_->AddItemToFolder(std::move(app_item), folder_id);
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

void ChromeAppListModelUpdater::SetStatus(
    app_list::AppListModel::Status status) {
  model_->SetStatus(status);
}

void ChromeAppListModelUpdater::SetState(app_list::AppListModel::State state) {
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

void ChromeAppListModelUpdater::SetSearchSpeechRecognitionButton(
    app_list::SpeechRecognitionState state) {
  search_model_->search_box()->SetSpeechRecognitionButton(state);
}

void ChromeAppListModelUpdater::UpdateSearchBox(const base::string16& text,
                                                bool initiated_by_user) {
  search_model_->search_box()->Update(text, initiated_by_user);
}

void ChromeAppListModelUpdater::ActivateChromeItem(const std::string& id,
                                                   int event_flags) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (!item)
    return;
  DCHECK(!item->is_folder());
  static_cast<ChromeAppListItem*>(item)->Activate(event_flags);
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
  return static_cast<ChromeAppListItem*>(model_->FindItem(id));
}

size_t ChromeAppListModelUpdater::ItemCount() {
  return model_->top_level_item_list()->item_count();
}

ChromeAppListItem* ChromeAppListModelUpdater::ItemAtForTest(size_t index) {
  return static_cast<ChromeAppListItem*>(
      model_->top_level_item_list()->item_at(index));
}

app_list::AppListFolderItem* ChromeAppListModelUpdater::FindFolderItem(
    const std::string& folder_id) {
  return model_->FindFolderItem(folder_id);
}

bool ChromeAppListModelUpdater::FindItemIndexForTest(const std::string& id,
                                                     size_t* index) {
  return model_->top_level_item_list()->FindItemIndex(id, index);
}

bool ChromeAppListModelUpdater::TabletMode() {
  return search_model_->tablet_mode();
}

app_list::AppListViewState ChromeAppListModelUpdater::StateFullscreen() {
  return model_->state_fullscreen();
}

bool ChromeAppListModelUpdater::SearchEngineIsGoogle() {
  return search_model_->search_engine_is_google();
}

std::map<std::string, size_t>
ChromeAppListModelUpdater::GetIdToAppListIndexMap() {
  std::map<std::string, size_t> id_to_app_list_index;
  for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i)
    id_to_app_list_index[model_->top_level_item_list()->item_at(i)->id()] = i;
  return id_to_app_list_index;
}

app_list::AppListFolderItem*
ChromeAppListModelUpdater::ResolveOemFolderPosition(
    const std::string& oem_folder_id,
    const syncer::StringOrdinal& preffered_oem_position) {
  app_list::AppListFolderItem* oem_folder = FindFolderItem(oem_folder_id);
  if (oem_folder) {
    const syncer::StringOrdinal& oem_folder_pos =
        preffered_oem_position.IsValid() ? preffered_oem_position
                                         : GetOemFolderPos();
    model_->SetItemPosition(oem_folder, oem_folder_pos);
  }
  return oem_folder;
}

ui::MenuModel* ChromeAppListModelUpdater::GetContextMenuModel(
    const std::string& id) {
  app_list::AppListItem* item = model_->FindItem(id);
  // TODO(stevenjb/jennyz): Implement this for folder items
  if (!item || item->is_folder())
    return nullptr;
  return static_cast<ChromeAppListItem*>(item)->GetContextMenuModel();
}

////////////////////////////////////////////////////////////////////////////////
// Methods for AppListSyncableService

void ChromeAppListModelUpdater::AddItemToOemFolder(
    std::unique_ptr<ChromeAppListItem> item,
    app_list::AppListSyncableService::SyncItem* oem_sync_item,
    const std::string& oem_folder_id,
    const std::string& oem_folder_name,
    const syncer::StringOrdinal& preffered_oem_position) {
  FindOrCreateOemFolder(oem_sync_item, oem_folder_id, oem_folder_name,
                        preffered_oem_position);
  AddItemToFolder(std::move(item), oem_folder_id);
}

void ChromeAppListModelUpdater::UpdateAppItemFromSyncItem(
    app_list::AppListSyncableService::SyncItem* sync_item,
    bool update_name,
    bool update_folder) {
  app_list::AppListItem* app_item = model_->FindItem(sync_item->item_id);
  if (!app_item)
    return;

  VLOG(2) << this << " UpdateAppItemFromSyncItem: " << sync_item->ToString();
  if (sync_item->item_ordinal.IsValid() &&
      !app_item->position().Equals(sync_item->item_ordinal)) {
    model_->SetItemPosition(app_item, sync_item->item_ordinal);
  }
  // Only update the item name if it is a Folder or the name is empty.
  if (update_name && sync_item->item_name != app_item->name() &&
      (app_item->GetItemType() == app_list::AppListFolderItem::kItemType ||
       app_item->name().empty())) {
    model_->SetItemName(app_item, sync_item->item_name);
  }
  if (update_folder && app_item->folder_id() != sync_item->parent_id) {
    VLOG(2) << " Moving Item To Folder: " << sync_item->parent_id;
    model_->MoveItemToFolder(app_item, sync_item->parent_id);
  }
}

////////////////////////////////////////////////////////////////////////////////
// TODO(hejq): Move the following methods to ash.

void ChromeAppListModelUpdater::FindOrCreateOemFolder(
    app_list::AppListSyncableService::SyncItem* oem_sync_item,
    const std::string& oem_folder_id,
    const std::string& oem_folder_name,
    const syncer::StringOrdinal& preffered_oem_position) {
  app_list::AppListFolderItem* oem_folder =
      model_->FindFolderItem(oem_folder_id);
  if (!oem_folder) {
    std::unique_ptr<app_list::AppListFolderItem> new_folder(
        new app_list::AppListFolderItem(
            oem_folder_id, app_list::AppListFolderItem::FOLDER_TYPE_OEM));
    syncer::StringOrdinal oem_position;
    if (oem_sync_item) {
      DCHECK(oem_sync_item->item_ordinal.IsValid());
      VLOG(1) << "Creating OEM folder from existing sync item: "
              << oem_sync_item->item_ordinal.ToDebugString();
      oem_position = oem_sync_item->item_ordinal;
    } else {
      oem_position = preffered_oem_position.IsValid() ? preffered_oem_position
                                                      : GetOemFolderPos();
      // Do not create a sync item for the OEM folder here, do it in
      // ResolveFolderPositions() when the item position is finalized.
    }
    oem_folder = static_cast<app_list::AppListFolderItem*>(
        model_->AddItem(std::move(new_folder)));
    model_->SetItemPosition(oem_folder, oem_position);
  }
  model_->SetItemName(oem_folder, oem_folder_name);
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
