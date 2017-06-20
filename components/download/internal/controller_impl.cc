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

// Helper function to move from a CompletionType to a Client::FailureReason.
Client::FailureReason FailureReasonFromCompletionType(CompletionType type) {
  // SUCCEED does not map to a FailureReason.
  DCHECK_NE(CompletionType::SUCCEED, type);

  switch (type) {
    case CompletionType::FAIL:
      return Client::FailureReason::NETWORK;
    case CompletionType::ABORT:
      return Client::FailureReason::ABORTED;
    case CompletionType::TIMEOUT:
      return Client::FailureReason::TIMEDOUT;
    case CompletionType::UNKNOWN:
      return Client::FailureReason::UNKNOWN;
    case CompletionType::CANCEL:
      return Client::FailureReason::CANCELLED;
    default:
      NOTREACHED();
  }

  return Client::FailureReason::UNKNOWN;
}

}  // namespace

ControllerImpl::ControllerImpl(
    Configuration* config,
    std::unique_ptr<ClientSet> clients,
    std::unique_ptr<DownloadDriver> driver,
    std::unique_ptr<Model> model,
    std::unique_ptr<DeviceStatusListener> device_status_listener,
    std::unique_ptr<Scheduler> scheduler,
    std::unique_ptr<TaskScheduler> task_scheduler)
    : config_(config),
      clients_(std::move(clients)),
      driver_(std::move(driver)),
      model_(std::move(model)),
      device_status_listener_(std::move(device_status_listener)),
      scheduler_(std::move(scheduler)),
      task_scheduler_(std::move(task_scheduler)),
      initializing_internals_(false),
      weak_ptr_factory_(this) {}

ControllerImpl::~ControllerImpl() = default;

void ControllerImpl::Initialize() {
  DCHECK(!startup_status_.Complete());

  initializing_internals_ = true;
  driver_->Initialize(this);
  model_->Initialize(this);
}

const StartupStatus* ControllerImpl::GetStartupStatus() {
  return &startup_status_;
}

