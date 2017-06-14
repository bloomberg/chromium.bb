// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "base/build_time.h"
#include "base/cpu.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/metrics_hashes.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/environment_recorder.h"
#include "components/metrics/histogram_encoder.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/metrics/proto/histogram_event.pb.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "components/metrics/proto/user_action_event.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/active_field_trials.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_WIN)
#include "base/win/current_module.h"
#endif

using base::SampleCountIterator;
typedef variations::ActiveGroupId ActiveGroupId;

namespace metrics {

namespace internal {
// Maximum number of events before truncation.
extern const int kOmniboxEventLimit = 5000;
extern const int kUserActionEventLimit = 5000;
}

namespace {

// A simple class to write histogram data to a log.
class IndependentFlattener : public base::HistogramFlattener {
 public:
  explicit IndependentFlattener(MetricsLog* log) : log_(log) {}

  // base::HistogramFlattener:
  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override {
    log_->RecordHistogramDelta(histogram.histogram_name(), snapshot);
  }
  void InconsistencyDetected(
      base::HistogramBase::Inconsistency problem) override {}
  void UniqueInconsistencyDetected(
      base::HistogramBase::Inconsistency problem) override {}
  void InconsistencyDetectedInLoggedCount(int amount) override {}

 private:
  MetricsLog* const log_;

