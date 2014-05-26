// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_log.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/cpu.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/profiler/alternate_timer.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/third_party/nspr/prtime.h"
#include "base/time/time.h"
#include "base/tracked_objects.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/proto/profiler_event.pb.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "components/nacl/common/nacl_process_type.h"
#include "components/variations/active_field_trials.h"
#include "content/public/common/content_client.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_WIN)
#include "base/win/metro.h"

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

using metrics::MetricsLogBase;
using metrics::ProfilerEventProto;
using metrics::SystemProfileProto;
using tracked_objects::ProcessDataSnapshot;
typedef variations::ActiveGroupId ActiveGroupId;

namespace {

// Returns the date at which the current metrics client ID was created as
// a string containing seconds since the epoch, or "0" if none was found.
std::string GetMetricsEnabledDate(PrefService* pref) {
  if (!pref) {
    NOTREACHED();
    return "0";
  }

  return pref->GetString(prefs::kMetricsReportingEnabledTimestamp);
}

ProfilerEventProto::TrackedObject::ProcessType AsProtobufProcessType(
    int process_type) {
  switch (process_type) {
    case content::PROCESS_TYPE_BROWSER:
      return ProfilerEventProto::TrackedObject::BROWSER;
    case content::PROCESS_TYPE_RENDERER:
      return ProfilerEventProto::TrackedObject::RENDERER;
    case content::PROCESS_TYPE_PLUGIN:
      return ProfilerEventProto::TrackedObject::PLUGIN;
    case content::PROCESS_TYPE_WORKER:
      return ProfilerEventProto::TrackedObject::WORKER;
    case content::PROCESS_TYPE_UTILITY:
      return ProfilerEventProto::TrackedObject::UTILITY;
    case content::PROCESS_TYPE_ZYGOTE:
      return ProfilerEventProto::TrackedObject::ZYGOTE;
    case content::PROCESS_TYPE_SANDBOX_HELPER:
      return ProfilerEventProto::TrackedObject::SANDBOX_HELPER;
    case content::PROCESS_TYPE_GPU:
      return ProfilerEventProto::TrackedObject::GPU;
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
      return ProfilerEventProto::TrackedObject::PPAPI_PLUGIN;
    case content::PROCESS_TYPE_PPAPI_BROKER:
      return ProfilerEventProto::TrackedObject::PPAPI_BROKER;
    case PROCESS_TYPE_NACL_LOADER:
      return ProfilerEventProto::TrackedObject::NACL_LOADER;
    case PROCESS_TYPE_NACL_BROKER:
      return ProfilerEventProto::TrackedObject::NACL_BROKER;
    default:
      NOTREACHED();
      return ProfilerEventProto::TrackedObject::UNKNOWN;
  }
}

// Computes a SHA-1 hash of |data| and returns it as a hex string.
std::string ComputeSHA1(const std::string& data) {
  const std::string sha1 = base::SHA1HashString(data);
  return base::HexEncode(sha1.data(), sha1.size());
}

void WriteFieldTrials(const std::vector<ActiveGroupId>& field_trial_ids,
                      SystemProfileProto* system_profile) {
  for (std::vector<ActiveGroupId>::const_iterator it =
       field_trial_ids.begin(); it != field_trial_ids.end(); ++it) {
    SystemProfileProto::FieldTrial* field_trial =
        system_profile->add_field_trial();
    field_trial->set_name_id(it->name);
    field_trial->set_group_id(it->group);
  }
}

// Maps a thread name by replacing trailing sequence of digits with "*".
// Examples:
// 1. "BrowserBlockingWorker1/23857" => "BrowserBlockingWorker1/*"
// 2. "Chrome_IOThread" => "Chrome_IOThread"
std::string MapThreadName(const std::string& thread_name) {
  size_t i = thread_name.length();

  while (i > 0 && isdigit(thread_name[i - 1])) {
    --i;
  }

  if (i == thread_name.length())
    return thread_name;

  return thread_name.substr(0, i) + '*';
}

// Normalizes a source filename (which is platform- and build-method-dependent)
// by extracting the last component of the full file name.
// Example: "c:\b\build\slave\win\build\src\chrome\app\chrome_main.cc" =>
// "chrome_main.cc".
std::string NormalizeFileName(const std::string& file_name) {
  const size_t offset = file_name.find_last_of("\\/");
  return offset != std::string::npos ? file_name.substr(offset + 1) : file_name;
}

void WriteProfilerData(const ProcessDataSnapshot& profiler_data,
                       int process_type,
                       ProfilerEventProto* performance_profile) {
  for (std::vector<tracked_objects::TaskSnapshot>::const_iterator it =
           profiler_data.tasks.begin();
       it != profiler_data.tasks.end(); ++it) {
    const tracked_objects::DeathDataSnapshot& death_data = it->death_data;
    ProfilerEventProto::TrackedObject* tracked_object =
        performance_profile->add_tracked_object();
    tracked_object->set_birth_thread_name_hash(
        MetricsLogBase::Hash(MapThreadName(it->birth.thread_name)));
    tracked_object->set_exec_thread_name_hash(
        MetricsLogBase::Hash(MapThreadName(it->death_thread_name)));
    tracked_object->set_source_file_name_hash(
        MetricsLogBase::Hash(NormalizeFileName(it->birth.location.file_name)));
    tracked_object->set_source_function_name_hash(
        MetricsLogBase::Hash(it->birth.location.function_name));
    tracked_object->set_source_line_number(it->birth.location.line_number);
    tracked_object->set_exec_count(death_data.count);
    tracked_object->set_exec_time_total(death_data.run_duration_sum);
    tracked_object->set_exec_time_sampled(death_data.run_duration_sample);
    tracked_object->set_queue_time_total(death_data.queue_duration_sum);
    tracked_object->set_queue_time_sampled(death_data.queue_duration_sample);
    tracked_object->set_process_type(AsProtobufProcessType(process_type));
    tracked_object->set_process_id(profiler_data.process_id);
  }
}

// Round a timestamp measured in seconds since epoch to one with a granularity
// of an hour. This can be used before uploaded potentially sensitive
// timestamps.
int64 RoundSecondsToHour(int64 time_in_seconds) {
  return 3600 * (time_in_seconds / 3600);
}

}  // namespace

