// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_TYPED_URL_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_TYPED_URL_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"
#include "content/browser/cancelable_request.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/common/content_notification_types.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class NotificationSource;
class NotificationDetails;
class HistoryService;

namespace history {
class HistoryBackend;
}

namespace browser_sync {

class ControlTask;

// A class that manages the startup and shutdown of typed_url sync.
class TypedUrlDataTypeController : public NonFrontendDataTypeController,
                                   public NotificationObserver {
 public:
  TypedUrlDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
      Profile* profile);
  virtual ~TypedUrlDataTypeController();

  // NonFrontendDataTypeController implementation
  virtual syncable::ModelType type() const OVERRIDE;
  virtual browser_sync::ModelSafeGroup model_safe_group() const OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

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
  virtual bool StartAssociationAsync() OVERRIDE;
  virtual void CreateSyncComponents() OVERRIDE;
  virtual void StopModels() OVERRIDE;
  virtual bool StopAssociationAsync() OVERRIDE;
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;
  virtual void RecordAssociationTime(base::TimeDelta time) OVERRIDE;
  virtual void RecordStartFailure(StartResult result) OVERRIDE;

 private:
  friend class ControlTask;

  // Helper method to launch Associate() and Destroy() on the history thread.
  void RunOnHistoryThread(bool start, history::HistoryBackend* backend);

  history::HistoryBackend* backend_;
  scoped_refptr<HistoryService> history_service_;
  NotificationRegistrar notification_registrar_;
  PrefChangeRegistrar pref_registrar_;

  // Helper object to make sure we don't leave tasks running on the history
  // thread.
  CancelableRequestConsumerT<int, 0> cancelable_consumer_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_TYPED_URL_DATA_TYPE_CONTROLLER_H__
