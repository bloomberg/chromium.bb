// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_metrics_provider.h"

#include <cstring>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/metrics/metrics_hashes.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"

using base::StackSamplingProfiler;

namespace metrics {

namespace {

// Accepts and ignores the completed profiles. Used when metrics reporting is
// disabled.
void IgnoreCompletedProfiles(
    const StackSamplingProfiler::CallStackProfiles& profiles) {
}

// The protobuf expects the MD5 checksum prefix of the module name.
uint64 HashModuleFilename(const base::FilePath& filename) {
  const base::FilePath::StringType basename = filename.BaseName().value();
  // Copy the bytes in basename into a string buffer.
  size_t basename_length_in_bytes =
      basename.size() * sizeof(base::FilePath::CharType);
  std::string name_bytes(basename_length_in_bytes, '\0');
  memcpy(&name_bytes[0], &basename[0], basename_length_in_bytes);
  return HashMetricName(name_bytes);
}

// Transcode |sample| into |proto_sample|, using base addresses in |modules| to
// compute module instruction pointer offsets.
void CopySampleToProto(
    const StackSamplingProfiler::Sample& sample,
    const std::vector<StackSamplingProfiler::Module>& modules,
    CallStackProfile::Sample* proto_sample) {
  for (const StackSamplingProfiler::Frame& frame : sample) {
    CallStackProfile::Entry* entry = proto_sample->add_entry();
    // A frame may not have a valid module. If so, we can't compute the
    // instruction pointer offset, and we don't want to send bare pointers, so
    // leave call_stack_entry empty.
    if (frame.module_index == StackSamplingProfiler::Frame::kUnknownModuleIndex)
      continue;
    int64 module_offset =
        reinterpret_cast<const char*>(frame.instruction_pointer) -
        reinterpret_cast<const char*>(modules[frame.module_index].base_address);
    DCHECK_GE(module_offset, 0);
    entry->set_address(static_cast<uint64>(module_offset));
    entry->set_module_id_index(frame.module_index);
  }
}

// Transcode |profile| into |proto_profile|.
void CopyProfileToProto(
    const StackSamplingProfiler::CallStackProfile& profile,
    CallStackProfile* proto_profile) {
  if (profile.samples.empty())
    return;

  if (profile.preserve_sample_ordering) {
    // Collapse only consecutive repeated samples together.
    CallStackProfile::Sample* current_sample_proto = nullptr;
    for (auto it = profile.samples.begin(); it != profile.samples.end(); ++it) {
      if (!current_sample_proto || *it != *(it - 1)) {
        current_sample_proto = proto_profile->add_sample();
        CopySampleToProto(*it, profile.modules, current_sample_proto);
        current_sample_proto->set_count(1);
      } else {
        current_sample_proto->set_count(current_sample_proto->count() + 1);
      }
    }
  } else {
    // Collapse all repeated samples together.
    std::map<StackSamplingProfiler::Sample, int> sample_index;
    for (auto it = profile.samples.begin(); it != profile.samples.end(); ++it) {
      auto location = sample_index.find(*it);
      if (location == sample_index.end()) {
        CallStackProfile::Sample* sample_proto = proto_profile->add_sample();
        CopySampleToProto(*it, profile.modules, sample_proto);
        sample_proto->set_count(1);
        sample_index.insert(
            std::make_pair(
                *it, static_cast<int>(proto_profile->sample().size()) - 1));
      } else {
        CallStackProfile::Sample* sample_proto =
            proto_profile->mutable_sample()->Mutable(location->second);
        sample_proto->set_count(sample_proto->count() + 1);
      }
    }
  }

  for (const StackSamplingProfiler::Module& module : profile.modules) {
    CallStackProfile::ModuleIdentifier* module_id =
        proto_profile->add_module_id();
    module_id->set_build_id(module.id);
    module_id->set_name_md5_prefix(HashModuleFilename(module.filename));
  }

  proto_profile->set_profile_duration_ms(
      profile.profile_duration.InMilliseconds());
  proto_profile->set_sampling_period_ms(
      profile.sampling_period.InMilliseconds());
}

// Translates CallStackProfileMetricsProvider's trigger to the corresponding
// SampledProfile TriggerEvent.
SampledProfile::TriggerEvent ToSampledProfileTriggerEvent(
    CallStackProfileMetricsProvider::Trigger trigger) {
  switch (trigger) {
    case CallStackProfileMetricsProvider::UNKNOWN:
      return SampledProfile::UNKNOWN_TRIGGER_EVENT;
      break;
    case CallStackProfileMetricsProvider::PROCESS_STARTUP:
      return SampledProfile::PROCESS_STARTUP;
      break;
    case CallStackProfileMetricsProvider::JANKY_TASK:
      return SampledProfile::JANKY_TASK;
      break;
    case CallStackProfileMetricsProvider::THREAD_HUNG:
      return SampledProfile::THREAD_HUNG;
      break;
  }
  NOTREACHED();
  return SampledProfile::UNKNOWN_TRIGGER_EVENT;
}

}  // namespace

const char CallStackProfileMetricsProvider::kFieldTrialName[] =
    "StackProfiling";
const char CallStackProfileMetricsProvider::kReportProfilesGroupName[] =
    "Report profiles";

CallStackProfileMetricsProvider::CallStackProfileMetricsProvider()
    : weak_factory_(this) {
}

CallStackProfileMetricsProvider::~CallStackProfileMetricsProvider() {
  StackSamplingProfiler::SetDefaultCompletedCallback(
      StackSamplingProfiler::CompletedCallback());
}

void CallStackProfileMetricsProvider::OnRecordingEnabled() {
  StackSamplingProfiler::SetDefaultCompletedCallback(base::Bind(
      &CallStackProfileMetricsProvider::ReceiveCompletedProfiles,
      base::ThreadTaskRunnerHandle::Get(), weak_factory_.GetWeakPtr()));
}

void CallStackProfileMetricsProvider::OnRecordingDisabled() {
  StackSamplingProfiler::SetDefaultCompletedCallback(
      base::Bind(&IgnoreCompletedProfiles));
  pending_profiles_.clear();
}

void CallStackProfileMetricsProvider::ProvideGeneralMetrics(
    ChromeUserMetricsExtension* uma_proto) {
  DCHECK(IsSamplingProfilingReportingEnabled() || pending_profiles_.empty());
  for (const StackSamplingProfiler::CallStackProfile& profile :
       pending_profiles_) {
    SampledProfile* sampled_profile = uma_proto->add_sampled_profile();
    sampled_profile->set_trigger_event(ToSampledProfileTriggerEvent(
        static_cast<CallStackProfileMetricsProvider::Trigger>(
            profile.user_data)));
    CopyProfileToProto(profile, sampled_profile->mutable_call_stack_profile());
  }
  pending_profiles_.clear();
}

void CallStackProfileMetricsProvider::AppendSourceProfilesForTesting(
    const std::vector<StackSamplingProfiler::CallStackProfile>& profiles) {
  AppendCompletedProfiles(profiles);
}

// static
bool CallStackProfileMetricsProvider::IsSamplingProfilingReportingEnabled() {
  const std::string group_name = base::FieldTrialList::FindFullName(
      CallStackProfileMetricsProvider::kFieldTrialName);
  return group_name ==
      CallStackProfileMetricsProvider::kReportProfilesGroupName;
}

// static
// Posts a message back to our own thread to collect the profiles.
void CallStackProfileMetricsProvider::ReceiveCompletedProfiles(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<CallStackProfileMetricsProvider> provider,
    const StackSamplingProfiler::CallStackProfiles& profiles) {
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&CallStackProfileMetricsProvider::AppendCompletedProfiles,
                 provider, profiles));
}

void CallStackProfileMetricsProvider::AppendCompletedProfiles(
    const StackSamplingProfiler::CallStackProfiles& profiles) {
  // Don't bother to record profiles if reporting is not enabled.
  if (IsSamplingProfilingReportingEnabled()) {
    pending_profiles_.insert(pending_profiles_.end(), profiles.begin(),
                             profiles.end());
  }
}

}  // namespace metrics
