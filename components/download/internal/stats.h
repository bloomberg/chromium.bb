// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_STATS_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_STATS_H_

#include "components/download/internal/controller.h"
#include "components/download/internal/entry.h"
#include "components/download/public/clients.h"
#include "components/download/public/download_params.h"
#include "components/download/public/download_task_types.h"

namespace download {

struct StartupStatus;

namespace stats {

// Please follow the following rules for all enums:
// 1. Keep them in sync with the corresponding entry in enums.xml.
// 2. Treat them as append only.
// 3. Do not remove any enums.  Only mark them as deprecated.

// Enum used by UMA metrics to track which actions a Client is taking on the
// service.
enum class ServiceApiAction {
  // Represents a call to DownloadService::StartDownload.
  START_DOWNLOAD = 0,

  // Represents a call to DownloadService::PauseDownload.
  PAUSE_DOWNLOAD = 1,

  // Represents a call to DownloadService::ResumeDownload.
  RESUME_DOWNLOAD = 2,

  // Represents a call to DownloadService::CancelDownload.
  CANCEL_DOWNLOAD = 3,

  // Represents a call to DownloadService::ChangeCriteria.
  CHANGE_CRITERIA = 4,

  // The last entry for the enum.
  COUNT = 5,
};

// Enum used by UMA metrics to tie to specific actions taken on a Model.  This
// can be used to track failure events.
enum class ModelAction {
  // Represents an attempt to initialize the Model.
  INITIALIZE = 0,

  // Represents an attempt to add an Entry to the Model.
  ADD = 1,

  // Represents an attempt to update an Entry in the Model.
  UPDATE = 2,

  // Represents an attempt to remove an Entry from the Model.
  REMOVE = 3,

  // The last entry for the enum.
  COUNT = 4,
};

// Enum used by UMA metrics to log the status of scheduled tasks.
enum class ScheduledTaskStatus {
  // Startup failed and the task was not run.
  ABORTED_ON_FAILED_INIT = 0,

  // OnStopScheduledTask() was received before the task could be fired.
  CANCELLED_ON_STOP = 1,

  // Callback was run successfully after completion of the task.
  COMPLETED_NORMALLY = 2,
};

// Logs the results of starting up the Controller.  Will log each failure reason
// if |status| contains more than one initialization failure.
void LogControllerStartupStatus(const StartupStatus& status);

// Logs an action taken on the service API.
void LogServiceApiAction(DownloadClient client, ServiceApiAction action);

// Logs the result of a StartDownload() attempt on the service.
void LogStartDownloadResult(DownloadClient client,
                            DownloadParams::StartResult result);

// Logs statistics about the result of a Model operation.  Used to track failure
// cases.
void LogModelOperationResult(ModelAction action, bool success);

// Log statistics about the status of a TaskFinishedCallback.
void LogScheduledTaskStatus(DownloadTaskType task_type,
                            ScheduledTaskStatus status);

// Logs download completion event.
void LogDownloadCompletion(CompletionType type);

// Logs recovery operations that happened when we had to move from one state
// to another on startup.
void LogRecoveryOperation(Entry::State to_state);

}  // namespace stats
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_STATS_H_
