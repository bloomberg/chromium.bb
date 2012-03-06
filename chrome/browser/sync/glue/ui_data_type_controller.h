// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_UI_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_UI_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/glue/data_type_controller.h"

class Profile;
class ProfileSyncService;
class ProfileSyncComponentsFactory;
class SyncableService;
class SyncError;

namespace base { class TimeDelta; }
namespace browser_sync {

// Implementation for datatypes that reside on the (UI thread). This is the same
// thread we perform initialization on, so we don't have to worry about thread
// safety. The main start/stop funtionality is implemented by default.
// Note: RefCountedThreadSafe by way of DataTypeController.
class UIDataTypeController : public DataTypeController {
 public:
  UIDataTypeController(
      syncable::ModelType type,
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);

  // DataTypeController interface.
  virtual void Start(const StartCallback& start_callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual syncable::ModelType type() const OVERRIDE;
  virtual browser_sync::ModelSafeGroup model_safe_group() const OVERRIDE;
  virtual std::string name() const OVERRIDE;
  virtual State state() const OVERRIDE;

  // UnrecoverableErrorHandler interface.
  virtual void OnUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message) OVERRIDE;
  virtual void OnSingleDatatypeUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;

 protected:
  // For testing only.
  UIDataTypeController();
  // DataTypeController is RefCounted.
  virtual ~UIDataTypeController();

  // Start any dependent services that need to be running before we can
  // associate models. The default implementation is a no-op.
  // Return value:
  //   True - if models are ready and association can proceed.
  //   False - if models are not ready. Associate() should be called when the
  //           models are ready.
  virtual bool StartModels();

  // Associate the sync model with the service's model, then start syncing.
  virtual void Associate();

  // Perform any DataType controller specific state cleanup before stopping
  // the datatype controller. The default implementation is a no-op.
  virtual void StopModels();

  // Helper methods for cleaning up state and invoking the start callback.
  virtual void StartFailed(StartResult result, const SyncError& error);
  virtual void StartDone(StartResult result);

  // Datatype specific histogram methods.
  // Record unrecoverable errors.
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message);
  // Record association time.
  virtual void RecordAssociationTime(base::TimeDelta time);
  // Record causes of start failure.
  virtual void RecordStartFailure(StartResult result);

  ProfileSyncComponentsFactory* const profile_sync_factory_;
  Profile* const profile_;
  ProfileSyncService* const sync_service_;

  State state_;

  StartCallback start_callback_;

  // The sync datatype being controlled.
  syncable::ModelType type_;

  // A weak pointer to the actual local syncable service, which performs all the
  // real work. We do not own the object.
  base::WeakPtr<SyncableService> local_service_;

  DISALLOW_COPY_AND_ASSIGN(UIDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_UI_DATA_TYPE_CONTROLLER_H__
