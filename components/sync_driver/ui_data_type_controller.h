// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_UI_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_UI_DATA_TYPE_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/shared_change_processor.h"

namespace base {
class TimeDelta;
}

namespace syncer {
class SyncableService;
class SyncError;
}

namespace sync_driver {

// Implementation for datatypes that reside on the (UI thread). This is the same
// thread we perform initialization on, so we don't have to worry about thread
// safety. The main start/stop funtionality is implemented by default.
// Note: RefCountedThreadSafe by way of DataTypeController.
class UIDataTypeController : public DataTypeController {
 public:
  UIDataTypeController(
      scoped_refptr<base::MessageLoopProxy> ui_thread,
      const base::Closure& error_callback,
      const DisableTypeCallback& disable_callback,
      syncer::ModelType type,
      SyncApiComponentFactory* sync_factory);

  // DataTypeController interface.
  virtual void LoadModels(
      const ModelLoadCallback& model_load_callback) OVERRIDE;
  virtual void StartAssociating(const StartCallback& start_callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual syncer::ModelType type() const OVERRIDE;
  virtual syncer::ModelSafeGroup model_safe_group() const OVERRIDE;
  virtual ChangeProcessor* GetChangeProcessor() const OVERRIDE;
  virtual std::string name() const OVERRIDE;
  virtual State state() const OVERRIDE;

  // DataTypeErrorHandler interface.
  virtual void OnSingleDatatypeUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;

  // Used by tests to override the factory used to create
  // GenericChangeProcessors.
  void SetGenericChangeProcessorFactoryForTest(
      scoped_ptr<GenericChangeProcessorFactory> factory);

 protected:
  // For testing only.
  UIDataTypeController();
  // DataTypeController is RefCounted.
  virtual ~UIDataTypeController();

  // Start any dependent services that need to be running before we can
  // associate models. The default implementation is a no-op.
  // Return value:
  //   True - if models are ready and association can proceed.
  //   False - if models are not ready. OnModelLoaded() should be called when
  //           the models are ready.
  virtual bool StartModels();

  // Perform any DataType controller specific state cleanup before stopping
  // the datatype controller. The default implementation is a no-op.
  virtual void StopModels();

  // DataTypeController interface.
  virtual void OnModelLoaded() OVERRIDE;

  // Helper method for cleaning up state and invoking the start callback.
  virtual void StartDone(StartResult result,
                         const syncer::SyncMergeResult& local_merge_result,
                         const syncer::SyncMergeResult& syncer_merge_result);

  // Record association time.
  virtual void RecordAssociationTime(base::TimeDelta time);
  // Record causes of start failure.
  virtual void RecordStartFailure(StartResult result);

  SyncApiComponentFactory* const sync_factory_;

  State state_;

  StartCallback start_callback_;
  ModelLoadCallback model_load_callback_;

  // The sync datatype being controlled.
  syncer::ModelType type_;

  // Sync's interface to the datatype. All sync changes for |type_| are pushed
  // through it to the datatype as well as vice versa.
  //
  // Lifetime: it gets created when Start()) is called, and a reference to it
  // is passed to |local_service_| during Associate(). We release our reference
  // when Stop() or StartFailed() is called, and |local_service_| releases its
  // reference when local_service_->StopSyncing() is called or when it is
  // destroyed.
  //
  // Note: we use refcounting here primarily so that we can keep a uniform
  // SyncableService API, whether the datatype lives on the UI thread or not
  // (a syncer::SyncableService takes ownership of its SyncChangeProcessor when
  // MergeDataAndStartSyncing is called). This will help us eventually merge the
  // two datatype controller implementations (for ui and non-ui thread
  // datatypes).
  scoped_refptr<SharedChangeProcessor> shared_change_processor_;

  scoped_ptr<GenericChangeProcessorFactory> processor_factory_;

  // A weak pointer to the actual local syncable service, which performs all the
  // real work. We do not own the object.
  base::WeakPtr<syncer::SyncableService> local_service_;

  scoped_refptr<base::MessageLoopProxy> ui_thread_;
 private:
   // Associate the sync model with the service's model, then start syncing.
  virtual void Associate();

  virtual void AbortModelLoad();

  DISALLOW_COPY_AND_ASSIGN(UIDataTypeController);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_UI_DATA_TYPE_CONTROLLER_H_
