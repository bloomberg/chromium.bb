// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"

class Profile;
class ProfileSyncComponentsFactory;
class ProfileSyncService;
class SyncableService;

namespace extensions {
class SettingsFrontend;
}

namespace browser_sync {

class ExtensionSettingDataTypeController
    : public NonFrontendDataTypeController {
 public:
  ExtensionSettingDataTypeController(
      // Either EXTENSION_SETTINGS or APP_SETTINGS.
      syncable::ModelType type,
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* profile_sync_service);
  virtual ~ExtensionSettingDataTypeController();

  // NonFrontendDataTypeController implementation
  virtual syncable::ModelType type() const OVERRIDE;
  virtual browser_sync::ModelSafeGroup model_safe_group() const OVERRIDE;

 private:
  // NonFrontendDataTypeController implementation.
  virtual bool StartModels() OVERRIDE;
  virtual bool StartAssociationAsync() OVERRIDE;
  virtual void CreateSyncComponents() OVERRIDE;
  virtual bool StopAssociationAsync() OVERRIDE;
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;
  virtual void RecordAssociationTime(base::TimeDelta time) OVERRIDE;
  virtual void RecordStartFailure(StartResult result) OVERRIDE;

  // Starts sync association with |settings_service|.  Callback from
  // RunWithSettings of |extension_settings_ui_wrapper_| on FILE thread.
  void StartAssociationWithExtensionSettingsService(
      SyncableService* settings_service);

  // Either EXTENSION_SETTINGS or APP_SETTINGS.
  syncable::ModelType type_;

  // These only used on the UI thread.
  extensions::SettingsFrontend* settings_frontend_;
  ProfileSyncService* profile_sync_service_;

  // Only used on the FILE thread.
  SyncableService* settings_service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
