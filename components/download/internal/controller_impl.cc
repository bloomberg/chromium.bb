// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/controller_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/internal/client_set.h"
#include "components/download/internal/config.h"
#include "components/download/internal/entry.h"
#include "components/download/internal/entry_utils.h"
#include "components/download/internal/model.h"
#include "components/download/internal/scheduler/scheduler.h"
#include "components/download/internal/stats.h"
#include "components/download/public/client.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace download {
namespace {

// Helper function to transit the state of |entry| to |new_state|.
void TransitTo(Entry* entry, Entry::State new_state, Model* model) {
  DCHECK(entry);
  if (entry->state == new_state)
    return;
  entry->state = new_state;
  model->Update(*entry);
}

}  // namespace

ControllerImpl::ControllerImpl(
    std::unique_ptr<ClientSet> clients,
    std::unique_ptr<Configuration> config,
    std::unique_ptr<DownloadDriver> driver,
    std::unique_ptr<Model> model,
    std::unique_ptr<DeviceStatusListener> device_status_listener,
    std::unique_ptr<Scheduler> scheduler,
    std::unique_ptr<TaskScheduler> task_scheduler)
    : clients_(std::move(clients)),
      config_(std::move(config)),
      driver_(std::move(driver)),
      model_(std::move(model)),
      device_status_listener_(std::move(device_status_listener)),
      scheduler_(std::move(scheduler)),
      task_scheduler_(std::move(task_scheduler)) {}

ControllerImpl::~ControllerImpl() = default;

void ControllerImpl::Initialize() {
  DCHECK(!startup_status_.Complete());

  driver_->Initialize(this);
  model_->Initialize(this);
}

const StartupStatus* ControllerImpl::GetStartupStatus() {
  return &startup_status_;
}

void ControllerImpl::StartDownload(const DownloadParams& params) {
  DCHECK(startup_status_.Ok());

  if (start_callbacks_.find(params.guid) != start_callbacks_.end() ||
      model_->Get(params.guid) != nullptr) {
    HandleStartDownloadResponse(params.client, params.guid,
                                DownloadParams::StartResult::UNEXPECTED_GUID,
                                params.callback);
    return;
  }

  auto* client = clients_->GetClient(params.client);
  if (!client) {
    HandleStartDownloadResponse(params.client, params.guid,
                                DownloadParams::StartResult::UNEXPECTED_CLIENT,
                                params.callback);
    return;
  }

  uint32_t client_count =
      util::GetNumberOfEntriesForClient(params.client, model_->PeekEntries());
  if (client_count >= config_->max_scheduled_downloads) {
    HandleStartDownloadResponse(params.client, params.guid,
                                DownloadParams::StartResult::BACKOFF,
                                params.callback);
    return;
  }

  start_callbacks_[params.guid] = params.callback;
  model_->Add(Entry(params));
}

void ControllerImpl::PauseDownload(const std::string& guid) {
  DCHECK(startup_status_.Ok());

  auto* entry = model_->Get(guid);

  if (!entry || entry->state == Entry::State::PAUSED ||
      entry->state == Entry::State::COMPLETE ||
      entry->state == Entry::State::WATCHDOG) {
    return;
  }

  TransitTo(entry, Entry::State::PAUSED, model_.get());
  UpdateDriverState(*entry);

  // Pausing a download may yield a concurrent slot to start a new download, and
  // may change the scheduling criteria.
  ActivateMoreDownloads();
}

void ControllerImpl::ResumeDownload(const std::string& guid) {
  DCHECK(startup_status_.Ok());

  auto* entry = model_->Get(guid);
  DCHECK(entry);

  if (entry->state != Entry::State::PAUSED)
    return;

  TransitTo(entry, Entry::State::ACTIVE, model_.get());
  UpdateDriverState(*entry);

  ActivateMoreDownloads();
}