void ControllerImpl::StartDownload(const DownloadParams& params) {
  DCHECK(startup_status_.Ok());

  // TODO(dtrainor): Check if there are any downloads we can cancel.  We don't
  // want to return a BACKOFF if we technically could time out a download to
  // start this one.

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
      entry->state == Entry::State::NEW) {
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
    HandleStartDownloadResponse(entry->client, guid,
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
  // TODO(dtrainor): It might be useful to log how many downloads we have
  // running when we're asked to stop processing.
  stats::LogScheduledTaskStatus(task_type, status);
  task_finished_callbacks_.erase(task_type);
}

void ControllerImpl::OnDriverReady(bool success) {
  DCHECK(!startup_status_.driver_ok.has_value());
  startup_status_.driver_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnDownloadCreated(const DriverEntry& download) {
  if (initializing_internals_)
    return;

  Entry* entry = model_->Get(download.guid);

  if (!entry) {
    HandleExternalDownload(download.guid, true);
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
  if (initializing_internals_)
    return;

  Entry* entry = model_->Get(download.guid);
  if (!entry) {
    HandleExternalDownload(download.guid, false);
    return;
  }

  // TODO(dtrainor): Add retry logic here.  Connect to restart code for tracking
  // number of retries.

  // TODO(dtrainor, xingliu): We probably have to prevent cancel calls from
  // coming through here as we remove downloads (especially through
  // initialization).
  HandleCompleteDownload(CompletionType::FAIL, download.guid);
}

void ControllerImpl::OnDownloadSucceeded(const DriverEntry& download,
                                         const base::FilePath& path) {
  if (initializing_internals_)
    return;

  Entry* entry = model_->Get(download.guid);
  if (!entry) {
    HandleExternalDownload(download.guid, false);
    return;
  }

  HandleCompleteDownload(CompletionType::SUCCEED, download.guid);
}

void ControllerImpl::OnDownloadUpdated(const DriverEntry& download) {
  if (initializing_internals_)
    return;

  Entry* entry = model_->Get(download.guid);
  if (!entry) {
    HandleExternalDownload(download.guid, !download.paused);
    return;
  }

  DCHECK_EQ(download.state, DriverEntry::State::IN_PROGRESS);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ControllerImpl::SendOnDownloadUpdated,
                            weak_ptr_factory_.GetWeakPtr(), entry->client,
                            download.guid, download.bytes_downloaded));
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
}

void ControllerImpl::OnItemUpdated(bool success,
                                   DownloadClient client,
                                   const std::string& guid) {
  Entry* entry = model_->Get(guid);
  DCHECK(entry);

  // Now that we're sure that our state is set correctly, it is OK to remove the
  // DriverEntry.  If we restart we'll see a COMPLETE state and handle it
  // accordingly.
  if (entry->state == Entry::State::COMPLETE)
    driver_->Remove(guid);

  // TODO(dtrainor): If failed, clean up any download state accordingly.
}

void ControllerImpl::OnItemRemoved(bool success,
                                   DownloadClient client,
                                   const std::string& guid) {
  // TODO(dtrainor): If failed, clean up any download state accordingly.
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
  PollActiveDriverDownloads();
  CancelOrphanedRequests();
  ResolveInitialRequestStates();
  NotifyClientsOfStartup();

  initializing_internals_ = false;

  UpdateDriverStates();
  ProcessScheduledTasks();

  // Pull the initial straw if active downloads haven't reach maximum.
  ActivateMoreDownloads();
}

void ControllerImpl::PollActiveDriverDownloads() {
  std::set<std::string> guids = driver_->GetActiveDownloads();

  for (auto guid : guids) {
    if (!model_->Get(guid))
      externally_active_downloads_.insert(guid);
  }
}

void ControllerImpl::CancelOrphanedRequests() {
  auto entries = model_->PeekEntries();

  std::vector<std::string> guids_to_remove;
  for (auto* entry : entries) {
    if (!clients_->GetClient(entry->client))
      guids_to_remove.push_back(entry->guid);
  }

  for (const auto& guid : guids_to_remove) {
    // TODO(xingliu): Use file monitor to delete the files.
    driver_->Remove(guid);
    model_->Remove(guid);
  }
}

void ControllerImpl::ResolveInitialRequestStates() {
  auto entries = model_->PeekEntries();
  for (auto* entry : entries) {
    // Pull the initial Entry::State and DriverEntry::State.
    Entry::State state = entry->state;
    auto driver_entry = driver_->Find(entry->guid);
    base::Optional<DriverEntry::State> driver_state;
    if (driver_entry.has_value()) {
      DCHECK_NE(DriverEntry::State::UNKNOWN, driver_entry->state);
      driver_state = driver_entry->state;
    }

    // Determine what the new Entry::State should be based on the two original
    // states of the two different systems.
    Entry::State new_state = state;
    switch (state) {
      case Entry::State::NEW:
        // This means we shut down but may have not ACK'ed the download.  That
        // is OK, we will still notify the Client about the GUID when we send
        // them our initialize method.
        new_state = Entry::State::AVAILABLE;
        break;
      case Entry::State::COMPLETE:
        // We're already in our end state.  Just stay here.
        new_state = Entry::State::COMPLETE;
        break;
      case Entry::State::AVAILABLE:  // Intentional fallthrough.
      case Entry::State::ACTIVE:     // Intentional fallthrough.
      case Entry::State::PAUSED: {
        // All three of these states are effectively driven by the DriverEntry
        // state.
        if (!driver_state.has_value()) {
          // If we don't have a DriverEntry::State, just leave the state alone.
          new_state = state;
          break;
        }

        // If we have a real DriverEntry::State, we need to determine which of
        // those states makes sense for our Entry.  Our entry can either be in
        // two states now: It's effective 'active' state (ACTIVE or PAUSED) or
        // COMPLETE.
        bool is_paused = state == Entry::State::PAUSED;
        Entry::State active =
            is_paused ? Entry::State::PAUSED : Entry::State::ACTIVE;

        switch (driver_state.value()) {
          case DriverEntry::State::IN_PROGRESS:  // Intentional fallthrough.
          case DriverEntry::State::INTERRUPTED:
            // The DriverEntry isn't done, so we need to set the Entry to the
            // 'active' state.
            new_state = active;
            break;
          case DriverEntry::State::COMPLETE:  // Intentional fallthrough.
          // TODO(dtrainor, xingliu) Revisit this CANCELLED state to make sure
          // all embedders behave properly.
          case DriverEntry::State::CANCELLED:
            // The DriverEntry is done.  We need to set the Entry to the
            // COMPLETE state.
            new_state = Entry::State::COMPLETE;
            break;
          default:
            NOTREACHED();
            break;
        }
        break;
      }
      default:
        NOTREACHED();
        break;
    }

    // Update the Entry::State to the new correct state.
    if (new_state != entry->state) {
      stats::LogRecoveryOperation(new_state);
      TransitTo(entry, new_state, model_.get());
    }

    // Given the new correct state, update the DriverEntry to reflect the Entry.
    switch (new_state) {
      case Entry::State::NEW:        // Intentional fallthrough.
      case Entry::State::AVAILABLE:  // Intentional fallthrough.
        // We should not have a DriverEntry here.
        if (driver_entry.has_value())
          driver_->Remove(entry->guid);
        break;
      case Entry::State::ACTIVE:  // Intentional fallthrough.
      case Entry::State::PAUSED:
        // We're in the correct state.  Let UpdateDriverStates() restart us if
        // it wants to.
        break;
      case Entry::State::COMPLETE:
        if (state != Entry::State::COMPLETE) {
          // We are changing states to COMPLETE.  Handle this like a normal
          // completed download.

          // Treat CANCELLED and INTERRUPTED as failures.  We have to assume the
          // DriverEntry might not have persisted in time.
          CompletionType completion_type =
              (!driver_entry.has_value() ||
               driver_entry->state == DriverEntry::State::CANCELLED ||
               driver_entry->state == DriverEntry::State::INTERRUPTED)
                  ? CompletionType::UNKNOWN
                  : CompletionType::SUCCEED;
          HandleCompleteDownload(completion_type, entry->guid);
        } else {
          // We're staying in COMPLETE.  Make sure there is no DriverEntry here.
          if (driver_entry.has_value())
            driver_->Remove(entry->guid);
        }
        break;
    }
  }
}

void ControllerImpl::UpdateDriverStates() {
  DCHECK(startup_status_.Complete());

  for (auto* const entry : model_->PeekEntries())
    UpdateDriverState(*entry);
}

void ControllerImpl::UpdateDriverState(const Entry& entry) {
  DCHECK(!initializing_internals_);

  if (entry.state != Entry::State::ACTIVE &&
      entry.state != Entry::State::PAUSED) {
    return;
  }

  // This method will need to figure out what to do with a failed download and
  // either a) restart it or b) fail the download.

  base::Optional<DriverEntry> driver_entry = driver_->Find(entry.guid);

  bool meets_device_criteria = device_status_listener_->CurrentDeviceStatus()
                                   .MeetsCondition(entry.scheduling_params)
                                   .MeetsRequirements();
  bool force_pause =
      !externally_active_downloads_.empty() &&
      entry.scheduling_params.priority != SchedulingParams::Priority::UI;
  bool entry_paused = entry.state == Entry::State::PAUSED;

  bool pause_driver = entry_paused || force_pause || !meets_device_criteria;

  if (pause_driver) {
    if (driver_entry.has_value())
      driver_->Pause(entry.guid);
  } else {
    if (driver_entry.has_value()) {
      driver_->Resume(entry.guid);
    } else {
      driver_->Start(entry.request_params, entry.guid,
                     NO_TRAFFIC_ANNOTATION_YET);
    }
  }
}

void ControllerImpl::NotifyClientsOfStartup() {
  std::set<Entry::State> ignored_states = {Entry::State::COMPLETE};
  auto categorized = util::MapEntriesToClients(
      clients_->GetRegisteredClients(), model_->PeekEntries(), ignored_states);

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

  // UNEXPECTED_GUID means the guid was already in use.  Don't remove this entry
  // from the model because it's there due to another request.
  if (result != DownloadParams::StartResult::ACCEPTED &&
      result != DownloadParams::StartResult::UNEXPECTED_GUID &&
      model_->Get(guid) != nullptr) {
    model_->Remove(guid);
  }

  if (callback.is_null())
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, guid, result));
}

