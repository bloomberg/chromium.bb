// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_NON_FRONTEND_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_NON_FRONTEND_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_error_handler.h"

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
//    PostTaskOnBackendThread()
//    CreateSyncComponents()
class NonFrontendDataTypeController : public DataTypeController {
 public:
  NonFrontendDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);

  // DataTypeController interface.
  virtual void Start(const StartCallback& start_callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual syncable::ModelType type() const = 0;
  virtual browser_sync::ModelSafeGroup model_safe_group() const = 0;
  virtual std::string name() const OVERRIDE;
  virtual State state() const OVERRIDE;

  // DataTypeErrorHandler interface.
  // Note: this is performed on the datatype's thread.
  virtual void OnUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message) OVERRIDE;
  virtual void OnSingleDatatypeUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;

 protected:
  // For testing only.
  NonFrontendDataTypeController();

  virtual ~NonFrontendDataTypeController();

  // Start any dependent services that need to be running before we can
  // associate models. The default implementation is a no-op.
  // Return value:
  //   True - if models are ready and association can proceed.
  //   False - if models are not ready. StartAssociationAsync should be called
  //           when the models are ready.
  // Note: this is performed on the frontend (UI) thread.
  virtual bool StartModels();

  // Posts the given task to the backend thread, i.e. the thread the
  // datatype lives on.  Return value: True if task posted successfully,
  // false otherwise.
  // NOTE: The StopAssociationAsync() implementation relies on the fact that
  // implementations of this API do not hold any references to the DTC while
  // the task is executing. See http://crbug.com/127706.
  virtual bool PostTaskOnBackendThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task) = 0;

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

  // Implementation of OnUnrecoverableError that lives on UI thread.
  virtual void OnUnrecoverableErrorImpl(
      const tracked_objects::Location& from_here,
      const std::string& message);

  // The actual implementation of Disabling the datatype. This happens
  // on the UI thread.
  virtual void DisableImpl(const tracked_objects::Location& from_here,
                           const std::string& message);

  // Record association time. Called on Datatype's thread.
  virtual void RecordAssociationTime(base::TimeDelta time);
  // Record causes of start failure. Called on UI thread.
  virtual void RecordStartFailure(StartResult result);

  // Post the association task to the thread the datatype lives on.
  // Note: this is performed on the frontend (UI) thread.
  // Return value: True if task posted successfully, False otherwise.
  //
  // TODO(akalin): Callers handle false return values inconsistently;
  // some set the state to NOT_RUNNING, and some set the state to
  // DISABLED.  Move the error handling inside this function to be
  // consistent.
  virtual bool StartAssociationAsync();

  // Accessors and mutators used by derived classes.
  ProfileSyncComponentsFactory* profile_sync_factory() const;
  Profile* profile() const;
  ProfileSyncService* profile_sync_service() const;
  void set_start_callback(const StartCallback& callback);
  void set_state(State state);

  virtual AssociatorInterface* associator() const;
  virtual void set_model_associator(AssociatorInterface* associator);
  virtual ChangeProcessor* change_processor() const;
  virtual void set_change_processor(ChangeProcessor* change_processor);

 private:
  // Build sync components and associate models.
  // Note: this is performed on the datatype's thread.
  void StartAssociation();

  // Helper method to stop associating.
  void StopWhileAssociating();

  // Post the StopAssociation task to the thread the datatype lives on.
  // Note: this is performed on the frontend (UI) thread.
  // Return value: True if task posted successfully, False otherwise.
  bool StopAssociationAsync();

  // Disassociate the models and destroy the sync components.
  // Note: this is performed on the datatype's thread.
  void StopAssociation();

  ProfileSyncComponentsFactory* const profile_sync_factory_;
  Profile* const profile_;
  ProfileSyncService* const profile_sync_service_;

  State state_;

  StartCallback start_callback_;
  scoped_ptr<AssociatorInterface> model_associator_;
  scoped_ptr<ChangeProcessor> change_processor_;

  // Locks/Barriers for aborting association early.
  base::Lock abort_association_lock_;
  bool abort_association_;
  base::WaitableEvent abort_association_complete_;

  // This is added for debugging purpose.
  // TODO(lipalani): Remove this after debugging.
  base::WaitableEvent start_association_called_;

  // This is added for debugging purpose.
  // TODO(lipalani): Remove after debugging.
  bool start_models_failed_;

  DISALLOW_COPY_AND_ASSIGN(NonFrontendDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_NON_FRONTEND_DATA_TYPE_CONTROLLER_H__
