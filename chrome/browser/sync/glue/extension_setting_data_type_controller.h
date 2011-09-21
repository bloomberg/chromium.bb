// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"

class ExtensionSettings;
class ExtensionSettingsUIWrapper;
class Profile;
class ProfileSyncFactory;
class ProfileSyncService;

namespace browser_sync {

class ExtensionSettingDataTypeController
    : public NonFrontendDataTypeController {
 public:
  ExtensionSettingDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
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

  // Starts sync association with |extension_settings|.  Callback from
  // RunWithSettings of |extension_settings_ui_wrapper_| on FILE thread.
  void StartAssociationWithExtensionSettings(
      ExtensionSettings* extension_settings);

  // These only used on the UI thread.
  ExtensionSettingsUIWrapper* extension_settings_ui_wrapper_;
  ProfileSyncService* profile_sync_service_;

  // Only used on the FILE thread.
  ExtensionSettings* extension_settings_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