void ControllerImpl::HandleCompleteDownload(CompletionType type,
                                            const std::string& guid) {
  Entry* entry = model_->Get(guid);
  DCHECK(entry);
  stats::LogDownloadCompletion(type);

  if (entry->state == Entry::State::COMPLETE) {
    DVLOG(1) << "Download is already completed.";
    return;
  }

  if (type == CompletionType::SUCCEED) {
    auto driver_entry = driver_->Find(guid);
    DCHECK(driver_entry.has_value());
    // TODO(dtrainor): Move the FilePath generation to the controller and store
    // it in Entry.  Then pass it into the DownloadDriver.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ControllerImpl::SendOnDownloadSucceeded,
                   weak_ptr_factory_.GetWeakPtr(), entry->client, guid,
                   base::FilePath(), driver_entry->bytes_downloaded));
    TransitTo(entry, Entry::State::COMPLETE, model_.get());
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ControllerImpl::SendOnDownloadFailed,
                              weak_ptr_factory_.GetWeakPtr(), entry->client,
                              guid, FailureReasonFromCompletionType(type)));
    model_->Remove(guid);
  }

  ActivateMoreDownloads();
}

void ControllerImpl::ActivateMoreDownloads() {
  if (initializing_internals_)
    return;

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

void ControllerImpl::HandleExternalDownload(const std::string& guid,
                                            bool active) {
  if (active) {
    externally_active_downloads_.insert(guid);
  } else {
    externally_active_downloads_.erase(guid);
  }

  UpdateDriverStates();
}

void ControllerImpl::SendOnDownloadUpdated(DownloadClient client_id,
                                           const std::string& guid,
                                           uint64_t bytes_downloaded) {
  if (!model_->Get(guid))
    return;

  auto* client = clients_->GetClient(client_id);
  DCHECK(client);
  client->OnDownloadUpdated(guid, bytes_downloaded);
}

void ControllerImpl::SendOnDownloadSucceeded(DownloadClient client_id,
                                             const std::string& guid,
                                             const base::FilePath& path,
                                             uint64_t size) {
  auto* client = clients_->GetClient(client_id);
  DCHECK(client);
  client->OnDownloadSucceeded(guid, path, size);
}

void ControllerImpl::SendOnDownloadFailed(
    DownloadClient client_id,
    const std::string& guid,
    download::Client::FailureReason reason) {
  auto* client = clients_->GetClient(client_id);
  DCHECK(client);
  client->OnDownloadFailed(guid, reason);
}

}  // namespace download