  DISALLOW_COPY_AND_ASSIGN(IndependentFlattener);
};

// Any id less than 16 bytes is considered to be a testing id.
bool IsTestingID(const std::string& id) {
  return id.size() < 16;
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

// Round a timestamp measured in seconds since epoch to one with a granularity
// of an hour. This can be used before uploaded potentially sensitive
// timestamps.
int64_t RoundSecondsToHour(int64_t time_in_seconds) {
  return 3600 * (time_in_seconds / 3600);
}

}  // namespace

MetricsLog::MetricsLog(const std::string& client_id,
                       int session_id,
                       LogType log_type,
                       MetricsServiceClient* client,
                       PrefService* local_state)
    : closed_(false),
      log_type_(log_type),
      client_(client),
      creation_time_(base::TimeTicks::Now()),
      local_state_(local_state) {
  if (IsTestingID(client_id))
    uma_proto_.set_client_id(0);
  else
    uma_proto_.set_client_id(Hash(client_id));

  uma_proto_.set_session_id(session_id);

  const int32_t product = client_->GetProduct();
  // Only set the product if it differs from the default value.
  if (product != uma_proto_.product())
    uma_proto_.set_product(product);

  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();
  RecordCoreSystemProfile(client_, system_profile);
  if (log_type_ == ONGOING_LOG) {
    GlobalPersistentSystemProfile::GetInstance()->SetSystemProfile(
        *system_profile, /*complete=*/false);
  }
}

MetricsLog::~MetricsLog() {
}

// static
void MetricsLog::RegisterPrefs(PrefRegistrySimple* registry) {
  EnvironmentRecorder::RegisterPrefs(registry);
}

// static
uint64_t MetricsLog::Hash(const std::string& value) {
  uint64_t hash = base::HashMetricName(value);

  // The following log is VERY helpful when folks add some named histogram into
  // the code, but forgot to update the descriptive list of histograms.  When
  // that happens, all we get to see (server side) is a hash of the histogram
  // name.  We can then use this logging to find out what histogram name was
  // being hashed to a given MD5 value by just running the version of Chromium
  // in question with --enable-logging.
  DVLOG(1) << "Metrics: Hash numeric [" << value << "]=[" << hash << "]";

  return hash;
}

// static
int64_t MetricsLog::GetBuildTime() {
  static int64_t integral_build_time = 0;
  if (!integral_build_time)
    integral_build_time = static_cast<int64_t>(base::GetBuildTime().ToTimeT());
  return integral_build_time;
}

// static
int64_t MetricsLog::GetCurrentTime() {
  return (base::TimeTicks::Now() - base::TimeTicks()).InSeconds();
}

void MetricsLog::RecordUserAction(const std::string& key) {
  DCHECK(!closed_);

  UserActionEventProto* user_action = uma_proto_.add_user_action_event();
  user_action->set_name_hash(Hash(key));
  user_action->set_time(GetCurrentTime());
}

void MetricsLog::RecordCoreSystemProfile(MetricsServiceClient* client,
                                         SystemProfileProto* system_profile) {
  system_profile->set_build_timestamp(metrics::MetricsLog::GetBuildTime());
  system_profile->set_app_version(client->GetVersionString());
  system_profile->set_channel(client->GetChannel());
  system_profile->set_application_locale(client->GetApplicationLocale());

#if defined(SYZYASAN)
  system_profile->set_is_asan_build(true);
#endif

  metrics::SystemProfileProto::Hardware* hardware =
      system_profile->mutable_hardware();
#if !defined(OS_IOS)
  // On iOS, OperatingSystemArchitecture() returns values like iPad4,4 which is
  // not the actual CPU architecture. Don't set it until the API is fixed. See
  // crbug.com/370104 for details.
  hardware->set_cpu_architecture(base::SysInfo::OperatingSystemArchitecture());
#endif
  hardware->set_system_ram_mb(base::SysInfo::AmountOfPhysicalMemoryMB());
  hardware->set_hardware_class(base::SysInfo::HardwareModelName());
#if defined(OS_WIN)
  hardware->set_dll_base(reinterpret_cast<uint64_t>(CURRENT_MODULE()));
#endif

  metrics::SystemProfileProto::OS* os = system_profile->mutable_os();
  os->set_name(base::SysInfo::OperatingSystemName());
  os->set_version(base::SysInfo::OperatingSystemVersion());
#if defined(OS_ANDROID)
  os->set_fingerprint(
      base::android::BuildInfo::GetInstance()->android_build_fp());
#endif
}

void MetricsLog::RecordHistogramDelta(const std::string& histogram_name,
                                      const base::HistogramSamples& snapshot) {
  DCHECK(!closed_);
  EncodeHistogramDelta(histogram_name, snapshot, &uma_proto_);
}

void MetricsLog::RecordStabilityMetrics(
    const std::vector<std::unique_ptr<MetricsProvider>>& metrics_providers,
    base::TimeDelta incremental_uptime,
    base::TimeDelta uptime) {
  DCHECK(!closed_);
  DCHECK(HasEnvironment());
  DCHECK(!HasStabilityMetrics());

  // Record recent delta for critical stability metrics.  We can't wait for a
  // restart to gather these, as that delay biases our observation away from
  // users that run happily for a looooong time.  We send increments with each
  // uma log upload, just as we send histogram data.
  WriteRealtimeStabilityAttributes(incremental_uptime, uptime);

  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();
  for (size_t i = 0; i < metrics_providers.size(); ++i) {
    if (log_type() == INITIAL_STABILITY_LOG)
      metrics_providers[i]->ProvideInitialStabilityMetrics(system_profile);
    metrics_providers[i]->ProvideStabilityMetrics(system_profile);
  }
}

void MetricsLog::RecordGeneralMetrics(
    const std::vector<std::unique_ptr<MetricsProvider>>& metrics_providers) {
  if (local_state_->GetBoolean(prefs::kMetricsResetIds))
    UMA_HISTOGRAM_BOOLEAN("UMA.IsClonedInstall", true);

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

void MetricsLog::WriteMetricsEnableDefault(EnableMetricsDefault metrics_default,
                                           SystemProfileProto* system_profile) {
  if (client_->IsReportingPolicyManaged()) {
    // If it's managed, then it must be reporting, otherwise we wouldn't be
    // sending metrics.
    system_profile->set_uma_default_state(
        SystemProfileProto_UmaDefaultState_POLICY_FORCED_ENABLED);
    return;
  }

  switch (metrics_default) {
    case EnableMetricsDefault::DEFAULT_UNKNOWN:
      // Don't set the field if it's unknown.
      break;
    case EnableMetricsDefault::OPT_IN:
      system_profile->set_uma_default_state(
          SystemProfileProto_UmaDefaultState_OPT_IN);
      break;
    case EnableMetricsDefault::OPT_OUT:
      system_profile->set_uma_default_state(
          SystemProfileProto_UmaDefaultState_OPT_OUT);
  }
}

bool MetricsLog::HasStabilityMetrics() const {
  return uma_proto()->system_profile().stability().has_launch_count();
}

void MetricsLog::WriteRealtimeStabilityAttributes(
    base::TimeDelta incremental_uptime,
    base::TimeDelta uptime) {
  // Update the stats which are critical for real-time stability monitoring.
  // Since these are "optional," only list ones that are non-zero, as the counts
  // are aggregated (summed) server side.

  SystemProfileProto::Stability* stability =
      uma_proto()->mutable_system_profile()->mutable_stability();

  const uint64_t incremental_uptime_sec = incremental_uptime.InSeconds();
  if (incremental_uptime_sec)
    stability->set_incremental_uptime_sec(incremental_uptime_sec);
  const uint64_t uptime_sec = uptime.InSeconds();
  if (uptime_sec)
    stability->set_uptime_sec(uptime_sec);
}

std::string MetricsLog::RecordEnvironment(
    const std::vector<std::unique_ptr<MetricsProvider>>& metrics_providers,
    const std::vector<variations::ActiveGroupId>& synthetic_trials,
    int64_t install_date,
    int64_t metrics_reporting_enabled_date) {
  DCHECK(!HasEnvironment());

  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();

  WriteMetricsEnableDefault(client_->GetMetricsReportingDefaultState(),
                            system_profile);

  std::string brand_code;
  if (client_->GetBrand(&brand_code))
    system_profile->set_brand_code(brand_code);

  // Reduce granularity of the enabled_date field to nearest hour.
  system_profile->set_uma_enabled_date(
      RoundSecondsToHour(metrics_reporting_enabled_date));

  // Reduce granularity of the install_date field to nearest hour.
  system_profile->set_install_date(RoundSecondsToHour(install_date));

  SystemProfileProto::Hardware::CPU* cpu =
      system_profile->mutable_hardware()->mutable_cpu();
  base::CPU cpu_info;
  cpu->set_vendor_name(cpu_info.vendor_name());
  cpu->set_signature(cpu_info.signature());
  cpu->set_num_cores(base::SysInfo::NumberOfProcessors());

  std::vector<ActiveGroupId> field_trial_ids;
  GetFieldTrialIds(&field_trial_ids);
  WriteFieldTrials(field_trial_ids, system_profile);
  WriteFieldTrials(synthetic_trials, system_profile);

  for (size_t i = 0; i < metrics_providers.size(); ++i)
    metrics_providers[i]->ProvideSystemProfileMetrics(system_profile);

  EnvironmentRecorder recorder(local_state_);
  std::string serialized_proto =
      recorder.SerializeAndRecordEnvironmentToPrefs(*system_profile);

  if (log_type_ == ONGOING_LOG) {
    GlobalPersistentSystemProfile::GetInstance()->SetSystemProfile(
        serialized_proto, /*complete=*/true);
  }

  return serialized_proto;
}

bool MetricsLog::LoadIndependentMetrics(MetricsProvider* metrics_provider) {
  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();
  IndependentFlattener flattener(this);
  base::HistogramSnapshotManager snapshot_manager(&flattener);

  return metrics_provider->ProvideIndependentMetrics(system_profile,
                                                     &snapshot_manager);
}

bool MetricsLog::LoadSavedEnvironmentFromPrefs(std::string* app_version) {
  DCHECK(app_version);
  app_version->clear();

  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();
  EnvironmentRecorder recorder(local_state_);
  bool success = recorder.LoadEnvironmentFromPrefs(system_profile);
  if (success)
    *app_version = system_profile->app_version();
  return success;
}

void MetricsLog::CloseLog() {
  DCHECK(!closed_);
  closed_ = true;
}

void MetricsLog::TruncateEvents() {
  DCHECK(!closed_);
  if (uma_proto_.user_action_event_size() > internal::kUserActionEventLimit) {
    UMA_HISTOGRAM_COUNTS_100000("UMA.TruncatedEvents.UserAction",
                                uma_proto_.user_action_event_size());
    uma_proto_.mutable_user_action_event()->DeleteSubrange(
        internal::kUserActionEventLimit,
        uma_proto_.user_action_event_size() - internal::kUserActionEventLimit);
  }

  if (uma_proto_.omnibox_event_size() > internal::kOmniboxEventLimit) {
    UMA_HISTOGRAM_COUNTS_100000("UMA.TruncatedEvents.Omnibox",
                                uma_proto_.omnibox_event_size());
    uma_proto_.mutable_omnibox_event()->DeleteSubrange(
        internal::kOmniboxEventLimit,
        uma_proto_.omnibox_event_size() - internal::kOmniboxEventLimit);
  }
}

void MetricsLog::GetEncodedLog(std::string* encoded_log) {
  DCHECK(closed_);
  uma_proto_.SerializeToString(encoded_log);
}

}  // namespace metrics