void ControllerImpl::CancelDownload(const std::string& guid) {
  DCHECK(startup_status_.Ok());

  auto* entry = model_->Get(guid);
  if (!entry)
    return;

  if (entry->state == Entry::State::NEW) {
    // Check if we're currently trying to add the download.
    DCHECK(start_callbacks_.find(entry->guid) != start_callbacks_.end());
    HandleStartDownloadResponse(entry->client, entry->guid,
                                DownloadParams::StartResult::CLIENT_CANCELLED);
    return;
  }

  HandleCompleteDownload(CompletionType::CANCEL, guid);
}

void ControllerImpl::ChangeDownloadCriteria(const std::string& guid,
                                            const SchedulingParams& params) {
  DCHECK(startup_status_.Ok());

  auto* entry = model_->Get(guid);
  if (!entry || entry->scheduling_params == params) {
    DVLOG(1) << "Try to update the same scheduling parameters.";
    return;
  }

  UpdateDriverState(*entry);

  // Update the scheduling parameters.
  entry->scheduling_params = params;
  model_->Update(*entry);

  ActivateMoreDownloads();
}

DownloadClient ControllerImpl::GetOwnerOfDownload(const std::string& guid) {
  auto* entry = model_->Get(guid);
  return entry ? entry->client : DownloadClient::INVALID;
}

void ControllerImpl::OnStartScheduledTask(
    DownloadTaskType task_type,
    const TaskFinishedCallback& callback) {
  task_finished_callbacks_[task_type] = callback;
  if (!startup_status_.Complete()) {
    return;
  } else if (!startup_status_.Ok()) {
    HandleTaskFinished(task_type, false,
                       stats::ScheduledTaskStatus::ABORTED_ON_FAILED_INIT);
    return;
  }

  ProcessScheduledTasks();
}

bool ControllerImpl::OnStopScheduledTask(DownloadTaskType task_type) {
  HandleTaskFinished(task_type, false,
                     stats::ScheduledTaskStatus::CANCELLED_ON_STOP);
  return true;
}

void ControllerImpl::ProcessScheduledTasks() {
  if (!startup_status_.Ok()) {
    while (!task_finished_callbacks_.empty()) {
      auto it = task_finished_callbacks_.begin();
      HandleTaskFinished(it->first, false,
                         stats::ScheduledTaskStatus::ABORTED_ON_FAILED_INIT);
    }
    return;
  }

  while (!task_finished_callbacks_.empty()) {
    auto it = task_finished_callbacks_.begin();
    // TODO(shaktisahu): Execute the scheduled task.

    HandleTaskFinished(it->first, false,
                       stats::ScheduledTaskStatus::COMPLETED_NORMALLY);
  }
}

void ControllerImpl::HandleTaskFinished(DownloadTaskType task_type,
                                        bool needs_reschedule,
                                        stats::ScheduledTaskStatus status) {
  if (status != stats::ScheduledTaskStatus::CANCELLED_ON_STOP) {
    base::ResetAndReturn(&task_finished_callbacks_[task_type])
        .Run(needs_reschedule);
  }
  stats::LogScheduledTaskStatus(task_type, status);
  task_finished_callbacks_.erase(task_type);
}

