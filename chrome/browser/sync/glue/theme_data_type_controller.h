// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_THEME_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_THEME_DATA_TYPE_CONTROLLER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/frontend_data_type_controller.h"

namespace browser_sync {

class ThemeDataTypeController : public FrontendDataTypeController {
 public:
  ThemeDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);
  virtual ~ThemeDataTypeController();

  // DataTypeController implementation.
  virtual syncable::ModelType type() const OVERRIDE;

 private:
  // DataTypeController implementations.
  virtual bool StartModels() OVERRIDE;
  virtual void CreateSyncComponents() OVERRIDE;
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;
  virtual void RecordAssociationTime(base::TimeDelta time) OVERRIDE;
  virtual void RecordStartFailure(StartResult result) OVERRIDE;
  DISALLOW_COPY_AND_ASSIGN(ThemeDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_THEME_DATA_TYPE_CONTROLLER_H_