MetricsLog::MetricsLog(const std::string& client_id,
                       int session_id,
                       LogType log_type,
                       metrics::MetricsServiceClient* client,
                       PrefService* local_state)
    : MetricsLogBase(client_id,
                     session_id,
                     log_type,
                     client->GetVersionString()),
      client_(client),
      creation_time_(base::TimeTicks::Now()),
      local_state_(local_state) {
  uma_proto()->mutable_system_profile()->set_channel(client_->GetChannel());
}

MetricsLog::~MetricsLog() {}

void MetricsLog::RecordStabilityMetrics(
    const std::vector<metrics::MetricsProvider*>& metrics_providers,
    base::TimeDelta incremental_uptime,
    base::TimeDelta uptime) {
  DCHECK(!locked());
  DCHECK(HasEnvironment());
  DCHECK(!HasStabilityMetrics());

  PrefService* pref = local_state_;
  DCHECK(pref);

  // Get stability attributes out of Local State, zeroing out stored values.
  // NOTE: This could lead to some data loss if this report isn't successfully
  //       sent, but that's true for all the metrics.

  WriteRequiredStabilityAttributes(pref);

  // Record recent delta for critical stability metrics.  We can't wait for a
  // restart to gather these, as that delay biases our observation away from
  // users that run happily for a looooong time.  We send increments with each
  // uma log upload, just as we send histogram data.
  WriteRealtimeStabilityAttributes(pref, incremental_uptime, uptime);

  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();
  for (size_t i = 0; i < metrics_providers.size(); ++i)
    metrics_providers[i]->ProvideStabilityMetrics(system_profile);

  // Omit some stats unless this is the initial stability log.
  if (log_type() != INITIAL_STABILITY_LOG)
    return;

  int incomplete_shutdown_count =
      pref->GetInteger(prefs::kStabilityIncompleteSessionEndCount);
  pref->SetInteger(prefs::kStabilityIncompleteSessionEndCount, 0);
  int breakpad_registration_success_count =
      pref->GetInteger(prefs::kStabilityBreakpadRegistrationSuccess);
  pref->SetInteger(prefs::kStabilityBreakpadRegistrationSuccess, 0);
  int breakpad_registration_failure_count =
      pref->GetInteger(prefs::kStabilityBreakpadRegistrationFail);
  pref->SetInteger(prefs::kStabilityBreakpadRegistrationFail, 0);
  int debugger_present_count =
      pref->GetInteger(prefs::kStabilityDebuggerPresent);
  pref->SetInteger(prefs::kStabilityDebuggerPresent, 0);
  int debugger_not_present_count =
      pref->GetInteger(prefs::kStabilityDebuggerNotPresent);
  pref->SetInteger(prefs::kStabilityDebuggerNotPresent, 0);

  // TODO(jar): The following are all optional, so we *could* optimize them for
  // values of zero (and not include them).
  SystemProfileProto::Stability* stability =
      system_profile->mutable_stability();
  stability->set_incomplete_shutdown_count(incomplete_shutdown_count);
  stability->set_breakpad_registration_success_count(
      breakpad_registration_success_count);
  stability->set_breakpad_registration_failure_count(
      breakpad_registration_failure_count);
  stability->set_debugger_present_count(debugger_present_count);
  stability->set_debugger_not_present_count(debugger_not_present_count);
}

