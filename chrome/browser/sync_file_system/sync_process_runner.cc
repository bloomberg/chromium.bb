// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_process_runner.h"

#include "base/format_macros.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"

namespace sync_file_system {

namespace {

// Default delay when more changes are available.
const int64 kSyncDelayInMilliseconds = 1 * base::Time::kMillisecondsPerSecond;

// Default delay when the previous change has had an error (but remote service
// is running).
const int64 kSyncDelayWithSyncError = 3 * base::Time::kMillisecondsPerSecond;

// Default delay when there're more than 10 pending changes.
const int64 kSyncDelayFastInMilliseconds = 100;
const int kPendingChangeThresholdForFastSync = 10;

// Default delay when remote service is temporarily unavailable.
const int64 kSyncDelaySlowInMilliseconds =
    30 * base::Time::kMillisecondsPerSecond;  // Start with 30 sec + exp backoff

// Default delay when there're no changes.
const int64 kSyncDelayMaxInMilliseconds =
    30 * 60 * base::Time::kMillisecondsPerSecond;  // 30 min

bool WasSuccessfulSync(SyncStatusCode status) {
  return status == SYNC_STATUS_OK ||
         status == SYNC_STATUS_HAS_CONFLICT ||
         status == SYNC_STATUS_NO_CONFLICT ||
         status == SYNC_STATUS_NO_CHANGE_TO_SYNC ||
         status == SYNC_STATUS_UNKNOWN_ORIGIN ||
         status == SYNC_STATUS_RETRY;
}

}  // namespace

SyncProcessRunner::SyncProcessRunner(
    const std::string& name,
    SyncFileSystemService* sync_service)
    : name_(name),
      sync_service_(sync_service),
      current_delay_(0),
      last_delay_(0),
      pending_changes_(0),
      running_(false),
      factory_(this) {}

SyncProcessRunner::~SyncProcessRunner() {}

void SyncProcessRunner::Schedule() {
  int64 delay = kSyncDelayInMilliseconds;
  if (pending_changes_ == 0) {
    ScheduleInternal(kSyncDelayMaxInMilliseconds);
    return;
  }
  switch (GetServiceState()) {
    case SYNC_SERVICE_RUNNING:
      if (pending_changes_ > kPendingChangeThresholdForFastSync)
        delay = kSyncDelayFastInMilliseconds;
      else
        delay = kSyncDelayInMilliseconds;
      break;

    case SYNC_SERVICE_TEMPORARY_UNAVAILABLE:
      delay = kSyncDelaySlowInMilliseconds;
      if (last_delay_ >= kSyncDelaySlowInMilliseconds)
        delay = last_delay_ * 2;
      if (delay >= kSyncDelayMaxInMilliseconds)
        delay = kSyncDelayMaxInMilliseconds;
      break;

    case SYNC_SERVICE_AUTHENTICATION_REQUIRED:
    case SYNC_SERVICE_DISABLED:
      delay = kSyncDelayMaxInMilliseconds;
      break;
  }
  ScheduleInternal(delay);
}

void SyncProcessRunner::ScheduleIfNotRunning() {
  if (!timer_.IsRunning())
    Schedule();
}

void SyncProcessRunner::OnChangesUpdated(
    int64 pending_changes) {
  DCHECK_GE(pending_changes, 0);
  int64 old_pending_changes = pending_changes_;
  pending_changes_ = pending_changes;
  if (old_pending_changes != pending_changes) {
    if (pending_changes == 0)
      sync_service()->OnSyncIdle();
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[%s] pending_changes updated: %" PRId64,
              name_.c_str(), pending_changes);
  }
  Schedule();
}

SyncServiceState SyncProcessRunner::GetServiceState() {
  return sync_service()->GetSyncServiceState();
}

void SyncProcessRunner::Finished(SyncStatusCode status) {
  DCHECK(running_);
  running_ = false;
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "[%s] * Finished (elapsed: %" PRId64 " sec)",
            name_.c_str(),
            (base::Time::Now() - last_scheduled_).InSeconds());
  if (status == SYNC_STATUS_NO_CHANGE_TO_SYNC ||
      status == SYNC_STATUS_FILE_BUSY)
    ScheduleInternal(kSyncDelayMaxInMilliseconds);
  else if (!WasSuccessfulSync(status) &&
           GetServiceState() == SYNC_SERVICE_RUNNING)
    ScheduleInternal(kSyncDelayWithSyncError);
  else
    Schedule();
}

void SyncProcessRunner::Run() {
  if (running_)
    return;
  running_ = true;
  last_scheduled_ = base::Time::Now();
  last_delay_ = current_delay_;

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "[%s] * Started", name_.c_str());

  StartSync(
      base::Bind(&SyncProcessRunner::Finished, factory_.GetWeakPtr()));
}

void SyncProcessRunner::ScheduleInternal(int64 delay) {
  base::TimeDelta time_to_next = base::TimeDelta::FromMilliseconds(delay);

  if (timer_.IsRunning()) {
    if (current_delay_ == delay)
      return;

    base::TimeDelta elapsed = base::Time::Now() - last_scheduled_;
    if (elapsed < time_to_next) {
      time_to_next = time_to_next - elapsed;
    } else {
      time_to_next = base::TimeDelta::FromMilliseconds(
          kSyncDelayFastInMilliseconds);
    }
    timer_.Stop();
  }

  if (current_delay_ != delay) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[%s] Scheduling task in %" PRId64 " secs",
              name_.c_str(), time_to_next.InSeconds());
  }
  current_delay_ = delay;

  timer_.Start(FROM_HERE, time_to_next, this, &SyncProcessRunner::Run);
}

}  // namespace sync_file_system
