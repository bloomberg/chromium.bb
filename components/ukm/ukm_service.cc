// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/proto/ukm/entry.pb.h"
#include "components/metrics/proto/ukm/report.pb.h"
#include "components/metrics/proto/ukm/source.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/metrics_reporting_scheduler.h"
#include "components/ukm/persisted_logs_metrics_impl.h"
#include "components/ukm/ukm_entry.h"
#include "components/ukm/ukm_entry_builder.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/ukm/ukm_source.h"

namespace ukm {

namespace {

constexpr char kMimeType[] = "application/vnd.chrome.ukm";

// The delay, in seconds, after starting recording before doing expensive
// initialization work.
constexpr int kInitializationDelaySeconds = 5;

// The number of UKM logs that will be stored in PersistedLogs before logs
// start being dropped.
constexpr int kMinPersistedLogs = 8;

// The number of bytes UKM logs that will be stored in PersistedLogs before
// logs start being dropped.
// This ensures that a reasonable amount of history will be stored even if there
// is a long series of very small logs.
constexpr int kMinPersistedBytes = 300000;

// If an upload fails, and the transmission was over this byte count, then we
// will discard the log, and not try to retransmit it.  We also don't persist
// the log to the prefs for transmission during the next chrome session if this
// limit is exceeded.
constexpr size_t kMaxLogRetransmitSize = 100 * 1024;

// Gets the UKM Server URL.
std::string GetServerUrl() {
  constexpr char kDefaultServerUrl[] = "https://clients4.google.com/ukm";
  std::string server_url =
      base::GetFieldTrialParamValueByFeature(kUkmFeature, "ServerUrl");
  if (!server_url.empty())
    return server_url;
  return kDefaultServerUrl;
}

// Gets the maximum number of Sources we'll keep in memory before discarding any
// new ones being added.
size_t GetMaxSources() {
  constexpr size_t kDefaultMaxSources = 500;
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      kUkmFeature, "MaxSources", kDefaultMaxSources));
}

// Gets the maximum number of Entries we'll keep in memory before discarding any
// new ones being added.
size_t GetMaxEntries() {
  constexpr size_t kDefaultMaxEntries = 5000;
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      kUkmFeature, "MaxEntries", kDefaultMaxEntries));
}

// True if we should record the initial_url field of the UKM Source proto.
bool ShouldRecordInitialUrl() {
  return base::GetFieldTrialParamByFeatureAsBool(kUkmFeature,
                                                 "RecordInitialUrl", false);
}

// True if we should record session ids in the UKM Report proto.
bool ShouldRecordSessionId() {
  return base::GetFieldTrialParamByFeatureAsBool(kUkmFeature, "RecordSessionId",
                                                 false);
}

// Generates a new client id and stores it in prefs.
uint64_t GenerateClientId(PrefService* pref_service) {
  uint64_t client_id = 0;
  while (!client_id)
    client_id = base::RandUint64();
  pref_service->SetInt64(prefs::kUkmClientId, client_id);

  // Also reset the session id counter.
  pref_service->SetInteger(prefs::kUkmSessionId, 0);
  return client_id;
}

uint64_t LoadOrGenerateClientId(PrefService* pref_service) {
  uint64_t client_id = pref_service->GetInt64(prefs::kUkmClientId);
  if (!client_id)
    client_id = GenerateClientId(pref_service);
  return client_id;
}

int32_t LoadSessionId(PrefService* pref_service) {
  int32_t session_id = pref_service->GetInteger(prefs::kUkmSessionId);
  ++session_id;  // increment session id, once per session
  pref_service->SetInteger(prefs::kUkmSessionId, session_id);
  return session_id;
}

enum class DroppedDataReason {
  NOT_DROPPED = 0,
  RECORDING_DISABLED = 1,
  MAX_HIT = 2,
  NUM_DROPPED_DATA_REASONS
};

void RecordDroppedSource(DroppedDataReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "UKM.Sources.Dropped", static_cast<int>(reason),
      static_cast<int>(DroppedDataReason::NUM_DROPPED_DATA_REASONS));
}

void RecordDroppedEntry(DroppedDataReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "UKM.Entries.Dropped", static_cast<int>(reason),
      static_cast<int>(DroppedDataReason::NUM_DROPPED_DATA_REASONS));
}

}  // namespace

const base::Feature kUkmFeature = {"Ukm", base::FEATURE_DISABLED_BY_DEFAULT};

