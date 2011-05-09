// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PREFERENCE_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_PREFERENCE_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/frontend_data_type_controller.h"

namespace browser_sync {

class PreferenceDataTypeController : public FrontendDataTypeController {
 public:
  PreferenceDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);
  virtual ~PreferenceDataTypeController();

  // FrontendDataTypeController implementation.
  virtual syncable::ModelType type() const OVERRIDE;
  virtual AssociatorInterface* model_associator() const OVERRIDE;
  virtual void set_model_associator(AssociatorInterface* associator) OVERRIDE;

 private:
  // FrontendDataTypeController implementations.
  // TODO(zea): Remove this once everything uses the NewAssociatorInterface.
  virtual void CreateSyncComponents() OVERRIDE;
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;
  virtual void RecordAssociationTime(base::TimeDelta time) OVERRIDE;
  virtual void RecordStartFailure(StartResult result) OVERRIDE;

  // Owned by pref service.
  // TODO(zea): Make this a SyncableService once AssociatorInterface is
  // deprecated.
  AssociatorInterface* pref_sync_service_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PREFERENCE_DATA_TYPE_CONTROLLER_H__
