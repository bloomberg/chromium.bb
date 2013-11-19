// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_syncable_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/extension_app_model_builder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "ui/app_list/app_list_model.h"

namespace app_list {

AppListSyncableService::AppListSyncableService(Profile* profile)
    : profile_(profile),
      model_(new AppListModel) {
  // For now, use the AppListControllerDelegate associated with the native
  // desktop. TODO(stevenjb): Remove ExtensionAppModelBuilder controller
  // dependency and move the dependent methods from AppListControllerDelegate
  // to an extension service delegate associated with this class.
  AppListControllerDelegate* controller = NULL;
  AppListService* service =
      AppListService::Get(chrome::HOST_DESKTOP_TYPE_NATIVE);
  if (service)
    controller = service->GetControllerDelegate();
  apps_builder_.reset(
      new ExtensionAppModelBuilder(profile_, model_.get(), controller));
  DCHECK(profile_);
  VLOG(1) << "AppListSyncableService Created.";
}

AppListSyncableService::~AppListSyncableService() {
}

}  // namespace app_list
