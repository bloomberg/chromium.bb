// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/external_metrics.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/serialization/metric_sample.h"
#include "components/metrics/serialization/serialization_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"

using base::UserMetricsAction;
using content::BrowserThread;

namespace chromeos {

namespace {

bool CheckValues(const std::string& name,
                 int minimum,
                 int maximum,
                 size_t bucket_count) {
  if (!base::Histogram::InspectConstructionArguments(
      name, &minimum, &maximum, &bucket_count))
    return false;
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  if (!histogram)
    return true;
  return histogram->HasConstructionArguments(minimum, maximum, bucket_count);
}

bool CheckLinearValues(const std::string& name, int maximum) {
  return CheckValues(name, 1, maximum, maximum + 1);
}

// Establishes field trial for wifi scanning in chromeos.  crbug.com/242733.
void SetupProgressiveScanFieldTrial() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  const char name_of_experiment[] = "ProgressiveScan";
  const base::FilePath group_file_path(
      "/home/chronos/.progressive_scan_variation");
  const base::FieldTrial::Probability kDivisor = 1000;
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(
          name_of_experiment, kDivisor, "Default", 2013, 12, 31,
          base::FieldTrial::SESSION_RANDOMIZED, NULL);

  // Announce the groups with 0 percentage; the actual percentages come from
  // the server configuration.
  std::map<int, std::string> group_to_char;
  group_to_char[trial->AppendGroup("FullScan", 0)] = "c";
  group_to_char[trial->AppendGroup("33Percent_4MinMax", 0)] = "1";
  group_to_char[trial->AppendGroup("50Percent_4MinMax", 0)] = "2";
  group_to_char[trial->AppendGroup("50Percent_8MinMax", 0)] = "3";
  group_to_char[trial->AppendGroup("100Percent_8MinMax", 0)] = "4";
  group_to_char[trial->AppendGroup("100Percent_1MinSeen_A", 0)] = "5";
  group_to_char[trial->AppendGroup("100Percent_1MinSeen_B", 0)] = "6";
  group_to_char[trial->AppendGroup("100Percent_1Min_4Max", 0)] = "7";

  // Announce the experiment to any listeners (especially important is the UMA
  // software, which will append the group names to UMA statistics).
  const int group_num = trial->group();
  std::string group_char = "x";
  if (ContainsKey(group_to_char, group_num))
    group_char = group_to_char[group_num];

  // Write the group to the file to be read by ChromeOS.
  int size = static_cast<int>(group_char.length());
  if (base::WriteFile(group_file_path, group_char.c_str(), size) == size) {
    VLOG(1) << "Configured in group '" << trial->group_name()
            << "' ('" << group_char << "') for "
            << name_of_experiment << " field trial";
  } else {
    VLOG(1) << "Couldn't write to " << group_file_path.value();
  }
}

}  // namespace

// The interval between external metrics collections in seconds
static const int kExternalMetricsCollectionIntervalSeconds = 30;
const char kEventsFilePath[] = "/var/run/metrics/uma-events";

ExternalMetrics::ExternalMetrics() : uma_events_file_(kEventsFilePath) {
}

ExternalMetrics::~ExternalMetrics() {}

void ExternalMetrics::Start() {
  // Register user actions external to the browser.
  // tools/metrics/actions/extract_actions.py won't understand these lines, so
  // all of these are explicitly added in that script.
  // TODO(derat): We shouldn't need to verify actions before reporting them;
  // remove all of this once http://crosbug.com/11125 is fixed.
  valid_user_actions_.insert("Cryptohome.PKCS11InitFail");
  valid_user_actions_.insert("Updater.ServerCertificateChanged");
  valid_user_actions_.insert("Updater.ServerCertificateFailed");

  // Initialize here field trials that don't need to read from files.
  // (None for the moment.)

  // Initialize any chromeos field trials that need to read from a file (e.g.,
  // those that have an upstart script determine their experimental group for
  // them) then schedule the data collection.  All of this is done on the file
  // thread.
  bool task_posted = BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&chromeos::ExternalMetrics::SetupFieldTrialsOnFileThread,
                 this));
  DCHECK(task_posted);
}

