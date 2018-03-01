// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_CONTROLLER_IMPL_H_
#define ASH_APP_LIST_APP_LIST_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "ash/app_list/app_list_view_delegate_mash.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/ash_export.h"
#include "ash/public/interfaces/app_list.mojom.h"
#include "components/sync/model/string_ordinal.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"

namespace ash {

// Ash's AppListController owns the AppListModel and implements interface
// functions that allow Chrome to modify and observe the Shelf and AppListModel
// state.
class ASH_EXPORT AppListControllerImpl : public mojom::AppListController,
                                         public app_list::AppListModelObserver {
 public:
  using AppListItemMetadataPtr = mojom::AppListItemMetadataPtr;
  AppListControllerImpl();
  ~AppListControllerImpl() override;

  // Binds the mojom::AppListController interface request to this object.
  void BindRequest(mojom::AppListControllerRequest request);

  app_list::AppListModel* model() { return &model_; }
  app_list::SearchModel* search_model() { return &search_model_; }
  app_list::AppListPresenterImpl* presenter() { return &presenter_; }
  mojom::AppListClient* client() { return client_.get(); }

  // mojom::AppListController:
  void SetClient(mojom::AppListClientPtr client_ptr) override;
  void AddItem(AppListItemMetadataPtr app_item) override;
  void AddItemToFolder(AppListItemMetadataPtr app_item,
                       const std::string& folder_id) override;
  void RemoveItem(const std::string& id) override;
  void RemoveUninstalledItem(const std::string& id) override;
  void MoveItemToFolder(const std::string& id,
                        const std::string& folder_id) override;
  void SetStatus(ash::AppListModelStatus status) override;
  void SetState(ash::AppListState state) override;
  void HighlightItemInstalledFromUI(const std::string& id) override;
  void SetSearchEngineIsGoogle(bool is_google) override;
  void SetSearchTabletAndClamshellAccessibleName(
      const base::string16& tablet_accessible_name,
      const base::string16& clamshell_accessible_name) override;
  void SetSearchHintText(const base::string16& hint_text) override;
  void UpdateSearchBox(const base::string16& text,
                       bool initiated_by_user) override;
  void SetItemMetadata(const std::string& id,
                       AppListItemMetadataPtr data) override;
  void SetItemIcon(const std::string& id, const gfx::ImageSkia& icon) override;
  void SetItemIsInstalling(const std::string& id, bool is_installing) override;
  void SetItemPercentDownloaded(const std::string& id,
                                int32_t percent_downloaded) override;

  void GetIdToAppListIndexMap(GetIdToAppListIndexMapCallback callback) override;
  void FindOrCreateOemFolder(
      const std::string& oem_folder_id,
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preferred_oem_position,
      FindOrCreateOemFolderCallback callback) override;
  void ResolveOemFolderPosition(
      const std::string& oem_folder_id,
      const syncer::StringOrdinal& preferred_oem_position,
      ResolveOemFolderPositionCallback callback) override;

  void DismissAppList() override;
  void GetAppInfoDialogBounds(GetAppInfoDialogBoundsCallback callback) override;
  void ShowAppListAndSwitchToState(ash::AppListState state) override;
  void ShowAppList() override;

  // app_list::AppListModelObserver:
  void OnAppListItemAdded(app_list::AppListItem* item) override;
  void OnAppListItemWillBeDeleted(app_list::AppListItem* item) override;
  void OnAppListItemUpdated(app_list::AppListItem* item) override;

 private:
  syncer::StringOrdinal GetOemFolderPos();
  std::unique_ptr<app_list::AppListItem> CreateAppListItem(
      AppListItemMetadataPtr metadata);
  app_list::AppListFolderItem* FindFolderItem(const std::string& folder_id);

  AppListViewDelegateMash view_delegate_;
  app_list::AppListPresenterImpl presenter_;
  app_list::AppListModel model_;
  app_list::SearchModel search_model_;

  // Bindings for the AppListController interface.
  mojo::BindingSet<mojom::AppListController> bindings_;

  mojom::AppListClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerImpl);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_CONTROLLER_IMPL_H_
