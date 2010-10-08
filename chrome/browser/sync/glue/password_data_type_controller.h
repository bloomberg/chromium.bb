// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PASSWORD_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_PASSWORD_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/glue/data_type_controller.h"

class Profile;
class ProfileSyncFactory;
class ProfileSyncService;

namespace browser_sync {

class AssociatorInterface;
class ChangeProcessor;
class ControlTask;

// A class that manages the startup and shutdown of password sync.
class PasswordDataTypeController : public DataTypeController {
 public:
  PasswordDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);
  virtual ~PasswordDataTypeController();

  // DataTypeController implementation
  virtual void Start(StartCallback* start_callback);

  virtual void Stop();

  virtual bool enabled() {
    return true;
  }

  virtual syncable::ModelType type() {
    return syncable::PASSWORDS;
  }

  virtual browser_sync::ModelSafeGroup model_safe_group() {
    return browser_sync::GROUP_PASSWORD;
  }

  virtual const char* name() const {
    // For logging only.
    return "password";
  }

  virtual State state() {
    return state_;
  }

  // UnrecoverableHandler implementation
  virtual void OnUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message);

 private:
  void StartImpl();
  void StartDone(StartResult result, State state);
  void StartDoneImpl(StartResult result, State state);
  void StopImpl();
  void StartFailed(StartResult result);
  void OnUnrecoverableErrorImpl(const tracked_objects::Location& from_here,
                                const std::string& message);

  void set_state(State state) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    state_ = state;
  }

  ProfileSyncFactory* profile_sync_factory_;
  Profile* profile_;
  ProfileSyncService* sync_service_;
  State state_;

  scoped_ptr<AssociatorInterface> model_associator_;
  scoped_ptr<ChangeProcessor> change_processor_;
  scoped_ptr<StartCallback> start_callback_;
  scoped_refptr<PasswordStore> password_store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PASSWORD_DATA_TYPE_CONTROLLER_H__
