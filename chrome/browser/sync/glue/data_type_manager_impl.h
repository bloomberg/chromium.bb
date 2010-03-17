// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_IMPL_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_IMPL_H__

#include "chrome/browser/sync/glue/data_type_manager.h"

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"

class NotificationSource;
class NotificationDetails;

namespace browser_sync {

class SyncBackendHost;

class DataTypeManagerImpl : public DataTypeManager,
                            public NotificationObserver {
 public:
  DataTypeManagerImpl(SyncBackendHost* backend,
                      const DataTypeController::TypeMap& controllers);
  virtual ~DataTypeManagerImpl() {}

  // DataTypeManager interface.
  virtual void Start(StartCallback* start_callback);

  virtual void Stop();

  virtual bool IsRegistered(syncable::ModelType type);

  virtual bool IsEnabled(syncable::ModelType type);

  virtual const DataTypeController::TypeMap& controllers() {
    return controllers_;
  };

  virtual State state() {
    return state_;
  }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Starts the next data type in the kStartOrder list, indicated by
  // the current_type_ member.  If there are no more data types to
  // start, the stashed start_callback_ is invoked.
  void StartNextType();

  // Callback passed to each data type controller on startup.
  void TypeStartCallback(DataTypeController::StartResult result);

  // Stops all data types.
  void FinishStop();

  void AddObserver(NotificationType type);
  void RemoveObserver(NotificationType type);

  SyncBackendHost* backend_;
  DataTypeController::TypeMap controllers_;
  State state_;
  int current_type_;

  scoped_ptr<StartCallback> start_callback_;
  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(DataTypeManagerImpl);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_IMPL_H__
