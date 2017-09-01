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
#include "components/download/internal/file_monitor.h"
#include "components/download/internal/model.h"
#include "components/download/internal/scheduler/scheduler.h"
#include "components/download/internal/stats.h"
#include "components/download/public/client.h"
#include "components/download/public/download_metadata.h"
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

// Helper function to determine if more downloads can be activated based on
// configuration.
bool CanActivateMoreDownloads(Configuration* config,
                              uint32_t active_count,
                              uint32_t paused_count) {
  if (config->max_concurrent_downloads <= paused_count + active_count ||
      config->max_running_downloads <= active_count) {
    return false;
  }
  return true;
}

}  // namespace

ControllerImpl::ControllerImpl(
    Configuration* config,
    std::unique_ptr<ClientSet> clients,
    std::unique_ptr<DownloadDriver> driver,
    std::unique_ptr<Model> model,
    std::unique_ptr<DeviceStatusListener> device_status_listener,
    std::unique_ptr<Scheduler> scheduler,
    std::unique_ptr<TaskScheduler> task_scheduler,
    std::unique_ptr<FileMonitor> file_monitor,
    const base::FilePath& download_file_dir)
    : config_(config),
      download_file_dir_(download_file_dir),
      clients_(std::move(clients)),
      driver_(std::move(driver)),
      model_(std::move(model)),
      device_status_listener_(std::move(device_status_listener)),
      scheduler_(std::move(scheduler)),
      task_scheduler_(std::move(task_scheduler)),
      file_monitor_(std::move(file_monitor)),
      controller_state_(State::CREATED),
      weak_ptr_factory_(this) {}

ControllerImpl::~ControllerImpl() = default;

void ControllerImpl::Initialize(const base::Closure& callback) {
  DCHECK_EQ(controller_state_, State::CREATED);

  init_callback_ = callback;
  controller_state_ = State::INITIALIZING;

  driver_->Initialize(this);
  model_->Initialize(this);
  file_monitor_->Initialize(base::Bind(&ControllerImpl::OnFileMonitorReady,
                                       weak_ptr_factory_.GetWeakPtr()));
}

Controller::State ControllerImpl::GetState() {
  return controller_state_;
}

void ControllerImpl::StartDownload(const DownloadParams& params) {
  DCHECK(controller_state_ == State::READY ||
         controller_state_ == State::UNAVAILABLE);

  // TODO(dtrainor): Validate all input parameters.
  DCHECK_LE(base::Time::Now(), params.scheduling_params.cancel_time);

  if (controller_state_ != State::READY) {
    HandleStartDownloadResponse(params.client, params.guid,
                                DownloadParams::StartResult::INTERNAL_ERROR,
                                params.callback);
    return;
  }

  KillTimedOutDownloads();

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

  uint32_t client_count = util::GetNumberOfLiveEntriesForClient(
      params.client, model_->PeekEntries());
  if (client_count >= config_->max_scheduled_downloads) {
    HandleStartDownloadResponse(params.client, params.guid,
                                DownloadParams::StartResult::BACKOFF,
                                params.callback);
    return;
  }

  start_callbacks_[params.guid] = params.callback;
  Entry entry(params);
  entry.target_file_path = download_file_dir_.AppendASCII(params.guid);
  model_->Add(entry);
}

void ControllerImpl::PauseDownload(const std::string& guid) {
  DCHECK(controller_state_ == State::READY ||
         controller_state_ == State::UNAVAILABLE);
  if (controller_state_ != State::READY)
    return;

  auto* entry = model_->Get(guid);

  if (!entry || entry->state == Entry::State::PAUSED ||
      entry->state == Entry::State::COMPLETE ||
      entry->state == Entry::State::NEW) {
    return;
  }

  TransitTo(entry, Entry::State::PAUSED, model_.get());
  UpdateDriverState(entry);

  // Pausing a download may yield a concurrent slot to start a new download, and
  // may change the scheduling criteria.
  ActivateMoreDownloads();
}

