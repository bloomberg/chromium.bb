// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_TYPED_URL_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_TYPED_URL_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"

class HistoryService;

namespace history {
class HistoryBackend;
}

namespace browser_sync {

class ControlTask;

// A class that manages the startup and shutdown of typed_url sync.
class TypedUrlDataTypeController : public NonFrontendDataTypeController {
 public:
  TypedUrlDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);

  // NonFrontendDataTypeController implementation
  virtual syncer::ModelType type() const OVERRIDE;
  virtual syncer::ModelSafeGroup model_safe_group() const OVERRIDE;

  // Invoked on the history thread to set our history backend - must be called
  // before CreateSyncComponents() is invoked.
  void SetBackend(history::HistoryBackend* backend);

 protected:
  // NonFrontendDataTypeController interface.
  virtual bool PostTaskOnBackendThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE;
  virtual void CreateSyncComponents() OVERRIDE;
  virtual void StopModels() OVERRIDE;

 private:
  virtual ~TypedUrlDataTypeController();

  void OnSavingBrowserHistoryDisabledChanged();

  history::HistoryBackend* backend_;
  PrefChangeRegistrar pref_registrar_;

  // Helper object to make sure we don't leave tasks running on the history
  // thread.
  CancelableRequestConsumerT<int, 0> cancelable_consumer_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_TYPED_URL_DATA_TYPE_CONTROLLER_H__
