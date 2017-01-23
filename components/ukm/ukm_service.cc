// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/proto/ukm/report.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/metrics_reporting_scheduler.h"
#include "components/ukm/persisted_logs_metrics_impl.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/variations/variations_associated_data.h"

namespace ukm {

namespace {

constexpr char kMimeType[] = "application/vnd.chrome.ukm";

// The UKM server's URL.
constexpr char kDefaultServerUrl[] = "https://clients4.google.com/ukm";

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

std::string GetServerUrl() {
  std::string server_url =
      variations::GetVariationParamValueByFeature(kUkmFeature, "ServerUrl");
  if (!server_url.empty())
    return server_url;
  return kDefaultServerUrl;
}

uint64_t LoadOrGenerateClientId(PrefService* pref_service) {
  uint64_t client_id = pref_service->GetInt64(prefs::kUkmClientId);
  if (!client_id) {
    // Generate and store a new client id.
    while (!client_id)
      client_id = base::RandUint64();
    pref_service->SetInt64(prefs::kUkmClientId, client_id);
  }
  return client_id;
}

}  // namespace

const base::Feature kUkmFeature = {"Ukm", base::FEATURE_DISABLED_BY_DEFAULT};

UkmService::UkmService(PrefService* pref_service,
                       metrics::MetricsServiceClient* client)
    : pref_service_(pref_service),
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

  persisted_logs_.DeserializeLogs();

  base::Closure rotate_callback =
      base::Bind(&UkmService::RotateLog, self_ptr_factory_.GetWeakPtr());
  // MetricsServiceClient outlives UkmService, and
  // MetricsReportingScheduler is tied to the lifetime of |this|.
  const base::Callback<base::TimeDelta(void)>& get_upload_interval_callback =
      base::Bind(&metrics::MetricsServiceClient::GetStandardUploadInterval,
                 base::Unretained(client_));
  scheduler_.reset(new ukm::MetricsReportingScheduler(
      rotate_callback, get_upload_interval_callback));
}

UkmService::~UkmService() {
  DisableReporting();
}

void UkmService::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::Initialize";
  initialize_started_ = true;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&UkmService::StartInitTask, self_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kInitializationDelaySeconds));
}

void UkmService::EnableReporting() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::EnableReporting";
#if defined(OFFICIAL_BUILD)
  CHECK(false);  // Remove this once UKM checks sync state.
#endif
  if (!initialize_started_)
    Initialize();
  scheduler_->Start();
}

void UkmService::DisableReporting() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::DisableReporting";
  scheduler_->Stop();
  Flush();
}

void UkmService::Flush() {
  if (initialize_complete_)
    BuildAndStoreLog();
  persisted_logs_.SerializeLogs();
}

void UkmService::Purge() {
  DVLOG(1) << "UkmService::Purge";
  persisted_logs_.Purge();
  // TODO(oystein): Delete any stored sources.
}

// static
void UkmService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kUkmClientId, 0);
  registry->RegisterListPref(prefs::kUkmPersistedLogs);
}

void UkmService::StartInitTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::StartInitTask";
  client_id_ = LoadOrGenerateClientId(pref_service_);
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
  if (persisted_logs_.empty()) {
    BuildAndStoreLog();
  }
  StartScheduledUpload();
}

void UkmService::BuildAndStoreLog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "UkmService::BuildAndStoreLog";
  Report report;
  report.set_client_id(client_id_);
  // TODO(holte): Populate system_profile.
  // TODO(oystein): Populate sources.
  std::string serialized_log;
  report.SerializeToString(&serialized_log);
  persisted_logs_.StoreLog(serialized_log);
}

void UkmService::StartScheduledUpload() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!log_upload_in_progress_);
  if (!persisted_logs_.has_staged_log())
    persisted_logs_.StageLog();
  // TODO(holte): Handle data usage on cellular, etc.
  if (!log_uploader_) {
    log_uploader_ = client_->CreateUploader(
        GetServerUrl(), kMimeType, base::Bind(&UkmService::OnLogUploadComplete,
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
    persisted_logs_.DiscardStagedLog();
    // Store the updated list to disk now that the removed log is uploaded.
    persisted_logs_.SerializeLogs();
  }

  // Error 400 indicates a problem with the log, not with the server, so
  // don't consider that a sign that the server is in trouble.
  bool server_is_healthy = upload_succeeded || response_code == 400;
  scheduler_->UploadFinished(server_is_healthy, !persisted_logs_.empty());
}

}  // namespace ukm