void ControllerImpl::ResumeDownload(const std::string& guid) {
  DCHECK(controller_state_ == State::READY ||
         controller_state_ == State::UNAVAILABLE);
  if (controller_state_ != State::READY)
    return;

  auto* entry = model_->Get(guid);
  DCHECK(entry);

  if (entry->state != Entry::State::PAUSED)
    return;

  TransitTo(entry, Entry::State::ACTIVE, model_.get());
  UpdateDriverState(entry);

  ActivateMoreDownloads();
}

void ControllerImpl::CancelDownload(const std::string& guid) {
  DCHECK(controller_state_ == State::READY ||
         controller_state_ == State::UNAVAILABLE);
  if (controller_state_ != State::READY)
    return;

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
  DCHECK(controller_state_ == State::READY ||
         controller_state_ == State::UNAVAILABLE);
  if (controller_state_ != State::READY)
    return;

  auto* entry = model_->Get(guid);
  if (!entry || entry->scheduling_params == params) {
    DVLOG(1) << "Try to update the same scheduling parameters.";
    return;
  }

  UpdateDriverState(entry);

  // Update the scheduling parameters.
  entry->scheduling_params = params;
  model_->Update(*entry);

  ActivateMoreDownloads();
}

DownloadClient ControllerImpl::GetOwnerOfDownload(const std::string& guid) {
  DCHECK(controller_state_ == State::READY ||
         controller_state_ == State::UNAVAILABLE);
  if (controller_state_ != State::READY)
    return DownloadClient::INVALID;

  auto* entry = model_->Get(guid);
  return entry ? entry->client : DownloadClient::INVALID;
}

void ControllerImpl::OnStartScheduledTask(
    DownloadTaskType task_type,
    const TaskFinishedCallback& callback) {
  task_finished_callbacks_[task_type] = callback;

  switch (controller_state_) {
    case State::READY:
      if (task_type == DownloadTaskType::DOWNLOAD_TASK) {
        ActivateMoreDownloads();
      } else if (task_type == DownloadTaskType::CLEANUP_TASK) {
        RemoveCleanupEligibleDownloads();
        ScheduleCleanupTask();
      }
      break;
    case State::UNAVAILABLE:
      HandleTaskFinished(task_type, false,
                         stats::ScheduledTaskStatus::ABORTED_ON_FAILED_INIT);
      break;
    case State::CREATED:       // Intentional fallthrough.
    case State::INITIALIZING:  // Intentional fallthrough.
    case State::RECOVERING:    // Intentional fallthrough.
    default:
      NOTREACHED();
      break;
  }
}

bool ControllerImpl::OnStopScheduledTask(DownloadTaskType task_type) {
  HandleTaskFinished(task_type, false,
                     stats::ScheduledTaskStatus::CANCELLED_ON_STOP);
  return true;
}

void ControllerImpl::OnCompleteCleanupTask() {
  HandleTaskFinished(DownloadTaskType::CLEANUP_TASK, false,
                     stats::ScheduledTaskStatus::COMPLETED_NORMALLY);
}

void ControllerImpl::RemoveCleanupEligibleDownloads() {
  std::vector<Entry*> entries_to_remove;
  for (auto* entry : model_->PeekEntries()) {
    if (entry->state != Entry::State::COMPLETE)
      continue;

    bool optional_cleanup =
        base::Time::Now() >
        (entry->last_cleanup_check_time + config_->file_keep_alive_time);
    bool mandatory_cleanup =
        base::Time::Now() >
        (entry->completion_time + config_->max_file_keep_alive_time);

    if (!optional_cleanup && !mandatory_cleanup)
      continue;

    download::Client* client = clients_->GetClient(entry->client);
    DCHECK(client);
    bool client_ok =
        client->CanServiceRemoveDownloadedFile(entry->guid, mandatory_cleanup);
    entry->cleanup_attempt_count++;

    if (client_ok || mandatory_cleanup) {
      entries_to_remove.push_back(entry);
    } else {
      entry->last_cleanup_check_time = base::Time::Now();
    }
  }

  file_monitor_->CleanupFilesForCompletedEntries(
      entries_to_remove, base::Bind(&ControllerImpl::OnCompleteCleanupTask,
                                    weak_ptr_factory_.GetWeakPtr()));

  for (auto* entry : entries_to_remove) {
    DCHECK_EQ(Entry::State::COMPLETE, entry->state);
    model_->Remove(entry->guid);
  }
}

