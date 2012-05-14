// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_TYPED_URL_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_TYPED_URL_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"

class HistoryService;

namespace history {
class HistoryBackend;
}

namespace browser_sync {

class ControlTask;

// A class that manages the startup and shutdown of typed_url sync.
class TypedUrlDataTypeController : public NonFrontendDataTypeController,
                                   public content::NotificationObserver {
 public:
  TypedUrlDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);

  // NonFrontendDataTypeController implementation
  virtual syncable::ModelType type() const OVERRIDE;
  virtual browser_sync::ModelSafeGroup model_safe_group() const OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // CancelableRequestConsumerBase implementation.
  virtual void OnRequestAdded(CancelableRequestProvider* provider,
                              CancelableRequestProvider::Handle handle) {}
  virtual void OnRequestRemoved(CancelableRequestProvider* provider,
                                CancelableRequestProvider::Handle handle) {}
  virtual void WillExecute(CancelableRequestProvider* provider,
                           CancelableRequestProvider::Handle handle) {}
  virtual void DidExecute(CancelableRequestProvider* provider,
                          CancelableRequestProvider::Handle handle) {}

 protected:
  // NonFrontendDataTypeController interface.
  virtual bool PostTaskOnBackendThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE;
  virtual void CreateSyncComponents() OVERRIDE;
  virtual void StopModels() OVERRIDE;

 private:
  virtual ~TypedUrlDataTypeController();

  // Used by PostTaskOnBackendThread().
  void RunTaskOnBackendThread(const base::Closure& task,
                              history::HistoryBackend* backend);

  history::HistoryBackend* backend_;
  scoped_refptr<HistoryService> history_service_;
  content::NotificationRegistrar notification_registrar_;
  PrefChangeRegistrar pref_registrar_;

  // Helper object to make sure we don't leave tasks running on the history
  // thread.
  CancelableRequestConsumerT<int, 0> cancelable_consumer_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_TYPED_URL_DATA_TYPE_CONTROLLER_H__