void MetricsLog::RecordGeneralMetrics(
    const std::vector<metrics::MetricsProvider*>& metrics_providers) {
  for (size_t i = 0; i < metrics_providers.size(); ++i)
    metrics_providers[i]->ProvideGeneralMetrics(uma_proto());
}

void MetricsLog::GetFieldTrialIds(
    std::vector<ActiveGroupId>* field_trial_ids) const {
  variations::GetFieldTrialActiveGroupIds(field_trial_ids);
}

bool MetricsLog::HasEnvironment() const {
  return uma_proto()->system_profile().has_uma_enabled_date();
}

bool MetricsLog::HasStabilityMetrics() const {
  return uma_proto()->system_profile().stability().has_launch_count();
}

// The server refuses data that doesn't have certain values.  crashcount and
// launchcount are currently "required" in the "stability" group.
// TODO(isherman): Stop writing these attributes specially once the migration to
// protobufs is complete.
void MetricsLog::WriteRequiredStabilityAttributes(PrefService* pref) {
  int launch_count = pref->GetInteger(prefs::kStabilityLaunchCount);
  pref->SetInteger(prefs::kStabilityLaunchCount, 0);
  int crash_count = pref->GetInteger(prefs::kStabilityCrashCount);
  pref->SetInteger(prefs::kStabilityCrashCount, 0);

  SystemProfileProto::Stability* stability =
      uma_proto()->mutable_system_profile()->mutable_stability();
  stability->set_launch_count(launch_count);
  stability->set_crash_count(crash_count);
}

void MetricsLog::WriteRealtimeStabilityAttributes(
    PrefService* pref,
    base::TimeDelta incremental_uptime,
    base::TimeDelta uptime) {
  // Update the stats which are critical for real-time stability monitoring.
  // Since these are "optional," only list ones that are non-zero, as the counts
  // are aggregated (summed) server side.

  SystemProfileProto::Stability* stability =
      uma_proto()->mutable_system_profile()->mutable_stability();

  int count = pref->GetInteger(prefs::kStabilityChildProcessCrashCount);
  if (count) {
    stability->set_child_process_crash_count(count);
    pref->SetInteger(prefs::kStabilityChildProcessCrashCount, 0);
  }

  const uint64 incremental_uptime_sec = incremental_uptime.InSeconds();
  if (incremental_uptime_sec)
    stability->set_incremental_uptime_sec(incremental_uptime_sec);
  const uint64 uptime_sec = uptime.InSeconds();
  if (uptime_sec)
    stability->set_uptime_sec(uptime_sec);
}

