// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/external_data_use_observer.h"

#include <utility>

#include "base/containers/hash_tables.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/data_usage/data_use_tab_model.h"
#include "chrome/browser/android/data_usage/external_data_use_observer_bridge.h"
#include "components/data_usage/core/data_use.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Default duration after which matching rules are periodically fetched. May be
// overridden by the field trial.
const int kDefaultFetchMatchingRulesDurationSeconds = 60 * 15;  // 15 minutes.

// Default value of the minimum number of bytes that should be buffered before
// a data use report is submitted. May be overridden by the field trial.
const int64_t kDefaultDataUseReportMinBytes = 100 * 1024;  // 100 KB.

// Populates various parameters from the values specified in the field trial.
int32_t GetFetchMatchingRulesDurationSeconds() {
  int32_t duration_seconds = -1;
  std::string variation_value = variations::GetVariationParamValue(
      chrome::android::ExternalDataUseObserver::
          kExternalDataUseObserverFieldTrial,
      "fetch_matching_rules_duration_seconds");
  if (!variation_value.empty() &&
      base::StringToInt(variation_value, &duration_seconds)) {
    DCHECK_LE(0, duration_seconds);
    return duration_seconds;
  }
  return kDefaultFetchMatchingRulesDurationSeconds;
}

// Populates various parameters from the values specified in the field trial.
int64_t GetMinBytes() {
  int64_t min_bytes = -1;
  std::string variation_value = variations::GetVariationParamValue(
      chrome::android::ExternalDataUseObserver::
          kExternalDataUseObserverFieldTrial,
      "data_use_report_min_bytes");
  if (!variation_value.empty() &&
      base::StringToInt64(variation_value, &min_bytes)) {
    DCHECK_LE(0, min_bytes);
    return min_bytes;
  }
  return kDefaultDataUseReportMinBytes;
}

}  // namespace

namespace chrome {

namespace android {

// static
const char ExternalDataUseObserver::kExternalDataUseObserverFieldTrial[] =
    "ExternalDataUseObserver";

// static
const size_t ExternalDataUseObserver::kMaxBufferSize = 100;

ExternalDataUseObserver::ExternalDataUseObserver(
    data_usage::DataUseAggregator* data_use_aggregator,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
    : data_use_aggregator_(data_use_aggregator),
      matching_rules_fetch_pending_(false),
      submit_data_report_pending_(false),
      ui_task_runner_(ui_task_runner),
      previous_report_time_(base::Time::Now()),
      last_matching_rules_fetch_time_(base::TimeTicks::Now()),
      external_data_use_observer_bridge_(new ExternalDataUseObserverBridge()),
      total_bytes_buffered_(0),
      fetch_matching_rules_duration_(
          base::TimeDelta::FromSeconds(GetFetchMatchingRulesDurationSeconds())),
      data_use_report_min_bytes_(GetMinBytes()),
      weak_factory_(this) {
  DCHECK(data_use_aggregator_);
  DCHECK(io_task_runner);
  DCHECK(ui_task_runner_);

  // Initialize the ExternalDataUseObserverBridge object. Initialization will
  // also trigger the fetching of matching rules. It is okay to use
  // base::Unretained here since |external_data_use_observer_bridge_| is
  // owned by |this|, and is destroyed on UI thread when |this| is destroyed.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseObserverBridge::Init,
                 base::Unretained(external_data_use_observer_bridge_),
                 io_task_runner, GetWeakPtr()));

  // |this| owns and must outlive the |data_use_tab_model_|.
  data_use_tab_model_.reset(new DataUseTabModel(ui_task_runner_));

  matching_rules_fetch_pending_ = true;
  data_use_aggregator_->AddObserver(this);
}

ExternalDataUseObserver::~ExternalDataUseObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());

  data_use_aggregator_->RemoveObserver(this);

  // Delete |external_data_use_observer_bridge_| on the UI thread.
  if (!ui_task_runner_->DeleteSoon(FROM_HERE,
                                   external_data_use_observer_bridge_)) {
    NOTIMPLEMENTED()
        << " ExternalDataUseObserverBridge was not deleted successfully";
  }
}

void ExternalDataUseObserver::FetchMatchingRulesDone(
    const std::vector<std::string>* app_package_name,
    const std::vector<std::string>* domain_path_regex,
    const std::vector<std::string>* label) {
  DCHECK(thread_checker_.CalledOnValidThread());

  data_use_tab_model_->RegisterURLRegexes(app_package_name, domain_path_regex,
                                          label);
  matching_rules_fetch_pending_ = false;
  // Process buffered reports.
}

void ExternalDataUseObserver::OnReportDataUseDone(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(submit_data_report_pending_);

  // TODO(tbansal): If not successful, record UMA.

  submit_data_report_pending_ = false;

  SubmitBufferedDataUseReport();
}

void ExternalDataUseObserver::OnDataUse(const data_usage::DataUse& data_use) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::TimeTicks now_ticks = base::TimeTicks::Now();
  const base::Time now_time = base::Time::Now();

  // If the time when the matching rules were last fetched is more than
  // |fetch_matching_rules_duration_|, fetch them again.
  if (now_ticks - last_matching_rules_fetch_time_ >=
      fetch_matching_rules_duration_) {
    last_matching_rules_fetch_time_ = now_ticks;

    // It is okay to use base::Unretained here since
    // |external_data_use_observer_bridge_| is owned by |this|, and is destroyed
    // on UI thread when |this| is destroyed.
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ExternalDataUseObserverBridge::FetchMatchingRules,
                   base::Unretained(external_data_use_observer_bridge_)));
  }

  if (matching_rules_fetch_pending_) {
    // TODO(tbansal): Buffer reports.
  }

  std::string label;
  if (data_use_tab_model_->GetLabelForDataUse(data_use, &label))
    BufferDataUseReport(data_use, label, previous_report_time_, now_time);

  previous_report_time_ = now_time;

  SubmitBufferedDataUseReport();
}