void ControllerImpl::HandleTaskFinished(DownloadTaskType task_type,
                                        bool needs_reschedule,
                                        stats::ScheduledTaskStatus status) {
  if (task_finished_callbacks_.find(task_type) ==
      task_finished_callbacks_.end()) {
    return;
  }

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

void ControllerImpl::OnDriverHardRecoverComplete(bool success) {
  DCHECK(!startup_status_.driver_ok.has_value());
  startup_status_.driver_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnDownloadCreated(const DriverEntry& download) {
  if (controller_state_ != State::READY)
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
  stats::LogStartDownloadResponse(entry->client, should_download);
  if (should_download == ShouldDownload::ABORT) {
    HandleCompleteDownload(CompletionType::ABORT, entry->guid);
  }
}

void ControllerImpl::OnDownloadFailed(const DriverEntry& download,
                                      FailureType failure_type) {
  if (controller_state_ != State::READY)
    return;

  Entry* entry = model_->Get(download.guid);
  if (!entry) {
    HandleExternalDownload(download.guid, false);
    return;
  }

  if (failure_type == FailureType::RECOVERABLE) {
    // Because the network offline signal comes later than actual download
    // failure, retry the download after a delay to avoid the retry to fail
    // immediately again.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ControllerImpl::UpdateDriverStateWithGuid,
                   weak_ptr_factory_.GetWeakPtr(), download.guid),
        config_->download_retry_delay);
  } else {
    // TODO(dtrainor, xingliu): We probably have to prevent cancel calls from
    // coming through here as we remove downloads (especially through
    // initialization).
    HandleCompleteDownload(CompletionType::FAIL, download.guid);
  }
}

void ControllerImpl::OnDownloadSucceeded(const DriverEntry& download) {
  if (controller_state_ != State::READY)
    return;

  Entry* entry = model_->Get(download.guid);
  if (!entry) {
    HandleExternalDownload(download.guid, false);
    return;
  }

  HandleCompleteDownload(CompletionType::SUCCEED, download.guid);
}

