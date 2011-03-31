// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_FRONTEND_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_FRONTEND_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/glue/data_type_controller.h"

class Profile;
class ProfileSyncService;
class ProfileSyncFactory;

namespace base { class TimeDelta; }
namespace browser_sync {

class AssociatorInterface;
class ChangeProcessor;

// Implementation for datatypes that reside on the frontend thread
// (UI thread). This is the same thread we perform initialization on, so we
// don't have to worry about thread safety. The main start/stop funtionality is
// implemented by default.
// Derived classes must implement (at least):
//    syncable::ModelType type() const
//    void CreateSyncComponents();
//    void RecordUnrecoverableError(
//        const tracked_objects::Location& from_here,
//        const std::string& message);
//    void RecordAssociationTime(base::TimeDelta time);
//    void RecordStartFailure(StartResult result);
class FrontendDataTypeController : public DataTypeController {
 public:
  FrontendDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);
  virtual ~FrontendDataTypeController();

  // DataTypeController interface.
  virtual void Start(StartCallback* start_callback);

  virtual void Stop();

  virtual syncable::ModelType type() const = 0;

  virtual browser_sync::ModelSafeGroup model_safe_group() const;

  virtual std::string name() const;

  virtual State state() const;

  // UnrecoverableErrorHandler interface.
  virtual void OnUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message);
 protected:
  // For testing only.
  FrontendDataTypeController();

  // Kick off any dependent services that need to be running before we can
  // associate models. The default implementation is a no-op.
  virtual bool StartModels();

  // Build sync components and associate models.
  virtual bool Associate();

  // Perform any DataType controller specific state cleanup before stopping
  // the datatype controller. The default implementation is a no-op.
  virtual void CleanupState();

  // Helper method to run the stashed start callback with a given result.
  virtual void FinishStart(StartResult result,
      const tracked_objects::Location& from_here);

  // Cleans up state and calls callback when start fails.
  virtual void StartFailed(StartResult result,
      const tracked_objects::Location& from_here);

  // Datatype specific creation of sync components.
  virtual void CreateSyncComponents() = 0;

  // DataType specific histogram methods. Because histograms use static's, the
  // specific datatype controllers must implement this themselves.
  // Record unrecoverable errors.
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) = 0;
  // Record association time.
  virtual void RecordAssociationTime(base::TimeDelta time) = 0;
  // Record causes of start failure.
  virtual void RecordStartFailure(StartResult result) = 0;

  ProfileSyncFactory* const profile_sync_factory_;
  Profile* const profile_;
  ProfileSyncService* const sync_service_;

  State state_;

  scoped_ptr<StartCallback> start_callback_;
  scoped_ptr<AssociatorInterface> model_associator_;
  scoped_ptr<ChangeProcessor> change_processor_;

  DISALLOW_COPY_AND_ASSIGN(FrontendDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_FRONTEND_DATA_TYPE_CONTROLLER_H__
