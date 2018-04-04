// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_client_impl.h"

#include <utility>

#include "ash/public/cpp/menu_utils.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/geometry/rect.h"

AppListClientImpl::AppListClientImpl() : binding_(this) {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &app_list_controller_);
  ash::mojom::AppListClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  app_list_controller_->SetClient(std::move(client));
  AppListServiceImpl::GetInstance()->SetAppListControllerAndClient(
      app_list_controller_.get(), this);
}

AppListClientImpl::~AppListClientImpl() = default;

void AppListClientImpl::StartSearch(const base::string16& raw_query) {
  if (GetViewDelegate()->search_controller_) {
    GetViewDelegate()->search_controller_->Start(raw_query);
    GetViewDelegate()->controller_->OnSearchStarted();
  }
}

void AppListClientImpl::OpenSearchResult(const std::string& result_id,
                                         int event_flags) {
  GetViewDelegate()->OpenSearchResult(result_id, event_flags);
}

void AppListClientImpl::InvokeSearchResultAction(const std::string& result_id,
                                                 int action_index,
                                                 int event_flags) {
  GetViewDelegate()->InvokeSearchResultAction(result_id, action_index,
                                              event_flags);
}

void AppListClientImpl::ViewClosing() {
  GetViewDelegate()->ViewClosing();
}

void AppListClientImpl::ViewShown(int64_t display_id) {
  GetViewDelegate()->ViewShown(display_id);
}

void AppListClientImpl::ActivateItem(const std::string& id, int event_flags) {
  GetViewDelegate()->model_updater_->ActivateChromeItem(id, event_flags);
}

void AppListClientImpl::GetContextMenuModel(
    const std::string& id,
    GetContextMenuModelCallback callback) {
  ui::MenuModel* menu =
      GetViewDelegate()->model_updater_->GetContextMenuModel(id);
  std::move(callback).Run(ash::menu_utils::GetMojoMenuItemsFromModel(menu));
}

void AppListClientImpl::ContextMenuItemSelected(const std::string& id,
                                                int command_id,
                                                int event_flags) {
  GetViewDelegate()->model_updater_->ContextMenuItemSelected(id, command_id,
                                                             event_flags);
}

void AppListClientImpl::OnAppListTargetVisibilityChanged(bool visible) {
  AppListServiceImpl::GetInstance()->set_app_list_target_visible(visible);
}

void AppListClientImpl::OnAppListVisibilityChanged(bool visible) {
  AppListServiceImpl::GetInstance()->set_app_list_visible(visible);
}

void AppListClientImpl::StartVoiceInteractionSession() {
  auto* service =
      arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(
          ChromeLauncherController::instance()->profile());
  if (service)
    service->StartSessionFromUserInteraction(gfx::Rect());
}

void AppListClientImpl::ToggleVoiceInteractionSession() {
  auto* service =
      arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(
          ChromeLauncherController::instance()->profile());
  if (service)
    service->ToggleSessionFromUserInteraction();
}

void AppListClientImpl::OnFolderCreated(
    ash::mojom::AppListItemMetadataPtr item) {
  DCHECK(item->is_folder);
  GetViewDelegate()->model_updater_->OnFolderCreated(std::move(item));
}

void AppListClientImpl::OnFolderDeleted(
    ash::mojom::AppListItemMetadataPtr item) {
  DCHECK(item->is_folder);
  GetViewDelegate()->model_updater_->OnFolderDeleted(std::move(item));
}

void AppListClientImpl::OnItemUpdated(ash::mojom::AppListItemMetadataPtr item) {
  GetViewDelegate()->model_updater_->OnItemUpdated(std::move(item));
}

AppListViewDelegate* AppListClientImpl::GetViewDelegate() {
  return AppListServiceImpl::GetInstance()->GetViewDelegate();
}

void AppListClientImpl::FlushMojoForTesting() {
  app_list_controller_.FlushForTesting();
  binding_.FlushForTesting();
}
