// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/new_non_frontend_data_type_controller.h"

class Profile;
class ProfileSyncComponentsFactory;
class ProfileSyncService;

namespace csync {
class SyncableService;
}

namespace extensions {
class SettingsFrontend;
}

namespace browser_sync {

class ExtensionSettingDataTypeController
    : public NewNonFrontendDataTypeController {
 public:
  ExtensionSettingDataTypeController(
      // Either EXTENSION_SETTINGS or APP_SETTINGS.
      syncable::ModelType type,
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* profile_sync_service);

  // NonFrontendDataTypeController implementation
  virtual syncable::ModelType type() const OVERRIDE;
  virtual csync::ModelSafeGroup model_safe_group() const OVERRIDE;

 private:
  virtual ~ExtensionSettingDataTypeController();

  // NonFrontendDataTypeController implementation.
  virtual bool PostTaskOnBackendThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE;
  virtual bool StartModels() OVERRIDE;

  // Either EXTENSION_SETTINGS or APP_SETTINGS.
  syncable::ModelType type_;

  // These only used on the UI thread.
  Profile* profile_;
  ProfileSyncService* profile_sync_service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
