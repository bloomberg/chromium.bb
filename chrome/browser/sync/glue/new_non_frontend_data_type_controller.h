// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_NEW_NON_FRONTEND_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_NEW_NON_FRONTEND_DATA_TYPE_CONTROLLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"
#include "chrome/browser/sync/glue/shared_change_processor.h"

namespace {
class SyncableService;
}

namespace browser_sync {

class NewNonFrontendDataTypeController : public NonFrontendDataTypeController {
 public:
  NewNonFrontendDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);

  // DataTypeController interface.
  virtual void LoadModels(
      const ModelLoadCallback& model_load_callback) OVERRIDE;
  virtual void StartAssociating(const StartCallback& start_callback) OVERRIDE;

  virtual void Stop() OVERRIDE;

 protected:
  friend class NewNonFrontendDataTypeControllerMock;

  NewNonFrontendDataTypeController();
  virtual ~NewNonFrontendDataTypeController();

  // DataTypeController interface.
  virtual void OnModelLoaded() OVERRIDE;

  // Overrides of NonFrontendDataTypeController methods.
  virtual void StartDone(DataTypeController::StartResult result,
                         DataTypeController::State new_state,
                         const csync::SyncError& error) OVERRIDE;
  virtual void StartDoneImpl(DataTypeController::StartResult result,
                             DataTypeController::State new_state,
                             const csync::SyncError& error) OVERRIDE;

 private:
  // This overrides the same method in |NonFrontendDataTypeController|.
  virtual bool StartAssociationAsync() OVERRIDE;

  // Posted on the backend thread by StartAssociationAsync().
  void StartAssociationWithSharedChangeProcessor(
      const scoped_refptr<SharedChangeProcessor>& shared_change_processor);

  // Calls Disconnect() on |shared_change_processor_|, then sets it to
  // NULL.  Must be called only by StartDoneImpl() or Stop() (on the
  // UI thread) and only after a call to Start() (i.e.,
  // |shared_change_processor_| must be non-NULL).
  void ClearSharedChangeProcessor();

  // Posts StopLocalService() to the datatype's thread.
  void StopLocalServiceAsync();

  // Calls local_service_->StopSyncing() and releases our references to it.
  void StopLocalService();

  void AbortModelStarting();

  // Deprecated.
  virtual void CreateSyncComponents() OVERRIDE;

  // The shared change processor is the thread-safe interface to the
  // datatype.  We hold a reference to it from the UI thread so that
  // we can call Disconnect() on it from Stop()/StartDoneImpl().  Most
  // of the work is done on the backend thread, and in
  // StartAssociationWithSharedChangeProcessor() for this class in
  // particular.
  //
  // Lifetime: The SharedChangeProcessor object is created on the UI
  // thread and passed on to the backend thread.  This reference is
  // released on the UI thread in Stop()/StartDoneImpl(), but the
  // backend thread may still have references to it (which is okay,
  // since we call Disconnect() before releasing the UI thread
  // reference).
  scoped_refptr<SharedChangeProcessor> shared_change_processor_;

  // A weak pointer to the actual local syncable service, which performs all the
  // real work. We do not own the object, and it is only safe to access on the
  // DataType's thread.
  // Lifetime: it gets set in StartAssociation() and released in
  // StopLocalService().
  base::WeakPtr<csync::SyncableService> local_service_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_NEW_NON_FRONTEND_DATA_TYPE_CONTROLLER_H_
