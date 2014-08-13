// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/compiler_specific.h"
#include "components/sync_driver/non_ui_data_type_controller.h"

class Profile;
class ProfileSyncComponentsFactory;

namespace syncer {
class SyncableService;
}

namespace extensions {
class StorageFrontend;
}

namespace browser_sync {

class ExtensionSettingDataTypeController
    : public sync_driver::NonUIDataTypeController {
 public:
  ExtensionSettingDataTypeController(
      // Either EXTENSION_SETTINGS or APP_SETTINGS.
      syncer::ModelType type,
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile);

  // NonFrontendDataTypeController implementation
  virtual syncer::ModelType type() const OVERRIDE;
  virtual syncer::ModelSafeGroup model_safe_group() const OVERRIDE;

 private:
  virtual ~ExtensionSettingDataTypeController();

  // NonFrontendDataTypeController implementation.
  virtual bool PostTaskOnBackendThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE;
  virtual bool StartModels() OVERRIDE;

  // Either EXTENSION_SETTINGS or APP_SETTINGS.
  syncer::ModelType type_;

  // Only used on the UI thread.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
