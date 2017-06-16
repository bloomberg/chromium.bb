// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/stats.h"

#include "components/download/internal/startup_status.h"

namespace download {
namespace stats {

void LogControllerStartupStatus(const StartupStatus& status) {
  DCHECK(status.Complete());

  // TODO(dtrainor): Log each failure reason.
  // TODO(dtrainor): Finally, log SUCCESS or FAILURE based on status.Ok().
}

void LogServiceApiAction(DownloadClient client, ServiceApiAction action) {
  // TODO(dtrainor): Log |action| for |client|.
}

void LogStartDownloadResult(DownloadClient client,
                            DownloadParams::StartResult result) {
  // TODO(dtrainor): Log |result| for |client|.
}

void LogModelOperationResult(ModelAction action, bool success) {
  // TODO(dtrainor): Log |action|.
}

void LogScheduledTaskStatus(DownloadTaskType task_type,
                            ScheduledTaskStatus status) {
  // TODO(shaktisahu): Log |task_type| and |status|.
}

void LogDownloadCompletion(CompletionType type) {
  // TODO(xingliu): Log completion.
}

void LogRecoveryOperation(Entry::State to_state) {
  // TODO(dtrainor): Log |to_state|.
}

}  // namespace stats
}  // namespace download
