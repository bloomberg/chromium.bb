// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_IMPL2_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_IMPL2_H__
#pragma once

#include "chrome/browser/sync/glue/data_type_manager.h"

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/task.h"

namespace browser_sync {

class DataTypeController;
class SyncBackendHost;

class DataTypeManagerImpl2 : public DataTypeManager {
 public:
  DataTypeManagerImpl2(SyncBackendHost* backend,
                       const DataTypeController::TypeMap& controllers);
  virtual ~DataTypeManagerImpl2();

  // DataTypeManager interface.
  virtual void Configure(const TypeSet& desired_types);
  virtual void Stop();
  virtual const DataTypeController::TypeMap& controllers();
  virtual State state();

 private:
  // Starts the next data type in the kStartOrder list, indicated by
  // the current_type_ member.  If there are no more data types to
  // start, the stashed start_callback_ is invoked.
  void StartNextType();

  // Callback passed to each data type controller on startup.
  void TypeStartCallback(DataTypeController::StartResult result);

  // Stops all data types.
  void FinishStop();
  void FinishStopAndNotify(ConfigureResult result);

  void Restart();
  void DownloadReady();
  void NotifyStart();
  void NotifyDone(ConfigureResult result);

  SyncBackendHost* backend_;
  // Map of all data type controllers that are available for sync.
  // This list is determined at startup by various command line flags.
  const DataTypeController::TypeMap controllers_;
  State state_;
  DataTypeController* current_dtc_;
  std::map<syncable::ModelType, int> start_order_;
  TypeSet last_requested_types_;
  std::vector<DataTypeController*> needs_start_;
  std::vector<DataTypeController*> needs_stop_;

  ScopedRunnableMethodFactory<DataTypeManagerImpl2> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataTypeManagerImpl2);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_IMPL2_H__