void ControllerImpl::OnDownloadUpdated(const DriverEntry& download) {
  if (controller_state_ != State::READY)
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

void ControllerImpl::OnFileMonitorReady(bool success) {
  DCHECK(!startup_status_.file_monitor_ok.has_value());
  startup_status_.file_monitor_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnFileMonitorHardRecoverComplete(bool success) {
  DCHECK(!startup_status_.file_monitor_ok.has_value());
  startup_status_.file_monitor_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnModelReady(bool success) {
  DCHECK(!startup_status_.model_ok.has_value());
  startup_status_.model_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnModelHardRecoverComplete(bool success) {
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
  if (controller_state_ != State::READY)
    return;

  UpdateDriverStates();
  ActivateMoreDownloads();
}

void ControllerImpl::AttemptToFinalizeSetup() {
  DCHECK(controller_state_ == State::INITIALIZING ||
         controller_state_ == State::RECOVERING);

  if (!startup_status_.Complete())
    return;

  bool in_recovery = controller_state_ == State::RECOVERING;

  stats::LogControllerStartupStatus(in_recovery, startup_status_);
  if (!startup_status_.Ok()) {
    if (in_recovery) {
      HandleUnrecoverableSetup();
      NotifyServiceOfStartup();
    } else {
      StartHardRecoveryAttempt();
    }

    return;
  }

  device_status_listener_->Start(this);
  PollActiveDriverDownloads();
  CancelOrphanedRequests();
  CleanupUnknownFiles();
  RemoveCleanupEligibleDownloads();
  ResolveInitialRequestStates();

  NotifyClientsOfStartup(in_recovery);

  controller_state_ = State::READY;

  UpdateDriverStates();

  KillTimedOutDownloads();
  NotifyServiceOfStartup();

  // Pull the initial straw if active downloads haven't reach maximum.
  ActivateMoreDownloads();
}

void ControllerImpl::HandleUnrecoverableSetup() {
  controller_state_ = State::UNAVAILABLE;

  // If we cannot recover, notify Clients that the service is unavailable.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ControllerImpl::SendOnServiceUnavailable,
                            weak_ptr_factory_.GetWeakPtr()));
}

void ControllerImpl::StartHardRecoveryAttempt() {
  startup_status_.Reset();
  controller_state_ = State::RECOVERING;

  driver_->HardRecover();
  model_->HardRecover();
  file_monitor_->HardRecover(
      base::Bind(&ControllerImpl::OnFileMonitorHardRecoverComplete,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ControllerImpl::PollActiveDriverDownloads() {
  DCHECK(controller_state_ == State::INITIALIZING ||
         controller_state_ == State::RECOVERING);

  std::set<std::string> guids = driver_->GetActiveDownloads();

  for (auto guid : guids) {
    if (!model_->Get(guid))
      externally_active_downloads_.insert(guid);
  }
}

void ControllerImpl::CancelOrphanedRequests() {
  DCHECK(controller_state_ == State::INITIALIZING ||
         controller_state_ == State::RECOVERING);

  auto entries = model_->PeekEntries();

  std::vector<std::string> guids_to_remove;
  std::set<base::FilePath> files_to_remove;
  for (auto* entry : entries) {
    if (!clients_->GetClient(entry->client)) {
      guids_to_remove.push_back(entry->guid);
      files_to_remove.insert(entry->target_file_path);
    }
  }

  for (const auto& guid : guids_to_remove) {
    driver_->Remove(guid);
    model_->Remove(guid);
  }

  file_monitor_->DeleteFiles(files_to_remove,
                             stats::FileCleanupReason::ORPHANED);
}

void ControllerImpl::CleanupUnknownFiles() {
  DCHECK(controller_state_ == State::INITIALIZING ||
         controller_state_ == State::RECOVERING);

  auto entries = model_->PeekEntries();
  std::vector<DriverEntry> driver_entries;
  for (auto* entry : entries) {
    base::Optional<DriverEntry> driver_entry = driver_->Find(entry->guid);
    if (driver_entry.has_value())
      driver_entries.push_back(driver_entry.value());
  }

  file_monitor_->DeleteUnknownFiles(entries, driver_entries);
}

void ControllerImpl::ResolveInitialRequestStates() {
  DCHECK(controller_state_ == State::INITIALIZING ||
         controller_state_ == State::RECOVERING);

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
      case Entry::State::COUNT:
        NOTREACHED();
        break;
    }
  }
}

void ControllerImpl::UpdateDriverStates() {
  DCHECK(startup_status_.Complete());

  for (auto* entry : model_->PeekEntries())
    UpdateDriverState(entry);
}

void ControllerImpl::UpdateDriverStateWithGuid(const std::string& guid) {
  Entry* entry = model_->Get(guid);
  if (entry)
    UpdateDriverState(entry);
}

void ControllerImpl::UpdateDriverState(Entry* entry) {
  DCHECK_EQ(controller_state_, State::READY);

  if (entry->state != Entry::State::ACTIVE &&
      entry->state != Entry::State::PAUSED) {
    return;
  }

  // This method will need to figure out what to do with a failed download and
  // either a) restart it or b) fail the download.

  base::Optional<DriverEntry> driver_entry = driver_->Find(entry->guid);

  bool meets_device_criteria = device_status_listener_->CurrentDeviceStatus()
                                   .MeetsCondition(entry->scheduling_params)
                                   .MeetsRequirements();
  bool force_pause =
      !externally_active_downloads_.empty() &&
      entry->scheduling_params.priority != SchedulingParams::Priority::UI;
  bool entry_paused = entry->state == Entry::State::PAUSED;

  bool pause_driver = entry_paused || force_pause || !meets_device_criteria;

  if (pause_driver) {
    if (driver_entry.has_value())
      driver_->Pause(entry->guid);
  } else {
    bool is_new_attempt =
        !driver_entry.has_value() ||
        driver_entry->state == DriverEntry::State::INTERRUPTED;
    if (is_new_attempt) {
      entry->attempt_count++;
      model_->Update(*entry);
      if (entry->attempt_count >= config_->max_retry_count) {
        HandleCompleteDownload(CompletionType::FAIL, entry->guid);
        return;
      }
    }

    if (driver_entry.has_value()) {
      driver_->Resume(entry->guid);
    } else {
      driver_->Start(
          entry->request_params, entry->guid, entry->target_file_path,
          net::NetworkTrafficAnnotationTag(entry->traffic_annotation));
    }
  }
}

void ControllerImpl::NotifyClientsOfStartup(bool state_lost) {
  auto categorized = util::MapEntriesToMetadataForClients(
      clients_->GetRegisteredClients(), model_->PeekEntries());

  for (auto client_id : clients_->GetRegisteredClients()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ControllerImpl::SendOnServiceInitialized,
                              weak_ptr_factory_.GetWeakPtr(), client_id,
                              state_lost, categorized[client_id]));
  }
}

void ControllerImpl::NotifyServiceOfStartup() {
  if (init_callback_.is_null())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::ResetAndReturn(&init_callback_));
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

  if (entry->state == Entry::State::COMPLETE) {
    DVLOG(1) << "Download is already completed.";
    return;
  }

  auto driver_entry = driver_->Find(guid);
  uint64_t file_size =
      driver_entry.has_value() ? driver_entry->bytes_downloaded : 0;
  stats::LogDownloadCompletion(type, file_size);

  if (type == CompletionType::SUCCEED) {
    DCHECK(driver_entry.has_value());
    stats::LogFilePathRenamed(driver_entry->current_file_path !=
                              entry->target_file_path);
    entry->target_file_path = driver_entry->current_file_path;
    entry->completion_time = driver_entry->completion_time;
    entry->bytes_downloaded = driver_entry->bytes_downloaded;
    CompletionInfo completion_info(driver_entry->current_file_path,
                                   driver_entry->bytes_downloaded);
    entry->last_cleanup_check_time = driver_entry->completion_time;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ControllerImpl::SendOnDownloadSucceeded,
                              weak_ptr_factory_.GetWeakPtr(), entry->client,
                              guid, completion_info));
    TransitTo(entry, Entry::State::COMPLETE, model_.get());
    ScheduleCleanupTask();
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ControllerImpl::SendOnDownloadFailed,
                              weak_ptr_factory_.GetWeakPtr(), entry->client,
                              guid, FailureReasonFromCompletionType(type)));
    // TODO(dtrainor): Handle the case where we crash before the model write
    // happens and we have no driver entry.
    driver_->Remove(entry->guid);
    model_->Remove(guid);
  }

  ActivateMoreDownloads();
}

