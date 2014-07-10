// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/scheduler.h"

#include <algorithm>

#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/util.h"

namespace {

const unsigned kInvalidCollectorIndex = static_cast<unsigned>(-1);

const unsigned kDefaultMinimumUploadDelaySec = 60;
const unsigned kDefaultMaximumUploadDelaySec = 300;
const unsigned kDefaultUploadRetryIntervalSec = 60;

const char* kMinimumUploadDelayFieldTrialName = "DomRel-MinimumUploadDelay";
const char* kMaximumUploadDelayFieldTrialName = "DomRel-MaximumUploadDelay";
const char* kUploadRetryIntervalFieldTrialName = "DomRel-UploadRetryInterval";

unsigned GetUnsignedFieldTrialValueOrDefault(std::string field_trial_name,
                                             unsigned default_value) {
  if (!base::FieldTrialList::TrialExists(field_trial_name))
    return default_value;

  std::string group_name = base::FieldTrialList::FindFullName(field_trial_name);
  unsigned value;
  if (!base::StringToUint(group_name, &value)) {
    LOG(ERROR) << "Expected unsigned integer for field trial "
               << field_trial_name << " group name, but got \"" << group_name
               << "\".";
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

  params.minimum_upload_delay =
      base::TimeDelta::FromSeconds(GetUnsignedFieldTrialValueOrDefault(
          kMinimumUploadDelayFieldTrialName, kDefaultMinimumUploadDelaySec));
  params.maximum_upload_delay =
      base::TimeDelta::FromSeconds(GetUnsignedFieldTrialValueOrDefault(
          kMaximumUploadDelayFieldTrialName, kDefaultMaximumUploadDelaySec));
  params.upload_retry_interval =
      base::TimeDelta::FromSeconds(GetUnsignedFieldTrialValueOrDefault(
          kUploadRetryIntervalFieldTrialName, kDefaultUploadRetryIntervalSec));

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
      collector_index_(kInvalidCollectorIndex),
      last_upload_finished_(false) {
}

DomainReliabilityScheduler::~DomainReliabilityScheduler() {}

void DomainReliabilityScheduler::OnBeaconAdded() {
  if (!upload_pending_)
    first_beacon_time_ = time_->NowTicks();
  upload_pending_ = true;
  MaybeScheduleUpload();
}

size_t DomainReliabilityScheduler::OnUploadStart() {
  DCHECK(upload_scheduled_);
  DCHECK_EQ(kInvalidCollectorIndex, collector_index_);
  upload_pending_ = false;
  upload_scheduled_ = false;
  upload_running_ = true;

  base::TimeTicks now = time_->NowTicks();
  base::TimeTicks min_upload_time;
  GetNextUploadTimeAndCollector(now, &min_upload_time, &collector_index_);
  DCHECK(min_upload_time <= now);

  VLOG(1) << "Starting upload to collector " << collector_index_ << ".";

  last_upload_start_time_ = now;
  last_upload_collector_index_ = collector_index_;

  return collector_index_;
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

  base::TimeTicks now = time_->NowTicks();
  base::TimeDelta retry_interval = GetUploadRetryInterval(collector->failures);
  collector->next_upload = now + retry_interval;

  last_upload_end_time_ = now;
  last_upload_success_ = success;
  last_upload_finished_ = true;

  VLOG(1) << "Next upload to collector at least "
          << retry_interval.InSeconds() << " seconds from now.";

  MaybeScheduleUpload();
}

base::Value* DomainReliabilityScheduler::GetWebUIData() const {
  base::TimeTicks now = time_->NowTicks();

  base::DictionaryValue* data = new base::DictionaryValue();

  data->SetBoolean("upload_pending", upload_pending_);
  data->SetBoolean("upload_scheduled", upload_scheduled_);
  data->SetBoolean("upload_running", upload_running_);

  data->SetInteger("scheduled_min", (scheduled_min_time_ - now).InSeconds());
  data->SetInteger("scheduled_max", (scheduled_max_time_ - now).InSeconds());

  data->SetInteger("collector_index", static_cast<int>(collector_index_));

  if (last_upload_finished_) {
    base::DictionaryValue* last = new base::DictionaryValue();
    last->SetInteger("start_time", (now - last_upload_start_time_).InSeconds());
    last->SetInteger("end_time", (now - last_upload_end_time_).InSeconds());
    last->SetInteger("collector_index",
        static_cast<int>(last_upload_collector_index_));
    last->SetBoolean("success", last_upload_success_);
    data->Set("last_upload", last);
  }

  base::ListValue* collectors = new base::ListValue();
  for (size_t i = 0; i < collectors_.size(); ++i) {
    const CollectorState* state = &collectors_[i];
    base::DictionaryValue* value = new base::DictionaryValue();
    value->SetInteger("failures", state->failures);
    value->SetInteger("next_upload", (state->next_upload - now).InSeconds());
    collectors->Append(value);
  }
  data->Set("collectors", collectors);

  return data;
}

DomainReliabilityScheduler::CollectorState::CollectorState() : failures(0) {}

void DomainReliabilityScheduler::MaybeScheduleUpload() {
  if (!upload_pending_ || upload_scheduled_ || upload_running_)
    return;

  upload_scheduled_ = true;
  old_first_beacon_time_ = first_beacon_time_;

  base::TimeTicks now = time_->NowTicks();

  base::TimeTicks min_by_deadline, max_by_deadline;
  min_by_deadline = first_beacon_time_ + params_.minimum_upload_delay;
  max_by_deadline = first_beacon_time_ + params_.maximum_upload_delay;
  DCHECK(min_by_deadline <= max_by_deadline);

  base::TimeTicks min_by_backoff;
  size_t collector_index;
  GetNextUploadTimeAndCollector(now, &min_by_backoff, &collector_index);

  scheduled_min_time_ = std::max(min_by_deadline, min_by_backoff);
  scheduled_max_time_ = std::max(max_by_deadline, min_by_backoff);

  base::TimeDelta min_delay = scheduled_min_time_ - now;
  base::TimeDelta max_delay = scheduled_max_time_ - now;

  VLOG(1) << "Scheduling upload for between " << min_delay.InSeconds()
          << " and " << max_delay.InSeconds() << " seconds from now.";

  callback_.Run(min_delay, max_delay);
}

// TODO(ttuttle): Add min and max interval to config, use that instead.

// TODO(ttuttle): Cap min and max intervals received from config.

void DomainReliabilityScheduler::GetNextUploadTimeAndCollector(
    base::TimeTicks now,
    base::TimeTicks* upload_time_out,
    size_t* collector_index_out) {
  DCHECK(upload_time_out);
  DCHECK(collector_index_out);

  base::TimeTicks min_time;
  size_t min_index = kInvalidCollectorIndex;

  for (size_t i = 0; i < collectors_.size(); ++i) {
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
    unsigned failures) {
  if (failures == 0)
    return base::TimeDelta::FromSeconds(0);
  else {
    // Don't back off more than 64x the original delay.
    if (failures > 7)
      failures = 7;
    return params_.upload_retry_interval * (1 << (failures - 1));
  }
}

}  // namespace domain_reliability
