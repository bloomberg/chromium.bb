// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_FRONTEND_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_FRONTEND_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/data_type_error_handler.h"

class Profile;
class ProfileSyncService;
class ProfileSyncComponentsFactory;

namespace base {
class TimeDelta;
}

namespace syncer {
class SyncError;
}

namespace sync_driver {
class AssociatorInterface;
class ChangeProcessor;
}

namespace browser_sync {

// Implementation for datatypes that reside on the frontend thread
// (UI thread). This is the same thread we perform initialization on, so we
// don't have to worry about thread safety. The main start/stop funtionality is
// implemented by default.
// Derived classes must implement (at least):
//    syncer::ModelType type() const
//    void CreateSyncComponents();
// NOTE: This class is deprecated! New sync datatypes should be using the
// syncer::SyncableService API and the UIDataTypeController instead.
// TODO(zea): Delete this once all types are on the new API.
class FrontendDataTypeController : public sync_driver::DataTypeController {
 public:
  FrontendDataTypeController(
      scoped_refptr<base::MessageLoopProxy> ui_thread,
      const base::Closure& error_callback,
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);

  // DataTypeController interface.
  virtual void LoadModels(
      const ModelLoadCallback& model_load_callback) OVERRIDE;
  virtual void StartAssociating(const StartCallback& start_callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual syncer::ModelType type() const = 0;
  virtual syncer::ModelSafeGroup model_safe_group() const OVERRIDE;
  virtual std::string name() const OVERRIDE;
  virtual State state() const OVERRIDE;

  // DataTypeErrorHandler interface.
  virtual void OnSingleDataTypeUnrecoverableError(
      const syncer::SyncError& error) OVERRIDE;

 protected:
  friend class FrontendDataTypeControllerMock;

  // For testing only.
  FrontendDataTypeController();
  virtual ~FrontendDataTypeController();

  // Kick off any dependent services that need to be running before we can
  // associate models. The default implementation is a no-op.
  // Return value:
  //   True - if models are ready and association can proceed.
  //   False - if models are not ready. Associate() should be called when the
  //           models are ready. Refer to Start(_) implementation.
  virtual bool StartModels();

  // Datatype specific creation of sync components.
  virtual void CreateSyncComponents() = 0;

  // DataTypeController interface.
  virtual void OnModelLoaded() OVERRIDE;

  // Perform any DataType controller specific state cleanup before stopping
  // the datatype controller. The default implementation is a no-op.
  virtual void CleanUpState();

  // Helper method for cleaning up state and running the start callback.
  virtual void StartDone(
      ConfigureResult start_result,
      const syncer::SyncMergeResult& local_merge_result,
      const syncer::SyncMergeResult& syncer_merge_result);

  // Record association time.
  virtual void RecordAssociationTime(base::TimeDelta time);
  // Record causes of start failure.
  virtual void RecordStartFailure(ConfigureResult result);

  virtual sync_driver::AssociatorInterface* model_associator() const;
  virtual void set_model_associator(
      sync_driver::AssociatorInterface* associator);
  virtual sync_driver::ChangeProcessor* GetChangeProcessor() const OVERRIDE;
  virtual void set_change_processor(sync_driver::ChangeProcessor* processor);

  // Handles the reporting of unrecoverable error. It records stuff in
  // UMA and reports to breakpad.
  // Virtual for testing purpose.
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message);

  ProfileSyncComponentsFactory* const profile_sync_factory_;
  Profile* const profile_;
  ProfileSyncService* const sync_service_;

  State state_;

  StartCallback start_callback_;
  ModelLoadCallback model_load_callback_;

  // TODO(sync): transition all datatypes to SyncableService and deprecate
  // AssociatorInterface.
  scoped_ptr<sync_driver::AssociatorInterface> model_associator_;
  scoped_ptr<sync_driver::ChangeProcessor> change_processor_;

 private:
  // Build sync components and associate models.
  // Return value:
  //   True - if association was successful. FinishStart should have been
  //          invoked.
  //   False - if association failed. StartFailed should have been invoked.
  virtual bool Associate();

  void AbortModelLoad();

  // Clean up our state and state variables. Called in response
  // to a failure or abort or stop.
  void CleanUp();

  DISALLOW_COPY_AND_ASSIGN(FrontendDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_FRONTEND_DATA_TYPE_CONTROLLER_H__
