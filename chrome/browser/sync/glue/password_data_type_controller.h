// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PASSWORD_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_PASSWORD_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"

class PasswordStore;

namespace browser_sync {

// A class that manages the startup and shutdown of password sync.
class PasswordDataTypeController : public NonFrontendDataTypeController {
 public:
  PasswordDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile);
  virtual ~PasswordDataTypeController();

  // NonFrontendDataTypeController implementation
  virtual syncable::ModelType type() const OVERRIDE;
  virtual browser_sync::ModelSafeGroup model_safe_group() const OVERRIDE;

 protected:
  // NonFrontendDataTypeController interface.
  virtual bool StartModels() OVERRIDE;
  virtual bool StartAssociationAsync() OVERRIDE;
  virtual void CreateSyncComponents() OVERRIDE;
  virtual bool StopAssociationAsync() OVERRIDE;
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;
  virtual void RecordAssociationTime(base::TimeDelta time) OVERRIDE;
  virtual void RecordStartFailure(StartResult result) OVERRIDE;

 private:
  scoped_refptr<PasswordStore> password_store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PASSWORD_DATA_TYPE_CONTROLLER_H__
