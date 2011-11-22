// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_NON_FRONTEND_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_NON_FRONTEND_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/sync/glue/data_type_controller.h"

class Profile;
class ProfileSyncService;
class ProfileSyncComponentsFactory;
class SyncError;

namespace base { class TimeDelta; }
namespace browser_sync {

class AssociatorInterface;
class ChangeProcessor;

// TODO(zea): Rename frontend to UI (http://crbug.com/78833).
// Implementation for datatypes that do not reside on the frontend thread
// (UI thread). This is the same thread we perform initialization
// on, so we don't have to worry about thread safety. The main start/stop
// funtionality is implemented by default. Derived classes must implement:
//    type()
//    model_safe_group()
//    StartAssociationAsync()
//    CreateSyncComponents()
//    StopAssociationAsync()
//    RecordUnrecoverableError(...)
//    RecordAssociationTime(...);
//    RecordStartFailure(...);
class NonFrontendDataTypeController : public DataTypeController {
 public:
  NonFrontendDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile);
  virtual ~NonFrontendDataTypeController();

  // DataTypeController interface.
  virtual void Start(StartCallback* start_callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual syncable::ModelType type() const = 0;
  virtual browser_sync::ModelSafeGroup model_safe_group() const = 0;
  virtual std::string name() const OVERRIDE;
  virtual State state() const OVERRIDE;

  // UnrecoverableErrorHandler interface.
  // Note: this is performed on the datatype's thread.
  virtual void OnUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message) OVERRIDE;
 protected:
  // For testing only.
  NonFrontendDataTypeController();

  // Start any dependent services that need to be running before we can
  // associate models. The default implementation is a no-op.
  // Return value:
  //   True - if models are ready and association can proceed.
  //   False - if models are not ready. KickOffAssociation should be called
  //           when the models are ready. Refer to Start(_) implementation.
  // Note: this is performed on the frontend (UI) thread.
  virtual bool StartModels();

  // Post the association task to the thread the datatype lives on.
  // Note: this is performed on the frontend (UI) thread.
  // Return value: True if task posted successfully, False otherwise.
  virtual bool StartAssociationAsync() = 0;

  // Build sync components and associate models.
  // Note: this is performed on the datatype's thread.
  virtual void StartAssociation();

  // Datatype specific creation of sync components.
  // Note: this is performed on the datatype's thread.
  virtual void CreateSyncComponents() = 0;

  // Start failed, make sure we record it, clean up state, and invoke the
  // callback on the frontend thread.
  // Note: this is performed on the datatype's thread.
  virtual void StartFailed(StartResult result, const SyncError& error);

  // Start up complete, update the state and invoke the callback.
  // Note: this is performed on the datatype's thread.
  virtual void StartDone(DataTypeController::StartResult result,
                         DataTypeController::State new_state,
                         const SyncError& error);

  // UI thread implementation of StartDone.
  virtual void StartDoneImpl(DataTypeController::StartResult result,
                             DataTypeController::State new_state,
                             const SyncError& error);

  // Perform any DataType controller specific state cleanup before stopping
  // the datatype controller. The default implementation is a no-op.
  // Note: this is performed on the frontend (UI) thread.
  virtual void StopModels();

  // Post the StopAssociation task to the thread the datatype lives on.
  // Note: this is performed on the frontend (UI) thread.
  // Return value: True if task posted successfully, False otherwise.
  virtual bool StopAssociationAsync() = 0;

  // Disassociate the models and destroy the sync components.
  // Note: this is performed on the datatype's thread.
  virtual void StopAssociation();

  // Implementation of OnUnrecoverableError that lives on UI thread.
  virtual void OnUnrecoverableErrorImpl(
      const tracked_objects::Location& from_here,
      const std::string& message);

  // DataType specific histogram methods. Because histograms use static's, the
  // specific datatype controllers must implement this themselves.
  // Important: calling them on other threads can lead to memory corruption!
  // Record unrecoverable errors. Called on Datatype's thread.
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) = 0;
  // Record association time. Called on Datatype's thread.
  virtual void RecordAssociationTime(base::TimeDelta time) = 0;
  // Record causes of start failure. Called on UI thread.
  virtual void RecordStartFailure(StartResult result) = 0;

  // Accessors and mutators used by derived classes.
  ProfileSyncComponentsFactory* profile_sync_factory() const;
  Profile* profile() const;
  ProfileSyncService* profile_sync_service() const;
  void set_start_callback(StartCallback* callback);
  void set_state(State state);

  virtual AssociatorInterface* associator() const;
  virtual void set_model_associator(AssociatorInterface* associator);
  virtual ChangeProcessor* change_processor() const;
  virtual void set_change_processor(ChangeProcessor* change_processor);

 private:
  ProfileSyncComponentsFactory* const profile_sync_factory_;
  Profile* const profile_;
  ProfileSyncService* const profile_sync_service_;

  State state_;

  scoped_ptr<StartCallback> start_callback_;
  scoped_ptr<AssociatorInterface> model_associator_;
  scoped_ptr<ChangeProcessor> change_processor_;

  // Locks/Barriers for aborting association early.
  base::Lock abort_association_lock_;
  bool abort_association_;
  base::WaitableEvent abort_association_complete_;

  // Barrier to ensure that the datatype has been stopped on the DB thread
  // from the UI thread.
  base::WaitableEvent datatype_stopped_;

  DISALLOW_COPY_AND_ASSIGN(NonFrontendDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_NON_FRONTEND_DATA_TYPE_CONTROLLER_H__
