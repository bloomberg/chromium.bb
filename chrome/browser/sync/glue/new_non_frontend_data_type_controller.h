// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_NEW_NON_FRONTEND_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_NEW_NON_FRONTEND_DATA_TYPE_CONTROLLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"
#include "chrome/browser/sync/glue/shared_change_processor.h"

class SyncableService;

namespace browser_sync {

class NewNonFrontendDataTypeController : public NonFrontendDataTypeController {
 public:
  NewNonFrontendDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile);
  virtual ~NewNonFrontendDataTypeController();

  virtual void Start(StartCallback* start_callback) OVERRIDE;
  virtual void Stop() OVERRIDE;

 protected:
  NewNonFrontendDataTypeController();

  // Overrides of NonFrontendDataTypeController methods.
  virtual void StartAssociation() OVERRIDE;
  virtual void StartDone(DataTypeController::StartResult result,
                         DataTypeController::State new_state,
                         const SyncError& error) OVERRIDE;
  virtual void StartDoneImpl(DataTypeController::StartResult result,
                             DataTypeController::State new_state,
                             const SyncError& error) OVERRIDE;

  // Calls local_service_->StopSyncing() and releases our references to it and
  // |shared_change_processor_|.
  virtual void StopLocalService();
  // Posts StopLocalService() to the datatype's thread.
  virtual void StopLocalServiceAsync() = 0;

  // Extract/create the syncable service from the profile and return a
  // WeakHandle to it.
  virtual base::WeakPtr<SyncableService> GetWeakPtrToSyncableService()
      const = 0;

 private:
  // Deprecated.
  virtual bool StopAssociationAsync() OVERRIDE;
  virtual void CreateSyncComponents() OVERRIDE;

  // A weak pointer to the actual local syncable service, which performs all the
  // real work. We do not own the object, and it is only safe to access on the
  // DataType's thread.
  // Lifetime: it gets set in StartAssociation() and released in
  // StopLocalService().
  base::WeakPtr<SyncableService> local_service_;

  // The thread-safe layer between the datatype and the syncer. It allows us to
  // disconnect both the datatype and ourselves from the syncer at shutdown
  // time. All accesses to the syncer from any thread other than the UI thread
  // should be through this. Once connected, it must be released on the
  // datatype's thread (but if we never connect it we can destroy it on the UI
  // thread as well).
  // Lifetime: It gets created on the UI thread in Start(..), connected to
  // the syncer in StartAssociation() on the datatype's thread, and if
  // successfully connected released in StopLocalService() on the datatype's
  // thread.
  scoped_refptr<SharedChangeProcessor> shared_change_processor_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_NEW_NON_FRONTEND_DATA_TYPE_CONTROLLER_H_