UkmService::UkmService(PrefService* pref_service,
                       metrics::MetricsServiceClient* client)
    : pref_service_(pref_service),
      recording_enabled_(false),
      client_id_(0),
      session_id_(0),
      client_(client),
      persisted_logs_(std::unique_ptr<ukm::PersistedLogsMetricsImpl>(
                          new ukm::PersistedLogsMetricsImpl()),
                      pref_service,
                      prefs::kUkmPersistedLogs,
                      kMinPersistedLogs,
                      kMinPersistedBytes,
                      kMaxLogRetransmitSize),
      initialize_started_(false),
      initialize_complete_(false),
      log_upload_in_progress_(false),
      self_ptr_factory_(this) {
  DCHECK(pref_service_);
  DCHECK(client_);
  DVLOG(1) << "UkmService::Constructor";

  persisted_logs_.LoadPersistedUnsentLogs();

  base::Closure rotate_callback =
      base::Bind(&UkmService::RotateLog, self_ptr_factory_.GetWeakPtr());
  // MetricsServiceClient outlives UkmService, and
  // MetricsReportingScheduler is tied to the lifetime of |this|.
  const base::Callback<base::TimeDelta(void)>& get_upload_interval_callback =
      base::Bind(&metrics::MetricsServiceClient::GetStandardUploadInterval,
                 base::Unretained(client_));
  scheduler_.reset(new ukm::MetricsReportingScheduler(
      rotate_callback, get_upload_interval_callback));

  for (auto& provider : metrics_providers_)
    provider->Init();
}

UkmService::~UkmService() {
  DisableReporting();
}

void UkmService::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!initialize_started_);
  DVLOG(1) << "UkmService::Initialize";
  initialize_started_ = true;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&UkmService::StartInitTask, self_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kInitializationDelaySeconds));
}

void UkmService::EnableRecording() {
  recording_enabled_ = true;
}

void UkmService::DisableRecording() {
  recording_enabled_ = false;
}

void UkmService::EnableReporting() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::EnableReporting";

  for (auto& provider : metrics_providers_)
    provider->OnRecordingEnabled();

  if (!initialize_started_)
    Initialize();
  scheduler_->Start();
}

void UkmService::DisableReporting() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::DisableReporting";

  for (auto& provider : metrics_providers_)
    provider->OnRecordingDisabled();

  scheduler_->Stop();
  Flush();
}

#if defined(OS_ANDROID) || defined(OS_IOS)
void UkmService::OnAppEnterForeground() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::OnAppEnterForeground";

  // If initialize_started_ is false, UKM has not yet been started, so bail. The
  // scheduler will instead be started via EnableReporting().
  if (!initialize_started_)
    return;

  scheduler_->Start();
}

void UkmService::OnAppEnterBackground() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::OnAppEnterBackground";

  if (!initialize_started_)
    return;

  scheduler_->Stop();

  // Give providers a chance to persist ukm data as part of being backgrounded.
  for (auto& provider : metrics_providers_)
    provider->OnAppEnterBackground();

  Flush();
}
#endif

void UkmService::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (initialize_complete_)
    BuildAndStoreLog();
  persisted_logs_.PersistUnsentLogs();
}

void UkmService::Purge() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::Purge";

  persisted_logs_.Purge();
  sources_.clear();
  entries_.clear();
}

// TODO(bmcquade): rename this to something more generic, like
// ResetClientState. Consider resetting all prefs here.
void UkmService::ResetClientId() {
  client_id_ = GenerateClientId(pref_service_);
  session_id_ = LoadSessionId(pref_service_);
}

void UkmService::RegisterMetricsProvider(
    std::unique_ptr<metrics::MetricsProvider> provider) {
  metrics_providers_.push_back(std::move(provider));
}

// static
void UkmService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kUkmClientId, 0);
  registry->RegisterIntegerPref(prefs::kUkmSessionId, 0);
  registry->RegisterListPref(prefs::kUkmPersistedLogs);
}

void UkmService::StartInitTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::StartInitTask";
  client_id_ = LoadOrGenerateClientId(pref_service_);
  session_id_ = LoadSessionId(pref_service_);
  client_->InitializeSystemProfileMetrics(base::Bind(
      &UkmService::FinishedInitTask, self_ptr_factory_.GetWeakPtr()));
}

void UkmService::FinishedInitTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::FinishedInitTask";
  initialize_complete_ = true;
  scheduler_->InitTaskComplete();
}

void UkmService::RotateLog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!log_upload_in_progress_);
  DVLOG(1) << "UkmService::RotateLog";
  if (!persisted_logs_.has_unsent_logs())
    BuildAndStoreLog();
  StartScheduledUpload();
}

void UkmService::BuildAndStoreLog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::BuildAndStoreLog";

  // Suppress generating a log if we have no new data to include.
  // TODO(zhenw): add a histogram here to debug if this case is hitting a lot.
  if (sources_.empty() && entries_.empty())
    return;

  Report report;
  report.set_client_id(client_id_);
  if (ShouldRecordSessionId())
    report.set_session_id(session_id_);

  for (const auto& source : sources_) {
    Source* proto_source = report.add_sources();
    source->PopulateProto(proto_source);
    if (!ShouldRecordInitialUrl())
      proto_source->clear_initial_url();
  }
  for (const auto& entry : entries_) {
    Entry* proto_entry = report.add_entries();
    entry->PopulateProto(proto_entry);
  }

  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.SerializedCount", sources_.size());
  UMA_HISTOGRAM_COUNTS_1000("UKM.Entries.SerializedCount", entries_.size());
  sources_.clear();
  entries_.clear();

  metrics::MetricsLog::RecordCoreSystemProfile(client_,
                                               report.mutable_system_profile());

  for (auto& provider : metrics_providers_) {
    provider->ProvideSystemProfileMetrics(report.mutable_system_profile());
  }

  std::string serialized_log;
  report.SerializeToString(&serialized_log);
  persisted_logs_.StoreLog(serialized_log);
}

