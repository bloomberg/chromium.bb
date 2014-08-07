// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/rappor_service.h"

#include "base/base64.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "components/metrics/metrics_hashes.h"
#include "components/rappor/log_uploader.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "components/rappor/rappor_metric.h"
#include "components/rappor/rappor_pref_names.h"
#include "components/variations/variations_associated_data.h"

namespace rappor {

namespace {

// Seconds before the initial log is generated.
const int kInitialLogIntervalSeconds = 15;
// Interval between ongoing logs.
const int kLogIntervalSeconds = 30 * 60;

const char kMimeType[] = "application/vnd.chrome.rappor";

// Constants for the RAPPOR rollout field trial.
const char kRapporRolloutFieldTrialName[] = "RapporRollout";

// Constant for the finch parameter name for the server URL
const char kRapporRolloutServerUrlParam[] = "ServerUrl";

// Constant for the finch parameter name for the server URL
const char kRapporRolloutRequireUmaParam[] = "RequireUma";

// The rappor server's URL.
const char kDefaultServerUrl[] = "https://clients4.google.com/rappor";

GURL GetServerUrl(bool metrics_enabled) {
  bool require_uma = variations::GetVariationParamValue(
      kRapporRolloutFieldTrialName,
      kRapporRolloutRequireUmaParam) != "False";
  if (!metrics_enabled && require_uma)
    return GURL();  // Invalid URL disables Rappor.
  std::string server_url = variations::GetVariationParamValue(
      kRapporRolloutFieldTrialName,
      kRapporRolloutServerUrlParam);
  if (!server_url.empty())
    return GURL(server_url);
  else
    return GURL(kDefaultServerUrl);
}

const RapporParameters kRapporParametersForType[NUM_RAPPOR_TYPES] = {
    // ETLD_PLUS_ONE_RAPPOR_TYPE
    {128 /* Num cohorts */,
     16 /* Bloom filter size bytes */,
     2 /* Bloom filter hash count */,
     rappor::PROBABILITY_50 /* Fake data probability */,
     rappor::PROBABILITY_50 /* Fake one probability */,
     rappor::PROBABILITY_75 /* One coin probability */,
     rappor::PROBABILITY_25 /* Zero coin probability */},
};

}  // namespace

RapporService::RapporService() : cohort_(-1) {}

RapporService::~RapporService() {
  STLDeleteValues(&metrics_map_);
}

void RapporService::Start(PrefService* pref_service,
                          net::URLRequestContextGetter* request_context,
                          bool metrics_enabled) {
  const GURL server_url = GetServerUrl(metrics_enabled);
  if (!server_url.is_valid()) {
    DVLOG(1) << server_url.spec() << " is invalid. "
             << "RapporService not started.";
    return;
  }
  DVLOG(1) << "RapporService started. Reporting to " << server_url.spec();
  DCHECK(!uploader_);
  LoadSecret(pref_service);
  LoadCohort(pref_service);
  uploader_.reset(new LogUploader(server_url, kMimeType, request_context));
  log_rotation_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kInitialLogIntervalSeconds),
      this,
      &RapporService::OnLogInterval);
}

void RapporService::OnLogInterval() {
  DCHECK(uploader_);
  DVLOG(2) << "RapporService::OnLogInterval";
  RapporReports reports;
  if (ExportMetrics(&reports)) {
    std::string log_text;
    bool success = reports.SerializeToString(&log_text);
    DCHECK(success);
    DVLOG(1) << "RapporService sending a report of "
             << reports.report_size() << " value(s).";
    uploader_->QueueLog(log_text);
  }
  log_rotation_timer_.Start(FROM_HERE,
                            base::TimeDelta::FromSeconds(kLogIntervalSeconds),
                            this,
                            &RapporService::OnLogInterval);
}

// static
void RapporService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kRapporSecret, std::string());
  registry->RegisterIntegerPref(prefs::kRapporCohortDeprecated, -1);
  registry->RegisterIntegerPref(prefs::kRapporCohortSeed, -1);
}

