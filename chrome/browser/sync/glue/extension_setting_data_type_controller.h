// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sync/driver/non_ui_data_type_controller.h"

class Profile;

namespace syncer {
class SyncClient;
}

namespace browser_sync {

class ExtensionSettingDataTypeController
    : public syncer::NonUIDataTypeController {
 public:
  // |type| is either EXTENSION_SETTINGS or APP_SETTINGS.
  // |dump_stack| is called when an unrecoverable error occurs.
  ExtensionSettingDataTypeController(syncer::ModelType type,
                                     const base::Closure& dump_stack,
                                     syncer::SyncClient* sync_client,
                                     Profile* profile);
  ~ExtensionSettingDataTypeController() override;

  // NonFrontendDataTypeController implementation
  syncer::ModelSafeGroup model_safe_group() const override;

 private:
  // NonFrontendDataTypeController implementation.
  bool PostTaskOnBackendThread(const tracked_objects::Location& from_here,
                               const base::Closure& task) override;
  bool StartModels() override;

  // Only used on the UI thread.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
