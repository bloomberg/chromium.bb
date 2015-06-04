// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/rappor_service.h"

#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "components/metrics/metrics_hashes.h"
#include "components/rappor/log_uploader.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "components/rappor/rappor_metric.h"
#include "components/rappor/rappor_pref_names.h"
#include "components/rappor/rappor_prefs.h"
#include "components/variations/variations_associated_data.h"

namespace rappor {

namespace {

// Seconds before the initial log is generated.
const int kInitialLogIntervalSeconds = 15;
// Interval between ongoing logs.
const int kLogIntervalSeconds = 30 * 60;

const char kMimeType[] = "application/vnd.chrome.rappor";

const char kRapporDailyEventHistogram[] = "Rappor.DailyEvent.IntervalType";

// Constants for the RAPPOR rollout field trial.
const char kRapporRolloutFieldTrialName[] = "RapporRollout";

// Constant for the finch parameter name for the server URL
const char kRapporRolloutServerUrlParam[] = "ServerUrl";

// The rappor server's URL.
const char kDefaultServerUrl[] = "https://clients4.google.com/rappor";

GURL GetServerUrl() {
  std::string server_url = variations::GetVariationParamValue(
      kRapporRolloutFieldTrialName,
      kRapporRolloutServerUrlParam);
  if (!server_url.empty())
    return GURL(server_url);
  else
    return GURL(kDefaultServerUrl);
}

const NoiseParameters kBasicNoiseParameters = {
    rappor::PROBABILITY_50 /* Fake data probability */,
    rappor::PROBABILITY_50 /* Fake one probability */,
    rappor::PROBABILITY_75 /* One coin probability */,
    rappor::PROBABILITY_25 /* Zero coin probability */,
};

const NoiseParameters kWeakNoiseParameters = {
    rappor::PROBABILITY_25 /* Fake data probability */,
    rappor::PROBABILITY_50 /* Fake one probability */,
    rappor::PROBABILITY_875 /* One coin probability */,
    rappor::PROBABILITY_125 /* Zero coin probability */,
};

const RapporParameters kRapporParametersForType[NUM_RAPPOR_TYPES] = {
    // UMA_RAPPOR_TYPE
    {128 /* Num cohorts */,
     4 /* Bloom filter size bytes */,
     2 /* Bloom filter hash count */,
     kBasicNoiseParameters /* String noise parameters */,
     kWeakNoiseParameters /* Flags noise parameters */,
     FINE_LEVEL /* Recording level */},
    // COARSE_RAPPOR_TYPE
    {128 /* Num cohorts */,
     1 /* Bloom filter size bytes */,
     2 /* Bloom filter hash count */,
     kBasicNoiseParameters /* String noise parameters */,
     kWeakNoiseParameters /* Flags noise parameters */,
     COARSE_LEVEL /* Recording level */},
    // ETLD_PLUS_ONE_RAPPOR_TYPE
    {128 /* Num cohorts */,
     16 /* Bloom filter size bytes */,
     2 /* Bloom filter hash count */,
     kBasicNoiseParameters /* String noise parameters */,
     kWeakNoiseParameters /* Flags noise parameters */,
     FINE_LEVEL /* Recording level */},
};

}  // namespace

RapporService::RapporService(
    PrefService* pref_service,
    const base::Callback<bool(void)> is_incognito_callback)
    : pref_service_(pref_service),
      is_incognito_callback_(is_incognito_callback),
      cohort_(-1),
      daily_event_(pref_service,
                   prefs::kRapporLastDailySample,
                   kRapporDailyEventHistogram),
      recording_level_(RECORDING_DISABLED) {
}

RapporService::~RapporService() {
  STLDeleteValues(&metrics_map_);
}

void RapporService::AddDailyObserver(
    scoped_ptr<metrics::DailyEvent::Observer> observer) {
  daily_event_.AddObserver(observer.Pass());
}

void RapporService::Initialize(net::URLRequestContextGetter* request_context) {
  DCHECK(!IsInitialized());
  const GURL server_url = GetServerUrl();
  if (!server_url.is_valid()) {
    DVLOG(1) << server_url.spec() << " is invalid. "
             << "RapporService not started.";
    return;
  }
  DVLOG(1) << "RapporService reporting to " << server_url.spec();
  InitializeInternal(make_scoped_ptr(new LogUploader(server_url,
                                                     kMimeType,
                                                     request_context)),
                     internal::LoadCohort(pref_service_),
                     internal::LoadSecret(pref_service_));
}

void RapporService::Update(RecordingLevel recording_level, bool may_upload) {
  DCHECK(IsInitialized());
  if (recording_level_ != recording_level) {
    if (recording_level == RECORDING_DISABLED) {
      DVLOG(1) << "Rappor service stopped due to RECORDING_DISABLED.";
      recording_level_ = RECORDING_DISABLED;
      CancelNextLogRotation();
    } else if (recording_level_ == RECORDING_DISABLED) {
      DVLOG(1) << "RapporService started at recording level: "
               << recording_level;
      recording_level_ = recording_level;
      ScheduleNextLogRotation(
          base::TimeDelta::FromSeconds(kInitialLogIntervalSeconds));
    } else {
      DVLOG(1) << "RapporService recording_level changed:" << recording_level;
      recording_level_ = recording_level;
    }
  }

  DVLOG(1) << "RapporService recording_level=" << recording_level_
           << " may_upload=" << may_upload;
  if (may_upload) {
    uploader_->Start();
  } else {
    uploader_->Stop();
  }
}

// static
void RapporService::RegisterPrefs(PrefRegistrySimple* registry) {
  internal::RegisterPrefs(registry);
}

void RapporService::InitializeInternal(
    scoped_ptr<LogUploaderInterface> uploader,
    int32_t cohort,
    const std::string& secret) {
  DCHECK(!IsInitialized());
  DCHECK(secret_.empty());
  uploader_.swap(uploader);
  cohort_ = cohort;
  secret_ = secret;
}

void RapporService::SetRecordingLevel(RecordingLevel recording_level) {
  recording_level_ = recording_level;
}

void RapporService::CancelNextLogRotation() {
  STLDeleteValues(&metrics_map_);
  log_rotation_timer_.Stop();
}

void RapporService::ScheduleNextLogRotation(base::TimeDelta interval) {
  log_rotation_timer_.Start(FROM_HERE,
                            interval,
                            this,
                            &RapporService::OnLogInterval);
}

void RapporService::OnLogInterval() {
  DCHECK(uploader_);
  DVLOG(2) << "RapporService::OnLogInterval";
  daily_event_.CheckInterval();
  RapporReports reports;
  if (ExportMetrics(&reports)) {
    std::string log_text;
    bool success = reports.SerializeToString(&log_text);
    DCHECK(success);
    DVLOG(1) << "RapporService sending a report of "
             << reports.report_size() << " value(s).";
    uploader_->QueueLog(log_text);
  }
  ScheduleNextLogRotation(base::TimeDelta::FromSeconds(kLogIntervalSeconds));
}

bool RapporService::ExportMetrics(RapporReports* reports) {
  DCHECK_GE(cohort_, 0);
  reports->set_cohort(cohort_);

  for (const auto& kv : metrics_map_) {
    const RapporMetric* metric = kv.second;
    RapporReports::Report* report = reports->add_report();
    report->set_name_hash(metrics::HashMetricName(kv.first));
    ByteVector bytes = metric->GetReport(secret_);
    report->set_bits(std::string(bytes.begin(), bytes.end()));
  }
  STLDeleteValues(&metrics_map_);

  sampler_.ExportMetrics(secret_, reports);

  DVLOG(2) << "Generated a report with " << reports->report_size()
           << "metrics.";
  return reports->report_size() > 0;
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
  const RapporParameters& parameters = kRapporParametersForType[type];
  DVLOG(2) << "Recording sample \"" << sample
           << "\" for metric \"" << metric_name
           << "\" of type: " << type;
  RecordSampleInternal(metric_name, parameters, sample);
}

void RapporService::RecordSampleInternal(const std::string& metric_name,
                                         const RapporParameters& parameters,
                                         const std::string& sample) {
  DCHECK(IsInitialized());
  if (is_incognito_callback_.Run()) {
    DVLOG(2) << "Metric not logged due to incognito mode.";
    return;
  }
  // Skip this metric if its reporting level is less than the enabled
  // reporting level.
  if (recording_level_ < parameters.recording_level) {
    DVLOG(2) << "Metric not logged due to recording_level "
             << recording_level_ << " < " << parameters.recording_level;
    return;
  }
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

scoped_ptr<Sample> RapporService::CreateSample(RapporType type) {
  DCHECK(IsInitialized());
  return scoped_ptr<Sample>(
      new Sample(cohort_, kRapporParametersForType[type]));
}

void RapporService::RecordSampleObj(const std::string& metric_name,
                                    scoped_ptr<Sample> sample) {
  if (is_incognito_callback_.Run()) {
    DVLOG(2) << "Metric not logged due to incognito mode.";
    return;
  }
  // Skip this metric if its reporting level is less than the enabled
  // reporting level.
  if (recording_level_ < sample->parameters().recording_level) {
    DVLOG(2) << "Metric not logged due to recording_level "
             << recording_level_ << " < "
             << sample->parameters().recording_level;
    return;
  }
  sampler_.AddSample(metric_name, sample.Pass());
}

}  // namespace rappor