void ControllerImpl::ScheduleCleanupTask() {
  base::Time earliest_cleanup_start_time = base::Time::Max();
  for (const Entry* entry : model_->PeekEntries()) {
    if (entry->state != Entry::State::COMPLETE)
      continue;

    base::Time cleanup_time_for_entry =
        std::min(entry->last_cleanup_check_time + config_->file_keep_alive_time,
                 entry->completion_time + config_->max_file_keep_alive_time);
    cleanup_time_for_entry =
        std::max(cleanup_time_for_entry, base::Time::Now());
    if (cleanup_time_for_entry < earliest_cleanup_start_time) {
      earliest_cleanup_start_time = cleanup_time_for_entry;
    }
  }

  if (earliest_cleanup_start_time == base::Time::Max())
    return;

  base::TimeDelta start_time = earliest_cleanup_start_time - base::Time::Now();
  base::TimeDelta end_time = start_time + config_->file_cleanup_window;
  DCHECK_LT(std::ceil(start_time.InSecondsF()),
            std::ceil(end_time.InSecondsF()))
      << "GCM requires start time to be less than end time";

  task_scheduler_->ScheduleTask(DownloadTaskType::CLEANUP_TASK, false, false,
                                std::ceil(start_time.InSecondsF()),
                                std::ceil(end_time.InSecondsF()));
}

