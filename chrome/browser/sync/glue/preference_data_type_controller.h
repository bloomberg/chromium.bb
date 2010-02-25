// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PREFERENCE_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_PREFERENCE_DATA_TYPE_CONTROLLER_H__

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/glue/data_type_controller.h"

class ProfileSyncFactory;
class ProfileSyncService;

namespace browser_sync {

class AssociatorInterface;
class ChangeProcessor;

class PreferenceDataTypeController : public DataTypeController {
 public:
  PreferenceDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
      ProfileSyncService* sync_service);
  virtual ~PreferenceDataTypeController();

  virtual void Start(bool merge_allowed, StartCallback* start_callback);

  virtual void Stop();

  virtual bool enabled() {
    return true;
  }

  virtual syncable::ModelType type() {
    return syncable::PREFERENCES;
  }

  virtual browser_sync::ModelSafeGroup model_safe_group() {
    return browser_sync::GROUP_UI;
  }

  virtual State state() {
    return state_;
  }

 private:
  ProfileSyncFactory* profile_sync_factory_;
  ProfileSyncService* sync_service_;

  State state_;

  scoped_ptr<AssociatorInterface> model_associator_;
  scoped_ptr<ChangeProcessor> change_processor_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PREFERENCE_DATA_TYPE_CONTROLLER_H__
