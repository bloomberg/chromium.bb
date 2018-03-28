// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_CLIENT_IMPL_H_

#include <string>

#include "ash/public/interfaces/app_list.mojom.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "chrome/browser/ui/app_list/chrome_app_list_model_updater.h"
#include "mojo/public/cpp/bindings/binding.h"

class AppListViewDelegate;

class AppListClientImpl : public ash::mojom::AppListClient {
 public:
  AppListClientImpl();
  ~AppListClientImpl() override;

  // ash::mojom::AppListClient:
  void StartSearch(const base::string16& raw_query) override;
  void OpenSearchResult(const std::string& result_id, int event_flags) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index,
                                int event_flags) override;
  void ViewClosing() override;
  void ViewShown(int64_t display_id) override;
  void ActivateItem(const std::string& id, int event_flags) override;
  void GetContextMenuModel(const std::string& id,
                           GetContextMenuModelCallback callback) override;
  void ContextMenuItemSelected(const std::string& id,
                               int command_id,
                               int event_flags) override;

  void OnAppListTargetVisibilityChanged(bool visible) override;
  void OnAppListVisibilityChanged(bool visible) override;
  void StartVoiceInteractionSession() override;
  void ToggleVoiceInteractionSession() override;

  void OnFolderCreated(ash::mojom::AppListItemMetadataPtr item) override;
  void OnFolderDeleted(ash::mojom::AppListItemMetadataPtr item) override;
  void OnItemUpdated(ash::mojom::AppListItemMetadataPtr item) override;

  // Flushes all pending mojo call to Ash for testing.
  void FlushMojoForTesting();

 private:
  AppListViewDelegate* GetViewDelegate();

  mojo::Binding<ash::mojom::AppListClient> binding_;
  ash::mojom::AppListControllerPtr app_list_controller_;

  DISALLOW_COPY_AND_ASSIGN(AppListClientImpl);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_CLIENT_IMPL_H_