void ControllerImpl::ScheduleKillDownloadTaskIfNecessary() {
  base::Time earliest_cancel_time = base::Time::Max();
  for (const Entry* entry : model_->PeekEntries()) {
    if (entry->state != Entry::State::COMPLETE &&
        entry->scheduling_params.cancel_time < earliest_cancel_time) {
      earliest_cancel_time = entry->scheduling_params.cancel_time;
    }
  }

  if (earliest_cancel_time == base::Time::Max())
    return;

  base::TimeDelta time_to_cancel =
      earliest_cancel_time > base::Time::Now()
          ? earliest_cancel_time - base::Time::Now()
          : base::TimeDelta();

  cancel_downloads_callback_.Reset(base::Bind(
      &ControllerImpl::KillTimedOutDownloads, weak_ptr_factory_.GetWeakPtr()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, cancel_downloads_callback_.callback(), time_to_cancel);
}

void ControllerImpl::KillTimedOutDownloads() {
  for (const Entry* entry : model_->PeekEntries()) {
    if (entry->state != Entry::State::COMPLETE &&
        entry->scheduling_params.cancel_time <= base::Time::Now()) {
      HandleCompleteDownload(CompletionType::TIMEOUT, entry->guid);
    }
  }

  ScheduleKillDownloadTaskIfNecessary();
}

void ControllerImpl::ActivateMoreDownloads() {
  if (controller_state_ != State::READY)
    return;

  // Check all the entries and the configuration to throttle number of
  // downloads.
  std::map<Entry::State, uint32_t> entries_states;
  Model::EntryList scheduling_candidates;
  for (auto* const entry : model_->PeekEntries()) {
    entries_states[entry->state]++;
    // Only schedule background tasks based on available and active entries.
    if (entry->state == Entry::State::AVAILABLE ||
        entry->state == Entry::State::ACTIVE) {
      scheduling_candidates.emplace_back(entry);
    }
  }

  uint32_t paused_count = entries_states[Entry::State::PAUSED];
  uint32_t active_count = entries_states[Entry::State::ACTIVE];

  bool has_actionable_downloads = false;
  while (CanActivateMoreDownloads(config_, active_count, paused_count)) {
    Entry* next = scheduler_->Next(
        model_->PeekEntries(), device_status_listener_->CurrentDeviceStatus());
    if (!next)
      break;

    has_actionable_downloads = true;
    DCHECK_EQ(Entry::State::AVAILABLE, next->state);
    TransitTo(next, Entry::State::ACTIVE, model_.get());
    active_count++;
    UpdateDriverState(next);
  }

  if (!has_actionable_downloads) {
    HandleTaskFinished(DownloadTaskType::DOWNLOAD_TASK, false,
                       stats::ScheduledTaskStatus::COMPLETED_NORMALLY);
  }

  scheduler_->Reschedule(scheduling_candidates);
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

void ControllerImpl::SendOnServiceInitialized(
    DownloadClient client_id,
    bool state_lost,
    const std::vector<DownloadMetaData>& downloads) {
  auto* client = clients_->GetClient(client_id);
  DCHECK(client);
  client->OnServiceInitialized(state_lost, downloads);
}

void ControllerImpl::SendOnServiceUnavailable() {
  for (auto client_id : clients_->GetRegisteredClients()) {
    clients_->GetClient(client_id)->OnServiceUnavailable();
  }
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

void ControllerImpl::SendOnDownloadSucceeded(
    DownloadClient client_id,
    const std::string& guid,
    const CompletionInfo& completion_info) {
  auto* client = clients_->GetClient(client_id);
  DCHECK(client);
  client->OnDownloadSucceeded(guid, completion_info);
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
