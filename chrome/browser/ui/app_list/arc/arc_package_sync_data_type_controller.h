// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PACKAGE_SYNC_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PACKAGE_SYNC_DATA_TYPE_CONTROLLER_H_

#include "base/macros.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/ui_data_type_controller.h"

namespace sync_driver {
class SyncClient;
}

class Profile;

// A UIDataTypeController for arc package sync datatypes, which enables or
// disables these types based on whether ArcAppInstance is ready.
class ArcPackageSyncDataTypeController
    : public sync_driver::UIDataTypeController {
 public:
  ArcPackageSyncDataTypeController(syncer::ModelType type,
                                   const base::Closure& error_callback,
                                   sync_driver::SyncClient* sync_client,
                                   Profile* profile);

  bool ReadyForStart() const override;

 private:
  // DataTypeController is RefCounted.
  ~ArcPackageSyncDataTypeController() override;

  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(ArcPackageSyncDataTypeController);
};
#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PACKAGE_SYNC_DATA_TYPE_CONTROLLER_H_