// static
scoped_refptr<ExternalMetrics> ExternalMetrics::CreateForTesting(
    const std::string& filename) {
  scoped_refptr<ExternalMetrics> external_metrics(new ExternalMetrics());
  external_metrics->uma_events_file_ = filename;
  return external_metrics;
}

void ExternalMetrics::RecordActionUI(std::string action_string) {
  if (valid_user_actions_.count(action_string)) {
    content::RecordComputedAction(action_string);
  } else {
    DLOG(ERROR) << "undefined UMA action: " << action_string;
  }
}

void ExternalMetrics::RecordAction(const std::string& action) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ExternalMetrics::RecordActionUI, this, action));
}

void ExternalMetrics::RecordCrashUI(const std::string& crash_kind) {
  ChromeOSMetricsProvider::LogCrash(crash_kind);
}

void ExternalMetrics::RecordCrash(const std::string& crash_kind) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ExternalMetrics::RecordCrashUI, this, crash_kind));
}

void ExternalMetrics::RecordHistogram(const metrics::MetricSample& sample) {
  CHECK_EQ(metrics::MetricSample::HISTOGRAM, sample.type());
  if (!CheckValues(
          sample.name(), sample.min(), sample.max(), sample.bucket_count())) {
    DLOG(ERROR) << "Invalid histogram: " << sample.name();
    return;
  }

  base::HistogramBase* counter =
      base::Histogram::FactoryGet(sample.name(),
                                  sample.min(),
                                  sample.max(),
                                  sample.bucket_count(),
                                  base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(sample.sample());
}

void ExternalMetrics::RecordLinearHistogram(
    const metrics::MetricSample& sample) {
  CHECK_EQ(metrics::MetricSample::LINEAR_HISTOGRAM, sample.type());
  if (!CheckLinearValues(sample.name(), sample.max())) {
    DLOG(ERROR) << "Invalid linear histogram: " << sample.name();
    return;
  }
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      sample.name(),
      1,
      sample.max(),
      sample.max() + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(sample.sample());
}

void ExternalMetrics::RecordSparseHistogram(
    const metrics::MetricSample& sample) {
  CHECK_EQ(metrics::MetricSample::SPARSE_HISTOGRAM, sample.type());
  base::HistogramBase* counter = base::SparseHistogram::FactoryGet(
      sample.name(), base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample.sample());
}

int ExternalMetrics::CollectEvents() {
  ScopedVector<metrics::MetricSample> samples;
  metrics::SerializationUtils::ReadAndTruncateMetricsFromFile(uma_events_file_,
                                                              &samples);

  for (ScopedVector<metrics::MetricSample>::iterator it = samples.begin();
       it != samples.end();
       ++it) {
    const metrics::MetricSample& sample = **it;

    // Do not use the UMA_HISTOGRAM_... macros here.  They cache the Histogram
    // instance and thus only work if |sample.name()| is constant.
    switch (sample.type()) {
      case metrics::MetricSample::CRASH:
        RecordCrash(sample.name());
        break;
      case metrics::MetricSample::USER_ACTION:
        RecordAction(sample.name());
        break;
      case metrics::MetricSample::HISTOGRAM:
        RecordHistogram(sample);
        break;
      case metrics::MetricSample::LINEAR_HISTOGRAM:
        RecordLinearHistogram(sample);
        break;
      case metrics::MetricSample::SPARSE_HISTOGRAM:
        RecordSparseHistogram(sample);
        break;
    }
  }

  return samples.size();
}

void ExternalMetrics::CollectEventsAndReschedule() {
  base::ElapsedTimer timer;
  CollectEvents();
  UMA_HISTOGRAM_TIMES("UMA.CollectExternalEventsTime", timer.Elapsed());
  ScheduleCollector();
}

void ExternalMetrics::ScheduleCollector() {
  bool result;
  result = BrowserThread::PostDelayedTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&chromeos::ExternalMetrics::CollectEventsAndReschedule, this),
      base::TimeDelta::FromSeconds(kExternalMetricsCollectionIntervalSeconds));
  DCHECK(result);
}

void ExternalMetrics::SetupFieldTrialsOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Field trials that do not read from files can be initialized in
  // ExternalMetrics::Start() above.
  SetupProgressiveScanFieldTrial();

  ScheduleCollector();
}

}  // namespace chromeos
