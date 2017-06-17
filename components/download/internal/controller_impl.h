// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_CONTROLLER_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/download/internal/controller.h"
#include "components/download/internal/download_driver.h"
#include "components/download/internal/entry.h"
#include "components/download/internal/model.h"
#include "components/download/internal/scheduler/device_status_listener.h"
#include "components/download/internal/startup_status.h"
#include "components/download/internal/stats.h"
#include "components/download/public/client.h"
#include "components/download/public/download_params.h"
#include "components/download/public/task_scheduler.h"

namespace download {

class ClientSet;
class DownloadDriver;
class Model;
class Scheduler;

struct Configuration;
struct SchedulingParams;

// The internal Controller implementation.  This class does all of the heavy
// lifting for the DownloadService.
class ControllerImpl : public Controller,
                       public DownloadDriver::Client,
                       public Model::Client,
                       public DeviceStatusListener::Observer {
 public:
  // |config| is externally owned and must be guaranteed to outlive this class.
  ControllerImpl(Configuration* config,
                 std::unique_ptr<ClientSet> clients,
                 std::unique_ptr<DownloadDriver> driver,
                 std::unique_ptr<Model> model,
                 std::unique_ptr<DeviceStatusListener> device_status_listener,
                 std::unique_ptr<Scheduler> scheduler,
                 std::unique_ptr<TaskScheduler> task_scheduler);
  ~ControllerImpl() override;

  // Controller implementation.
  void Initialize() override;
  const StartupStatus* GetStartupStatus() override;
  void StartDownload(const DownloadParams& params) override;
  void PauseDownload(const std::string& guid) override;
  void ResumeDownload(const std::string& guid) override;
  void CancelDownload(const std::string& guid) override;
  void ChangeDownloadCriteria(const std::string& guid,
                              const SchedulingParams& params) override;
  DownloadClient GetOwnerOfDownload(const std::string& guid) override;
  void OnStartScheduledTask(DownloadTaskType task_type,
                            const TaskFinishedCallback& callback) override;
  bool OnStopScheduledTask(DownloadTaskType task_type) override;

 private:
  // DownloadDriver::Client implementation.
  void OnDriverReady(bool success) override;
  void OnDownloadCreated(const DriverEntry& download) override;
  void OnDownloadFailed(const DriverEntry& download, int reason) override;
  void OnDownloadSucceeded(const DriverEntry& download,
                           const base::FilePath& path) override;
  void OnDownloadUpdated(const DriverEntry& download) override;

  // Model::Client implementation.
  void OnModelReady(bool success) override;
  void OnItemAdded(bool success,
                   DownloadClient client,
                   const std::string& guid) override;
  void OnItemUpdated(bool success,
                     DownloadClient client,
                     const std::string& guid) override;
  void OnItemRemoved(bool success,
                     DownloadClient client,
                     const std::string& guid) override;

  // DeviceStatusListener::Observer implementation.
  void OnDeviceStatusChanged(const DeviceStatus& device_status) override;

  // Checks if initialization is complete and successful.  If so, completes the
  // internal state initialization.
  void AttemptToFinalizeSetup();

  // Checks for all the currently active driver downloads.  This lets us know
  // which ones are active that we haven't tracked.
  void PollActiveDriverDownloads();

  // Cancels and cleans upany requests that are no longer associated with a
  // Client in |clients_|.
  void CancelOrphanedRequests();

  // Fixes any discrepancies in state between |model_| and |driver_|.  Meant to
  // resolve state issues during startup.
  void ResolveInitialRequestStates();

  // Updates the driver states based on the states of entries in download
  // service.
  void UpdateDriverStates();

  // Processes the download based on the state of |entry|. May start, pause
  // or resume a download accordingly.
  void UpdateDriverState(const Entry& entry);

  // Notifies all Client in |clients_| that this controller is initialized and
  // lets them know which download requests we are aware of for their
  // DownloadClient.
  void NotifyClientsOfStartup();

  void HandleStartDownloadResponse(DownloadClient client,
                                   const std::string& guid,
                                   DownloadParams::StartResult result);
  void HandleStartDownloadResponse(
      DownloadClient client,
      const std::string& guid,
      DownloadParams::StartResult result,
      const DownloadParams::StartCallback& callback);

  // Entry point for a scheduled task after the task is fired.
  void ProcessScheduledTasks();

  // Handles and clears any pending task finished callbacks.
  void HandleTaskFinished(DownloadTaskType task_type,
                          bool needs_reschedule,
                          stats::ScheduledTaskStatus status);

  void HandleCompleteDownload(CompletionType type, const std::string& guid);

  // Find more available entries to download, until the number of active entries
  // reached maximum.
  void ActivateMoreDownloads();

  void HandleExternalDownload(const std::string& guid, bool active);

  // Postable methods meant to just be pass throughs to Client APIs.  This is
  // meant to help prevent reentrancy.
  void SendOnDownloadUpdated(DownloadClient client_id,
                             const std::string& guid,
                             uint64_t bytes_downloaded);
  void SendOnDownloadSucceeded(DownloadClient client_id,
                               const std::string& guid,
                               const base::FilePath& path,
                               uint64_t size);
  void SendOnDownloadFailed(DownloadClient client_id,
                            const std::string& guid,
                            download::Client::FailureReason reason);

  Configuration* config_;

  // Owned Dependencies.
  std::unique_ptr<ClientSet> clients_;
  std::unique_ptr<DownloadDriver> driver_;
  std::unique_ptr<Model> model_;
  std::unique_ptr<DeviceStatusListener> device_status_listener_;
  std::unique_ptr<Scheduler> scheduler_;
  std::unique_ptr<TaskScheduler> task_scheduler_;

  // Internal state.
  // Is set to true if this class is currently in the process of initializing
  // it's internal state.  This will be false until |startup_status_| signals it
  // is complete *and* all internal structures are set up.  This is to prevent
  // outside signals from triggering state updates before we are ready.
  bool initializing_internals_;
  StartupStatus startup_status_;
  std::set<std::string> externally_active_downloads_;
  std::map<std::string, DownloadParams::StartCallback> start_callbacks_;
  std::map<DownloadTaskType, TaskFinishedCallback> task_finished_callbacks_;

  // Only used to post tasks on the same thread.
  base::WeakPtrFactory<ControllerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ControllerImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_CONTROLLER_IMPL_H_
