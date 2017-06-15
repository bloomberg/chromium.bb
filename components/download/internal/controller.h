// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_CONTROLLER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "components/download/public/clients.h"
#include "components/download/public/download_service.h"
#include "components/download/public/download_task_types.h"

namespace download {

struct DownloadParams;
struct SchedulingParams;
struct StartupStatus;

// The type of completion when the download entry transits to complete state.
// TODO(xingliu): Implement timeout and unknown failure types.
enum class CompletionType {
  // The download is successfully finished.
  SUCCEED = 0,
  // The download is interrupted and failed.
  FAIL = 1,
  // The download is aborted by the client.
  ABORT = 2,
  // The download is timed out and the connection is closed.
  TIMEOUT = 3,
  // The download is failed for unknown reasons.
  UNKNOWN = 4,
  // The download is cancelled by the client.
  CANCEL = 5,
};

// The core Controller responsible for gluing various DownloadService components
// together to manage the active downloads.
class Controller {
 public:
  Controller() = default;
  virtual ~Controller() = default;

  // Initializes the controller.  Initialization may be asynchronous.
  virtual void Initialize() = 0;

  // Returns the status of Controller.
  virtual const StartupStatus* GetStartupStatus() = 0;

  // Starts a download with |params|.  See DownloadParams::StartCallback and
  // DownloadParams::StartResponse for information on how a caller can determine
  // whether or not the download was successfully accepted and queued.
  virtual void StartDownload(const DownloadParams& params) = 0;

  // Pauses a download request associated with |guid| if one exists.
  virtual void PauseDownload(const std::string& guid) = 0;

  // Resumes a download request associated with |guid| if one exists.  The
  // download request may or may not start downloading at this time, but it will
  // no longer be blocked by any prior PauseDownload() actions.
  virtual void ResumeDownload(const std::string& guid) = 0;

  // Cancels a download request associated with |guid| if one exists.
  virtual void CancelDownload(const std::string& guid) = 0;

  // Changes the SchedulingParams of a download request associated with |guid|
  // to |params|.
  virtual void ChangeDownloadCriteria(const std::string& guid,
                                      const SchedulingParams& params) = 0;

  // Exposes the owner of the download request for |guid| if one exists.
  // Otherwise returns DownloadClient::INVALID for an unowned entry.
  virtual DownloadClient GetOwnerOfDownload(const std::string& guid) = 0;

  // See DownloadService::OnStartScheduledTask.
  virtual void OnStartScheduledTask(DownloadTaskType task_type,
                                    const TaskFinishedCallback& callback) = 0;

  // See DownloadService::OnStopScheduledTask.
  virtual bool OnStopScheduledTask(DownloadTaskType task_type) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_CONTROLLER_H_
