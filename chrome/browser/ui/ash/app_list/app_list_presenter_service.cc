// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_list/app_list_presenter_service.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"
#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"
#include "components/arc/arc_service_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"

AppListPresenterService::AppListPresenterService() : binding_(this) {
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  if (connection && connection->GetConnector()) {
    // Connect to the app list interface in the ash service.
    app_list::mojom::AppListPtr app_list_ptr;
    connection->GetConnector()->BindInterface(ash::mojom::kServiceName,
                                              &app_list_ptr);
    // Register this object as the app list presenter.
    app_list_ptr->SetAppListPresenter(binding_.CreateInterfacePtrAndBind());
    // Pass the interface pointer to the presenter to report visibility changes.
    GetPresenter()->SetAppList(std::move(app_list_ptr));
  }
}

AppListPresenterService::~AppListPresenterService() {}

void AppListPresenterService::Show(int64_t display_id) {
  GetPresenter()->Show(display_id);
}

void AppListPresenterService::Dismiss() {
  GetPresenter()->Dismiss();
}

void AppListPresenterService::ToggleAppList(int64_t display_id) {
  GetPresenter()->ToggleAppList(display_id);
}

void AppListPresenterService::StartVoiceInteractionSession() {
  arc::ArcVoiceInteractionFrameworkService* service =
      arc::ArcServiceManager::Get()
          ->GetService<arc::ArcVoiceInteractionFrameworkService>();
  if (service)
    service->StartVoiceInteractionSession();
}

app_list::AppListPresenterImpl* AppListPresenterService::GetPresenter() {
  return AppListServiceAsh::GetInstance()->GetAppListPresenter();
}
