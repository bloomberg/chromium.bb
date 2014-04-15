// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/scheduler.h"

#include <algorithm>

#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/util.h"

namespace {

const int kInvalidCollectorIndex = -1;

const int kDefaultMinimumUploadDelaySec = 60;
const int kDefaultMaximumUploadDelaySec = 300;
const int kDefaultUploadRetryIntervalSec = 60;

const char* kMinimumUploadDelayFieldTrialName = "DomRel-MinimumUploadDelay";
const char* kMaximumUploadDelayFieldTrialName = "DomRel-MaximumUploadDelay";
const char* kUploadRetryIntervalFieldTrialName = "DomRel-UploadRetryInterval";

int GetIntegerFieldTrialValueOrDefault(
    std::string field_trial_name,
    int default_value) {
  if (!base::FieldTrialList::TrialExists(field_trial_name))
    return default_value;

  std::string group_name = base::FieldTrialList::FindFullName(field_trial_name);
  int value;
  if (!base::StringToInt(group_name, &value)) {
    LOG(ERROR) << "Expected integer for field trial " << field_trial_name
               << " group name, but got \"" << group_name << "\".";
    return default_value;
  }

  return value;
}

}  // namespace

namespace domain_reliability {

// static
DomainReliabilityScheduler::Params
DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults() {
  DomainReliabilityScheduler::Params params;

  params.minimum_upload_delay = base::TimeDelta::FromSeconds(
      GetIntegerFieldTrialValueOrDefault(kMinimumUploadDelayFieldTrialName,
                                         kDefaultMinimumUploadDelaySec));
  params.maximum_upload_delay = base::TimeDelta::FromSeconds(
      GetIntegerFieldTrialValueOrDefault(kMaximumUploadDelayFieldTrialName,
                                         kDefaultMaximumUploadDelaySec));
  params.upload_retry_interval = base::TimeDelta::FromSeconds(
      GetIntegerFieldTrialValueOrDefault(kUploadRetryIntervalFieldTrialName,
                                         kDefaultUploadRetryIntervalSec));

  return params;
}

DomainReliabilityScheduler::DomainReliabilityScheduler(
    MockableTime* time,
    size_t num_collectors,
    const Params& params,
    const ScheduleUploadCallback& callback)
    : time_(time),
      collectors_(num_collectors),
      params_(params),
      callback_(callback),
      upload_pending_(false),
      upload_scheduled_(false),
      upload_running_(false),
      collector_index_(kInvalidCollectorIndex) {
}

DomainReliabilityScheduler::~DomainReliabilityScheduler() {}

void DomainReliabilityScheduler::OnBeaconAdded() {
  if (!upload_pending_)
    first_beacon_time_ = time_->NowTicks();
  upload_pending_ = true;
  VLOG(2) << "OnBeaconAdded";
  MaybeScheduleUpload();
}

void DomainReliabilityScheduler::OnUploadStart(int* collector_index_out) {
  DCHECK(collector_index_out);
  DCHECK(upload_scheduled_);
  DCHECK_EQ(kInvalidCollectorIndex, collector_index_);
  upload_scheduled_ = false;
  upload_running_ = true;

  base::TimeTicks now = time_->NowTicks();
  base::TimeTicks min_upload_time;
  GetNextUploadTimeAndCollector(now, &min_upload_time, &collector_index_);
  DCHECK(min_upload_time <= now);

  *collector_index_out = collector_index_;

  VLOG(1) << "Starting upload to collector " << collector_index_ << ".";
}

void DomainReliabilityScheduler::OnUploadComplete(bool success) {
  DCHECK(upload_running_);
  DCHECK_NE(kInvalidCollectorIndex, collector_index_);
  upload_running_ = false;

  VLOG(1) << "Upload to collector " << collector_index_
          << (success ? " succeeded." : " failed.");

  CollectorState* collector = &collectors_[collector_index_];
  collector_index_ = kInvalidCollectorIndex;

  if (success) {
    collector->failures = 0;
  } else {
    // Restore upload_pending_ and first_beacon_time_ to pre-upload state,
    // since upload failed.
    upload_pending_ = true;
    first_beacon_time_ = old_first_beacon_time_;

    ++collector->failures;
  }

  base::TimeDelta retry_interval = GetUploadRetryInterval(collector->failures);
  collector->next_upload = time_->NowTicks() + retry_interval;

  VLOG(1) << "Next upload to collector at least "
          << retry_interval.InSeconds() << " seconds from now.";

  MaybeScheduleUpload();
}

DomainReliabilityScheduler::CollectorState::CollectorState() : failures(0) {}

void DomainReliabilityScheduler::MaybeScheduleUpload() {
  if (!upload_pending_ || upload_scheduled_ || upload_running_)
    return;

  upload_pending_ = false;
  upload_scheduled_ = true;
  old_first_beacon_time_ = first_beacon_time_;

  base::TimeTicks now = time_->NowTicks();

  base::TimeTicks min_by_deadline, max_by_deadline;
  min_by_deadline = first_beacon_time_ + params_.minimum_upload_delay;
  max_by_deadline = first_beacon_time_ + params_.maximum_upload_delay;
  DCHECK(min_by_deadline <= max_by_deadline);

  base::TimeTicks min_by_backoff;
  int collector_index;
  GetNextUploadTimeAndCollector(now, &min_by_backoff, &collector_index);

  base::TimeDelta min_delay = std::max(min_by_deadline, min_by_backoff) - now;
  base::TimeDelta max_delay = std::max(max_by_deadline, min_by_backoff) - now;

  VLOG(1) << "Scheduling upload for between " << min_delay.InSeconds()
          << " and " << max_delay.InSeconds() << " seconds from now.";

  callback_.Run(min_delay, max_delay);
}

// TODO(ttuttle): Add min and max interval to config, use that instead.
// TODO(ttuttle): Cap min and max intervals received from config.

void DomainReliabilityScheduler::GetNextUploadTimeAndCollector(
    base::TimeTicks now,
    base::TimeTicks* upload_time_out,
    int* collector_index_out) {
  DCHECK(upload_time_out);
  DCHECK(collector_index_out);

  base::TimeTicks min_time;
  int min_index = kInvalidCollectorIndex;

  for (unsigned i = 0; i < collectors_.size(); ++i) {
    CollectorState* collector = &collectors_[i];
    // If a collector is usable, use the first one in the list.
    if (collector->failures == 0 || collector->next_upload <= now) {
      min_time = now;
      min_index = i;
      break;
    // If not, keep track of which will be usable soonest:
    } else if (min_index == kInvalidCollectorIndex ||
        collector->next_upload < min_time) {
      min_time = collector->next_upload;
      min_index = i;
    }
  }

  DCHECK_NE(kInvalidCollectorIndex, min_index);
  *upload_time_out = min_time;
  *collector_index_out = min_index;
}

base::TimeDelta DomainReliabilityScheduler::GetUploadRetryInterval(
    int failures) {
  if (failures == 0)
    return base::TimeDelta::FromSeconds(0);
  else {
    return params_.upload_retry_interval * (1 << std::min(failures - 1, 5));
  }
}

}  // namespace domain_reliability