void ExternalDataUseObserver::BufferDataUseReport(
    const data_usage::DataUse& data_use,
    const std::string& label,
    const base::Time& start_time,
    const base::Time& end_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!label.empty());
  DCHECK_LE(0, data_use.rx_bytes);
  DCHECK_LE(0, data_use.tx_bytes);
  if (data_use.rx_bytes < 0 || data_use.tx_bytes < 0)
    return;

  DataUseReportKey data_use_report_key =
      DataUseReportKey(label, data_use.connection_type, data_use.mcc_mnc);

  DataUseReport report =
      DataUseReport(start_time, end_time, data_use.rx_bytes, data_use.tx_bytes);

  // Check if the |data_use_report_key| is already in the buffered reports.
  DataUseReports::iterator it =
      buffered_data_reports_.find(data_use_report_key);
  if (it == buffered_data_reports_.end()) {
    // Limit the buffer size.
    if (buffered_data_reports_.size() == kMaxBufferSize) {
      // TODO(tbansal): Add UMA to track impact of lost reports.
      return;
    }
    buffered_data_reports_.insert(std::make_pair(data_use_report_key, report));
  } else {
    DataUseReport existing_report = DataUseReport(it->second);
    DataUseReport merged_report = DataUseReport(
        std::min(existing_report.start_time, report.start_time),
        std::max(existing_report.end_time, report.end_time),
        existing_report.bytes_downloaded + report.bytes_downloaded,
        existing_report.bytes_uploaded + report.bytes_uploaded);
    buffered_data_reports_.erase(it);
    buffered_data_reports_.insert(
        std::make_pair(data_use_report_key, merged_report));
  }
  total_bytes_buffered_ += (data_use.rx_bytes + data_use.tx_bytes);

  DCHECK_LE(buffered_data_reports_.size(), kMaxBufferSize);
}

void ExternalDataUseObserver::SubmitBufferedDataUseReport() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (submit_data_report_pending_ || buffered_data_reports_.empty())
    return;

  if (total_bytes_buffered_ < data_use_report_min_bytes_)
    return;

  // Send one data use report.
  DataUseReports::iterator it = buffered_data_reports_.begin();
  DataUseReportKey key = it->first;
  DataUseReport report = it->second;

  // Remove the entry from the map.
  buffered_data_reports_.erase(it);
  total_bytes_buffered_ -= (report.bytes_downloaded + report.bytes_uploaded);

  submit_data_report_pending_ = true;

  // It is okay to use base::Unretained here since
  // |external_data_use_observer_bridge_| is owned by |this|, and is destroyed
  // on UI thread when |this| is destroyed.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseObserverBridge::ReportDataUse,
                 base::Unretained(external_data_use_observer_bridge_),
                 key.label, key.connection_type, key.mcc_mnc, report.start_time,
                 report.end_time, report.bytes_downloaded,
                 report.bytes_uploaded));
}

base::WeakPtr<ExternalDataUseObserver> ExternalDataUseObserver::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

DataUseTabModel* ExternalDataUseObserver::GetDataUseTabModel() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return data_use_tab_model_.get();
}

ExternalDataUseObserver::DataUseReportKey::DataUseReportKey(
    const std::string& label,
    net::NetworkChangeNotifier::ConnectionType connection_type,
    const std::string& mcc_mnc)
    : label(label), connection_type(connection_type), mcc_mnc(mcc_mnc) {}

bool ExternalDataUseObserver::DataUseReportKey::operator==(
    const DataUseReportKey& other) const {
  return label == other.label && connection_type == other.connection_type &&
         mcc_mnc == other.mcc_mnc;
}

ExternalDataUseObserver::DataUseReport::DataUseReport(
    const base::Time& start_time,
    const base::Time& end_time,
    int64_t bytes_downloaded,
    int64_t bytes_uploaded)
    : start_time(start_time),
      end_time(end_time),
      bytes_downloaded(bytes_downloaded),
      bytes_uploaded(bytes_uploaded) {}

size_t ExternalDataUseObserver::DataUseReportKeyHash::operator()(
    const DataUseReportKey& k) const {
  //  The hash is computed by hashing individual variables and combining them
  //  using prime numbers. Prime numbers are used for multiplication because the
  //  number of buckets used by map is always an even number. Using a prime
  //  number ensures that for two different DataUseReportKey objects (say |j|
  //  and |k|), if the hash value of |k.label| is equal to hash value of
  //  |j.mcc_mnc|, then |j| and |k| map to different buckets. Large prime
  //  numbers are used so that hash value is spread over a larger range.
  std::hash<std::string> hash_function;
  size_t hash = 1;
  hash = hash * 23 + hash_function(k.label);
  hash = hash * 43 + k.connection_type;
  hash = hash * 83 + hash_function(k.mcc_mnc);
  return hash;
}

}  // namespace android

}  // namespace chrome