void UkmService::StartScheduledUpload() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!log_upload_in_progress_);
  if (!persisted_logs_.has_unsent_logs()) {
    // No logs to send, so early out and schedule the next rotation.
    scheduler_->UploadFinished(true, /* has_unsent_logs */ false);
    return;
  }
  if (!persisted_logs_.has_staged_log())
    persisted_logs_.StageNextLog();
  // TODO(holte): Handle data usage on cellular, etc.
  if (!log_uploader_) {
    log_uploader_ = client_->CreateUploader(
        GetServerUrl(), kMimeType, metrics::MetricsLogUploader::UKM,
        base::Bind(&UkmService::OnLogUploadComplete,
                   self_ptr_factory_.GetWeakPtr()));
  }
  log_upload_in_progress_ = true;

  const std::string hash =
      base::HexEncode(persisted_logs_.staged_log_hash().data(),
                      persisted_logs_.staged_log_hash().size());
  log_uploader_->UploadLog(persisted_logs_.staged_log(), hash);
}

void UkmService::OnLogUploadComplete(int response_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(log_upload_in_progress_);
  DVLOG(1) << "UkmService::OnLogUploadComplete";
  log_upload_in_progress_ = false;

  UMA_HISTOGRAM_SPARSE_SLOWLY("UKM.Upload.ResponseCode", response_code);

  bool upload_succeeded = response_code == 200;

  // Provide boolean for error recovery (allow us to ignore response_code).
  bool discard_log = false;
  const size_t log_size_bytes = persisted_logs_.staged_log().length();
  if (upload_succeeded) {
    UMA_HISTOGRAM_COUNTS_10000("UKM.LogSize.OnSuccess", log_size_bytes / 1024);
  } else if (response_code == 400) {
    // Bad syntax.  Retransmission won't work.
    discard_log = true;
  }

  if (upload_succeeded || discard_log) {
    // TODO(holte): The if below is a temporary fix for a crash bug. We should
    // revisit the logic and update it with a more correct fix. crbug.com/698819
    if (persisted_logs_.has_staged_log())
      persisted_logs_.DiscardStagedLog();
    // Store the updated list to disk now that the removed log is uploaded.
    persisted_logs_.PersistUnsentLogs();
  }

  // Error 400 indicates a problem with the log, not with the server, so
  // don't consider that a sign that the server is in trouble.
  bool server_is_healthy = upload_succeeded || response_code == 400;
  scheduler_->UploadFinished(server_is_healthy,
                             persisted_logs_.has_unsent_logs());
}

void UkmService::RecordSource(std::unique_ptr<UkmSource> source) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!recording_enabled_) {
    RecordDroppedSource(DroppedDataReason::RECORDING_DISABLED);
    return;
  }
  if (sources_.size() >= GetMaxSources()) {
    RecordDroppedSource(DroppedDataReason::MAX_HIT);
    return;
  }

  sources_.push_back(std::move(source));
}

// static
int32_t UkmService::GetNewSourceID() {
  static int32_t next_source_id = 0;
  return next_source_id++;
}

std::unique_ptr<UkmEntryBuilder> UkmService::GetEntryBuilder(
    int32_t source_id,
    const char* event_name) {
  return std::unique_ptr<UkmEntryBuilder>(new UkmEntryBuilder(
      base::Bind(&UkmService::AddEntry, base::Unretained(this)), source_id,
      event_name));
}

void UkmService::UpdateSourceURL(int32_t source_id, const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!recording_enabled_) {
    RecordDroppedSource(DroppedDataReason::RECORDING_DISABLED);
    return;
  }

  // Update the pre-existing source if there is any. This happens when the
  // initial URL is different from the committed URL for the same source, e.g.,
  // when there is redirection.
  for (auto& source : sources_) {
    if (source_id != source->id())
      continue;

    source->UpdateUrl(url);
    return;
  }

  if (sources_.size() >= GetMaxSources()) {
    RecordDroppedSource(DroppedDataReason::MAX_HIT);
    return;
  }
  std::unique_ptr<UkmSource> source = base::MakeUnique<UkmSource>();
  source->set_id(source_id);
  source->set_url(url);
  sources_.push_back(std::move(source));
}

void UkmService::AddEntry(std::unique_ptr<UkmEntry> entry) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!recording_enabled_) {
    RecordDroppedEntry(DroppedDataReason::RECORDING_DISABLED);
    return;
  }
  if (entries_.size() >= GetMaxEntries()) {
    RecordDroppedEntry(DroppedDataReason::MAX_HIT);
    return;
  }

  entries_.push_back(std::move(entry));
}

}  // namespace ukm