void MetricsLog::RecordEnvironment(
    const std::vector<metrics::MetricsProvider*>& metrics_providers,
    const std::vector<variations::ActiveGroupId>& synthetic_trials) {
  DCHECK(!HasEnvironment());

  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();

  std::string brand_code;
  if (client_->GetBrand(&brand_code))
    system_profile->set_brand_code(brand_code);

  int enabled_date;
  bool success =
      base::StringToInt(GetMetricsEnabledDate(local_state_), &enabled_date);
  DCHECK(success);

  // Reduce granularity of the enabled_date field to nearest hour.
  system_profile->set_uma_enabled_date(RoundSecondsToHour(enabled_date));

  int64 install_date = local_state_->GetInt64(prefs::kInstallDate);

  // Reduce granularity of the install_date field to nearest hour.
  system_profile->set_install_date(RoundSecondsToHour(install_date));

  system_profile->set_application_locale(client_->GetApplicationLocale());

  SystemProfileProto::Hardware* hardware = system_profile->mutable_hardware();
  hardware->set_cpu_architecture(base::SysInfo::OperatingSystemArchitecture());
  hardware->set_system_ram_mb(base::SysInfo::AmountOfPhysicalMemoryMB());
#if defined(OS_WIN)
  hardware->set_dll_base(reinterpret_cast<uint64>(&__ImageBase));
#endif

  SystemProfileProto::OS* os = system_profile->mutable_os();
  std::string os_name = base::SysInfo::OperatingSystemName();
#if defined(OS_WIN)
  // TODO(mad): This only checks whether the main process is a Metro process at
  // upload time; not whether the collected metrics were all gathered from
  // Metro.  This is ok as an approximation for now, since users will rarely be
  // switching from Metro to Desktop mode; but we should re-evaluate whether we
  // can distinguish metrics more cleanly in the future: http://crbug.com/140568
  if (base::win::IsMetroProcess())
    os_name += " (Metro)";
#endif
  os->set_name(os_name);
  os->set_version(base::SysInfo::OperatingSystemVersion());
#if defined(OS_ANDROID)
  os->set_fingerprint(
      base::android::BuildInfo::GetInstance()->android_build_fp());
#endif

  base::CPU cpu_info;
  SystemProfileProto::Hardware::CPU* cpu = hardware->mutable_cpu();
  cpu->set_vendor_name(cpu_info.vendor_name());
  cpu->set_signature(cpu_info.signature());

  std::vector<ActiveGroupId> field_trial_ids;
  GetFieldTrialIds(&field_trial_ids);
  WriteFieldTrials(field_trial_ids, system_profile);
  WriteFieldTrials(synthetic_trials, system_profile);

  for (size_t i = 0; i < metrics_providers.size(); ++i)
    metrics_providers[i]->ProvideSystemProfileMetrics(system_profile);

  std::string serialied_system_profile;
  std::string base64_system_profile;
  if (system_profile->SerializeToString(&serialied_system_profile)) {
    base::Base64Encode(serialied_system_profile, &base64_system_profile);
    PrefService* local_state = local_state_;
    local_state->SetString(prefs::kStabilitySavedSystemProfile,
                           base64_system_profile);
    local_state->SetString(prefs::kStabilitySavedSystemProfileHash,
                           ComputeSHA1(serialied_system_profile));
  }
}

bool MetricsLog::LoadSavedEnvironmentFromPrefs() {
  PrefService* local_state = local_state_;
  const std::string base64_system_profile =
      local_state->GetString(prefs::kStabilitySavedSystemProfile);
  if (base64_system_profile.empty())
    return false;

  const std::string system_profile_hash =
      local_state->GetString(prefs::kStabilitySavedSystemProfileHash);
  local_state->ClearPref(prefs::kStabilitySavedSystemProfile);
  local_state->ClearPref(prefs::kStabilitySavedSystemProfileHash);

  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();
  std::string serialied_system_profile;
  return base::Base64Decode(base64_system_profile, &serialied_system_profile) &&
         ComputeSHA1(serialied_system_profile) == system_profile_hash &&
         system_profile->ParseFromString(serialied_system_profile);
}

void MetricsLog::RecordProfilerData(
    const tracked_objects::ProcessDataSnapshot& process_data,
    int process_type) {
  DCHECK(!locked());

  if (tracked_objects::GetTimeSourceType() !=
          tracked_objects::TIME_SOURCE_TYPE_WALL_TIME) {
    // We currently only support the default time source, wall clock time.
    return;
  }

  ProfilerEventProto* profile;
  if (!uma_proto()->profiler_event_size()) {
    // For the first process's data, add a new field to the protocol buffer.
    profile = uma_proto()->add_profiler_event();
    profile->set_profile_type(ProfilerEventProto::STARTUP_PROFILE);
    profile->set_time_source(ProfilerEventProto::WALL_CLOCK_TIME);
  } else {
    // For the remaining calls, re-use the existing field.
    profile = uma_proto()->mutable_profiler_event(0);
  }

  WriteProfilerData(process_data, process_type, profile);
}