void ControllerImpl::OnDriverReady(bool success) {
  DCHECK(!startup_status_.driver_ok.has_value());
  startup_status_.driver_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnDownloadCreated(const DriverEntry& download) {
  Entry* entry = model_->Get(download.guid);

  if (!entry) {
    // TODO(xingliu): Log non download service initiated downloads.
    return;
  }

  download::Client* client = clients_->GetClient(entry->client);
  DCHECK(client);
  using ShouldDownload = download::Client::ShouldDownload;
  ShouldDownload should_download = client->OnDownloadStarted(
      download.guid, download.url_chain, download.response_headers);
  if (should_download == ShouldDownload::ABORT) {
    HandleCompleteDownload(CompletionType::ABORT, entry->guid);
  }
}
void ControllerImpl::OnDownloadFailed(const DriverEntry& download, int reason) {
  Entry* entry = model_->Get(download.guid);
  if (!entry)
    return;

  HandleCompleteDownload(CompletionType::FAIL, download.guid);
}

void ControllerImpl::OnDownloadSucceeded(const DriverEntry& download,
                                         const base::FilePath& path) {
  Entry* entry = model_->Get(download.guid);
  if (!entry)
    return;

  HandleCompleteDownload(CompletionType::SUCCEED, download.guid);
}

void ControllerImpl::OnDownloadUpdated(const DriverEntry& download) {
  Entry* entry = model_->Get(download.guid);
  if (!entry)
    return;

  DCHECK_EQ(download.state, DriverEntry::State::IN_PROGRESS);

  download::Client* client = clients_->GetClient(entry->client);
  if (client)
    client->OnDownloadUpdated(download.guid, download.bytes_downloaded);
}

void ControllerImpl::OnModelReady(bool success) {
  DCHECK(!startup_status_.model_ok.has_value());
  startup_status_.model_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnItemAdded(bool success,
                                 DownloadClient client,
                                 const std::string& guid) {
  // If the StartCallback doesn't exist, we already notified the Client about
  // this item.  That means something went wrong, so stop here.
  if (start_callbacks_.find(guid) == start_callbacks_.end())
    return;

  if (!success) {
    HandleStartDownloadResponse(client, guid,
                                DownloadParams::StartResult::INTERNAL_ERROR);
    return;
  }

  HandleStartDownloadResponse(client, guid,
                              DownloadParams::StartResult::ACCEPTED);

  Entry* entry = model_->Get(guid);
  DCHECK(entry);
  DCHECK_EQ(Entry::State::NEW, entry->state);
  TransitTo(entry, Entry::State::AVAILABLE, model_.get());

  ActivateMoreDownloads();

  // TODO(dtrainor): Make sure we're running all of the downloads we care about.
}

void ControllerImpl::OnItemUpdated(bool success,
                                   DownloadClient client,
                                   const std::string& guid) {
  Entry* entry = model_->Get(guid);
  DCHECK(entry);
  if (entry->state == Entry::State::COMPLETE ||
      entry->state == Entry::State::WATCHDOG) {
    driver_->Remove(guid);
  }
}

void ControllerImpl::OnItemRemoved(bool success,
                                   DownloadClient client,
                                   const std::string& guid) {
  // TODO(dtrainor): Fail and clean up the download if necessary.
}

void ControllerImpl::OnDeviceStatusChanged(const DeviceStatus& device_status) {
  UpdateDriverStates();
  ActivateMoreDownloads();
}

void ControllerImpl::AttemptToFinalizeSetup() {
  if (!startup_status_.Complete())
    return;

  stats::LogControllerStartupStatus(startup_status_);
  if (!startup_status_.Ok()) {
    // TODO(dtrainor): Recover here.  Try to clean up any disk state and, if
    // possible, any DownloadDriver data and continue with initialization?
    ProcessScheduledTasks();
    return;
  }

  device_status_listener_->Start(this);
  CancelOrphanedRequests();
  ResolveInitialRequestStates();
  UpdateDriverStates();
  PullCurrentRequestStatus();

  // TODO(dtrainor): Post this so that the initialization step is finalized
  // before Clients can take action.
  NotifyClientsOfStartup();
  ProcessScheduledTasks();

  // Pull the initial straw if active downloads haven't reach maximum.
  ActivateMoreDownloads();
}

void ControllerImpl::CancelOrphanedRequests() {
  auto entries = model_->PeekEntries();

  std::vector<std::string> guids_to_remove;
  for (auto* entry : entries) {
    if (!clients_->GetClient(entry->client))
      guids_to_remove.push_back(entry->guid);
  }

  for (const auto& guid : guids_to_remove) {
    model_->Remove(guid);
    // TODO(xingliu): Use file monitor to delete the files.
    driver_->Remove(guid);
  }
}

void ControllerImpl::ResolveInitialRequestStates() {
  // TODO(dtrainor): Implement.
}

void ControllerImpl::UpdateDriverStates() {
  DCHECK(startup_status_.Complete());

  for (auto* const entry : model_->PeekEntries())
    UpdateDriverState(*entry);
}

void ControllerImpl::UpdateDriverState(const Entry& entry) {
  base::Optional<DriverEntry> driver_entry = driver_->Find(entry.guid);

  bool meets_device_criteria = device_status_listener_->CurrentDeviceStatus()
                                   .MeetsCondition(entry.scheduling_params)
                                   .MeetsRequirements();
  switch (entry.state) {
    case Entry::State::ACTIVE:
      if (!meets_device_criteria) {
        driver_->Pause(entry.guid);
        break;
      }
      // Start or resume the download if it should be running.
      if (!driver_entry.has_value()) {
        driver_->Start(entry.request_params, entry.guid,
                       NO_TRAFFIC_ANNOTATION_YET);
        break;
      }
      if (driver_entry->state != DriverEntry::State::IN_PROGRESS) {
        driver_->Resume(entry.guid);
      }
      break;
    case Entry::State::PAUSED:
      // Pause the in progress downloads that should not be running.
      if (driver_entry.has_value() &&
          driver_entry->state == DriverEntry::State::IN_PROGRESS) {
        driver_->Pause(entry.guid);
      }
      break;
    // Fall through.
    case Entry::State::AVAILABLE:
    case Entry::State::NEW:
    case Entry::State::COMPLETE:
    case Entry::State::WATCHDOG:
      break;
    default:
      NOTREACHED();
  }
}

void ControllerImpl::PullCurrentRequestStatus() {
  // TODO(dtrainor): Implement.
}

void ControllerImpl::NotifyClientsOfStartup() {
  auto categorized = util::MapEntriesToClients(clients_->GetRegisteredClients(),
                                               model_->PeekEntries());

  for (auto client_id : clients_->GetRegisteredClients()) {
    clients_->GetClient(client_id)->OnServiceInitialized(
        categorized[client_id]);
  }
}

void ControllerImpl::HandleStartDownloadResponse(
    DownloadClient client,
    const std::string& guid,
    DownloadParams::StartResult result) {
  auto callback = start_callbacks_[guid];
  start_callbacks_.erase(guid);
  HandleStartDownloadResponse(client, guid, result, callback);
}

void ControllerImpl::HandleStartDownloadResponse(
    DownloadClient client,
    const std::string& guid,
    DownloadParams::StartResult result,
    const DownloadParams::StartCallback& callback) {
  stats::LogStartDownloadResult(client, result);

  if (result != DownloadParams::StartResult::ACCEPTED) {
    // TODO(dtrainor): Clean up any download state.
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, guid, result));
}

void ControllerImpl::HandleCompleteDownload(CompletionType type,
                                            const std::string& guid) {
  Entry* entry = model_->Get(guid);
  DCHECK(entry);
  stats::LogDownloadCompletion(type);

  if (entry->state == Entry::State::COMPLETE ||
      entry->state == Entry::State::WATCHDOG) {
    DVLOG(1) << "Download is already completed.";
    return;
  }

  // TODO(xingliu): Notify the client based on completion type.
  TransitTo(entry, Entry::State::COMPLETE, model_.get());
  ActivateMoreDownloads();
}

void ControllerImpl::ActivateMoreDownloads() {
  // TODO(xingliu): Check the configuration to throttle downloads.
  Entry* next = scheduler_->Next(
      model_->PeekEntries(), device_status_listener_->CurrentDeviceStatus());

  while (next) {
    DCHECK_EQ(Entry::State::AVAILABLE, next->state);
    TransitTo(next, Entry::State::ACTIVE, model_.get());
    UpdateDriverState(*next);
    next = scheduler_->Next(model_->PeekEntries(),
                            device_status_listener_->CurrentDeviceStatus());
  }
  scheduler_->Reschedule(model_->PeekEntries());
}

}  // namespace download
