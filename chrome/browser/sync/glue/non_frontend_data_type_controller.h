// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_NON_FRONTEND_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_NON_FRONTEND_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
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

// Implementation for datatypes that do not reside on the frontend thread
// (UI thread). This is the same thread we perform initialization
// on, so we don't have to worry about thread safety. The main start/stop
// functionality is implemented by default. Derived classes must implement:
//    type()
//    model_safe_group()
//    PostTaskOnBackendThread()
//    CreateSyncComponents()
class NonFrontendDataTypeController : public sync_driver::DataTypeController {
 public:
  // For creating non-frontend processor/associator and associating on backend.
  class BackendComponentsContainer;

  NonFrontendDataTypeController(
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
  virtual syncer::ModelSafeGroup model_safe_group() const = 0;
  virtual std::string name() const OVERRIDE;
  virtual State state() const OVERRIDE;

  // DataTypeErrorHandler interface.
  // Note: this is performed on the datatype's thread.
  virtual void OnSingleDatatypeUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;

  // Callback to receive background association results.
  struct AssociationResult {
    explicit AssociationResult(syncer::ModelType type);
    ~AssociationResult();
    bool needs_crypto;
    bool unrecoverable_error;
    bool sync_has_nodes;
    syncer::SyncError error;
    syncer::SyncMergeResult local_merge_result;
    syncer::SyncMergeResult syncer_merge_result;
    base::TimeDelta association_time;
    sync_driver::ChangeProcessor* change_processor;
    sync_driver::AssociatorInterface* model_associator;
  };
  void AssociationCallback(AssociationResult result);

 protected:
  // For testing only.
  NonFrontendDataTypeController();

  virtual ~NonFrontendDataTypeController();

  // DataTypeController interface.
  virtual void OnModelLoaded() OVERRIDE;

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

  // Returns true if the current thread is the backend thread, i.e. the same
  // thread used by |PostTaskOnBackendThread|. The default implementation just
  // checks that the current thread is not the UI thread, but subclasses should
  // override it appropriately.
  virtual bool IsOnBackendThread();

  // Datatype specific creation of sync components.
  // Note: this is performed on the datatype's thread.
  virtual ProfileSyncComponentsFactory::SyncComponents
      CreateSyncComponents() = 0;

  // Called on UI thread during shutdown to effectively disable processing
  // any changes.
  virtual void DisconnectProcessor(sync_driver::ChangeProcessor* processor) = 0;

  // Start up complete, update the state and invoke the callback.
  // Note: this is performed on the datatype's thread.
  virtual void StartDone(
      DataTypeController::StartResult start_result,
      const syncer::SyncMergeResult& local_merge_result,
      const syncer::SyncMergeResult& syncer_merge_result);

  // UI thread implementation of StartDone.
  virtual void StartDoneImpl(
      DataTypeController::StartResult start_result,
      DataTypeController::State new_state,
      const syncer::SyncMergeResult& local_merge_result,
      const syncer::SyncMergeResult& syncer_merge_result);

  // The actual implementation of Disabling the datatype. This happens
  // on the UI thread.
  virtual void DisableImpl(const tracked_objects::Location& from_here,
                           const std::string& message);

  // Record association time. Called on Datatype's thread.
  virtual void RecordAssociationTime(base::TimeDelta time);
  // Record causes of start failure. Called on UI thread.
  virtual void RecordStartFailure(StartResult result);

  // Handles the reporting of unrecoverable error. It records stuff in
  // UMA and reports to breakpad.
  // Virtual for testing purpose.
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message);

  // Accessors and mutators used by derived classes.
  ProfileSyncComponentsFactory* profile_sync_factory() const;
  Profile* profile() const;
  ProfileSyncService* profile_sync_service() const;
  void set_start_callback(const StartCallback& callback);
  void set_state(State state);

  virtual sync_driver::AssociatorInterface* associator() const;
  virtual sync_driver::ChangeProcessor* GetChangeProcessor() const OVERRIDE;

  State state_;
  StartCallback start_callback_;
  ModelLoadCallback model_load_callback_;

 private:
  friend class BackendComponentsContainer;
  ProfileSyncComponentsFactory* const profile_sync_factory_;
  Profile* const profile_;
  ProfileSyncService* const profile_sync_service_;

  // Created on UI thread and passed to backend to create processor/associator
  // and associate model. Released on backend.
  scoped_ptr<BackendComponentsContainer> components_container_;

  sync_driver::AssociatorInterface* model_associator_;
  sync_driver::ChangeProcessor* change_processor_;

  base::WeakPtrFactory<NonFrontendDataTypeController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NonFrontendDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_NON_FRONTEND_DATA_TYPE_CONTROLLER_H__
