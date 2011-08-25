// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_IMPL_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_IMPL_H__
#pragma once

#include "chrome/browser/sync/glue/data_type_manager.h"

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/task.h"
#include "base/time.h"

namespace browser_sync {

class DataTypeController;
class SyncBackendHost;

class DataTypeManagerImpl : public DataTypeManager {
 public:
  DataTypeManagerImpl(SyncBackendHost* backend,
                      const DataTypeController::TypeMap* controllers);
  virtual ~DataTypeManagerImpl();

  // DataTypeManager interface.
  virtual void Configure(const TypeSet& desired_types,
                         sync_api::ConfigureReason reason);

  // Needed only for backend migration.
  virtual void ConfigureWithoutNigori(const TypeSet& desired_types,
                                      sync_api::ConfigureReason reason);

  virtual void Stop();
  virtual State state();

 private:
  // Starts the next data type in the kStartOrder list, indicated by
  // the current_type_ member.  If there are no more data types to
  // start, the stashed start_callback_ is invoked.
  void StartNextType();

  // Callback passed to each data type controller on startup.
  void TypeStartCallback(DataTypeController::StartResult result,
      const tracked_objects::Location& from_here);

  // Stops all data types.
  void FinishStop();
  void Abort(ConfigureStatus status,
             const tracked_objects::Location& location,
             syncable::ModelType last_attempted_type);

  // Returns true if any last_requested_types_ currently needs to start model
  // association.  If non-null, fills |needs_start| with all such controllers.
  bool GetControllersNeedingStart(
      std::vector<DataTypeController*>* needs_start);

  void Restart(sync_api::ConfigureReason reason, bool enable_nigori);
  void DownloadReady(bool success);
  void NotifyStart();
  void NotifyDone(const ConfigureResult& result);
  void SetBlockedAndNotify();

  // Add to |configure_time_delta_| the time since we last called
  // Restart().
  void AddToConfigureTime();

  virtual void ConfigureImpl(const TypeSet& desired_types,
                             sync_api::ConfigureReason reason,
                             bool enable_nigori);

  SyncBackendHost* backend_;
  // Map of all data type controllers that are available for sync.
  // This list is determined at startup by various command line flags.
  const DataTypeController::TypeMap* controllers_;
  State state_;
  std::map<syncable::ModelType, int> start_order_;
  TypeSet last_requested_types_;
  std::vector<DataTypeController*> needs_start_;
  std::vector<DataTypeController*> needs_stop_;

  // Whether an attempt to reconfigure was made while we were busy configuring.
  // The |last_requested_types_| will reflect the newest set of requested types.
  bool needs_reconfigure_;

  // The reason for the last reconfigure attempt. Not this will be set to a
  // valid value only if needs_reconfigure_ is set.
  sync_api::ConfigureReason last_configure_reason_;

  base::WeakPtrFactory<DataTypeManagerImpl> weak_ptr_factory_;

  // The last time Restart() was called.
  base::Time last_restart_time_;

  // The accumulated time spent between calls to Restart() and going
  // to the DONE/BLOCKED state.
  base::TimeDelta configure_time_delta_;

  DISALLOW_COPY_AND_ASSIGN(DataTypeManagerImpl);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_IMPL_H__
