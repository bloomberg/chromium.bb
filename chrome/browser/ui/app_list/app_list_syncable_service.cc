// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_syncable_service.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/extension_app_model_builder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "content/public/browser/notification_source.h"
#include "ui/app_list/app_list_model.h"

namespace app_list {

AppListSyncableService::AppListSyncableService(
    Profile* profile,
    ExtensionService* extension_service)
    : profile_(profile),
      model_(new AppListModel) {
  if (extension_service && extension_service->is_ready()) {
    BuildModel();
    return;
  }

  // The extensions for this profile have not yet all been loaded.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(profile));
}

AppListSyncableService::~AppListSyncableService() {
}

void AppListSyncableService::BuildModel() {
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

void AppListSyncableService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_EXTENSIONS_READY, type);
  DCHECK_EQ(profile_, content::Source<Profile>(source).ptr());
  registrar_.RemoveAll();
  BuildModel();
}

}  // namespace app_list