void RapporService::LoadCohort(PrefService* pref_service) {
  DCHECK(!IsInitialized());
  // Ignore and delete old cohort parameter.
  pref_service->ClearPref(prefs::kRapporCohortDeprecated);

  cohort_ = pref_service->GetInteger(prefs::kRapporCohortSeed);
  // If the user is already assigned to a valid cohort, we're done.
  if (cohort_ >= 0 && cohort_ < RapporParameters::kMaxCohorts)
    return;

  // This is the first time the client has started the service (or their
  // preferences were corrupted).  Randomly assign them to a cohort.
  cohort_ = base::RandGenerator(RapporParameters::kMaxCohorts);
  DVLOG(2) << "Selected a new Rappor cohort: " << cohort_;
  pref_service->SetInteger(prefs::kRapporCohortSeed, cohort_);
}

void RapporService::LoadSecret(PrefService* pref_service) {
  DCHECK(secret_.empty());
  std::string secret_base64 = pref_service->GetString(prefs::kRapporSecret);
  if (!secret_base64.empty()) {
    bool decoded = base::Base64Decode(secret_base64, &secret_);
    if (decoded && secret_.size() == HmacByteVectorGenerator::kEntropyInputSize)
      return;
    // If the preference fails to decode, or is the wrong size, it must be
    // corrupt, so continue as though it didn't exist yet and generate a new
    // one.
  }

  DVLOG(2) << "Generated a new Rappor secret.";
  secret_ = HmacByteVectorGenerator::GenerateEntropyInput();
  base::Base64Encode(secret_, &secret_base64);
  pref_service->SetString(prefs::kRapporSecret, secret_base64);
}

bool RapporService::ExportMetrics(RapporReports* reports) {
  if (metrics_map_.empty())
    return false;

  DCHECK_GE(cohort_, 0);
  reports->set_cohort(cohort_);

  for (std::map<std::string, RapporMetric*>::const_iterator it =
           metrics_map_.begin();
       it != metrics_map_.end();
       ++it) {
    const RapporMetric* metric = it->second;
    RapporReports::Report* report = reports->add_report();
    report->set_name_hash(metrics::HashMetricName(it->first));
    ByteVector bytes = metric->GetReport(secret_);
    report->set_bits(std::string(bytes.begin(), bytes.end()));
  }
  STLDeleteValues(&metrics_map_);
  return true;
}

bool RapporService::IsInitialized() const {
  return cohort_ >= 0;
}

void RapporService::RecordSample(const std::string& metric_name,
                                 RapporType type,
                                 const std::string& sample) {
  // Ignore the sample if the service hasn't started yet.
  if (!IsInitialized())
    return;
  DCHECK_LT(type, NUM_RAPPOR_TYPES);
  DVLOG(2) << "Recording sample \"" << sample
           << "\" for metric \"" << metric_name
           << "\" of type: " << type;
  RecordSampleInternal(metric_name, kRapporParametersForType[type], sample);
}

void RapporService::RecordSampleInternal(const std::string& metric_name,
                                         const RapporParameters& parameters,
                                         const std::string& sample) {
  DCHECK(IsInitialized());
  RapporMetric* metric = LookUpMetric(metric_name, parameters);
  metric->AddSample(sample);
}

RapporMetric* RapporService::LookUpMetric(const std::string& metric_name,
                                          const RapporParameters& parameters) {
  DCHECK(IsInitialized());
  std::map<std::string, RapporMetric*>::const_iterator it =
      metrics_map_.find(metric_name);
  if (it != metrics_map_.end()) {
    RapporMetric* metric = it->second;
    DCHECK_EQ(parameters.ToString(), metric->parameters().ToString());
    return metric;
  }

  RapporMetric* new_metric = new RapporMetric(metric_name, parameters, cohort_);
  metrics_map_[metric_name] = new_metric;
  return new_metric;
}

}  // namespace rappor
