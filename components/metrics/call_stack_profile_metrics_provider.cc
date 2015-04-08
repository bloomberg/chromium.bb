// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_metrics_provider.h"

#include <cstring>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/field_trial.h"
#include "base/profiler/stack_sampling_profiler.h"
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
  StackSamplingProfiler::SetDefaultCompletedCallback(
      base::Bind(&CallStackProfileMetricsProvider::ReceiveCompletedProfiles,
                 base::MessageLoopProxy::current(),
                 weak_factory_.GetWeakPtr()));
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
    CallStackProfile* call_stack_profile =
        uma_proto->add_sampled_profile()->mutable_call_stack_profile();
    CopyProfileToProto(profile, call_stack_profile);
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
    scoped_refptr<base::MessageLoopProxy> message_loop,
    base::WeakPtr<CallStackProfileMetricsProvider> provider,
    const StackSamplingProfiler::CallStackProfiles& profiles) {
  message_loop->PostTask(
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
