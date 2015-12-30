// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/external_data_use_observer.h"

#include <utility>

#include "base/containers/hash_tables.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "chrome/browser/android/data_usage/data_use_tab_model.h"
#include "chrome/browser/android/data_usage/external_data_use_observer_bridge.h"
#include "components/data_usage/core/data_use.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

namespace android {

namespace {

// Record the result of data use report submission. |bytes| is the sum of send
// and received bytes in the report.
void RecordDataUsageReportSubmission(
    ExternalDataUseObserver::DataUsageReportSubmissionResult result,
    int64_t bytes) {
  DCHECK_LE(0, bytes);
  UMA_HISTOGRAM_ENUMERATION(
      "DataUsage.ReportSubmissionResult", result,
      ExternalDataUseObserver::DATAUSAGE_REPORT_SUBMISSION_MAX);
  // Cap to the maximum sample value.
  const int32_t bytes_capped = bytes <= base::HistogramBase::kSampleType_MAX - 1
                                   ? bytes
                                   : base::HistogramBase::kSampleType_MAX - 1;
  switch (result) {
    case ExternalDataUseObserver::DATAUSAGE_REPORT_SUBMISSION_SUCCESSFUL:
      UMA_HISTOGRAM_COUNTS("DataUsage.ReportSubmission.Bytes.Successful",
                           bytes_capped);
      break;
    case ExternalDataUseObserver::DATAUSAGE_REPORT_SUBMISSION_FAILED:
      UMA_HISTOGRAM_COUNTS("DataUsage.ReportSubmission.Bytes.Failed",
                           bytes_capped);
      break;
    case ExternalDataUseObserver::DATAUSAGE_REPORT_SUBMISSION_TIMED_OUT:
      UMA_HISTOGRAM_COUNTS("DataUsage.ReportSubmission.Bytes.TimedOut",
                           bytes_capped);
      break;
    case ExternalDataUseObserver::DATAUSAGE_REPORT_SUBMISSION_LOST:
      UMA_HISTOGRAM_COUNTS("DataUsage.ReportSubmission.Bytes.Lost",
                           bytes_capped);
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }
}

// Default duration after which matching rules are periodically fetched. May be
// overridden by the field trial.
const int kDefaultFetchMatchingRulesDurationSeconds = 60 * 15;  // 15 minutes.

// Default duration after which a pending data use report is considered timed
// out. May be overridden by the field trial.
const int kDefaultDataUseReportSubmitTimeoutMsec = 60 * 2 * 1000;  // 2 minutes.

// Default value of the minimum number of bytes that should be buffered before
// a data use report is submitted. May be overridden by the field trial.
const int64_t kDefaultDataUseReportMinBytes = 100 * 1024;  // 100 KB.

// Populates various parameters from the values specified in the field trial.
int32_t GetFetchMatchingRulesDurationSeconds() {
  int32_t duration_seconds = -1;
  const std::string variation_value = variations::GetVariationParamValue(
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
int32_t GetDataReportSubmitTimeoutMsec() {
  int32_t duration_seconds = -1;
  const std::string variation_value = variations::GetVariationParamValue(
      chrome::android::ExternalDataUseObserver::
          kExternalDataUseObserverFieldTrial,
      "data_report_submit_timeout_msec");
  if (!variation_value.empty() &&
      base::StringToInt(variation_value, &duration_seconds)) {
    DCHECK_LE(0, duration_seconds);
    return duration_seconds;
  }
  return kDefaultDataUseReportSubmitTimeoutMsec;
}

// Populates various parameters from the values specified in the field trial.
int64_t GetMinBytes() {
  int64_t min_bytes = -1;
  const std::string variation_value = variations::GetVariationParamValue(
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
      data_use_tab_model_(new DataUseTabModel()),
      last_data_report_submitted_ticks_(base::TimeTicks()),
      pending_report_bytes_(0),
      ui_task_runner_(ui_task_runner),
      previous_report_time_(base::Time::Now()),
      last_matching_rules_fetch_time_(base::TimeTicks::Now()),
      external_data_use_observer_bridge_(new ExternalDataUseObserverBridge()),
      total_bytes_buffered_(0),
      fetch_matching_rules_duration_(
          base::TimeDelta::FromSeconds(GetFetchMatchingRulesDurationSeconds())),
      data_use_report_min_bytes_(GetMinBytes()),
      data_report_submit_timeout_(
          base::TimeDelta::FromMilliseconds(GetDataReportSubmitTimeoutMsec())),
#if defined(OS_ANDROID)
      app_state_listener_(new base::android::ApplicationStatusListener(
          base::Bind(&ExternalDataUseObserver::OnApplicationStateChange,
                     base::Unretained(this)))),
#endif
      registered_as_data_use_observer_(false),
      weak_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(data_use_aggregator_);
  DCHECK(io_task_runner);
  DCHECK(ui_task_runner_);
  DCHECK(last_data_report_submitted_ticks_.is_null());

  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&DataUseTabModel::InitOnUIThread,
                                       base::Unretained(data_use_tab_model_),
                                       io_task_runner, GetWeakPtr()));

  // Initialize the ExternalDataUseObserverBridge object. Initialization will
  // also trigger the fetching of matching rules. It is okay to use
  // base::Unretained here since |external_data_use_observer_bridge_| is
  // owned by |this|, and is destroyed on UI thread when |this| is destroyed.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseObserverBridge::Init,
                 base::Unretained(external_data_use_observer_bridge_),
                 io_task_runner, GetWeakPtr(), data_use_tab_model_));
}

ExternalDataUseObserver::~ExternalDataUseObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (registered_as_data_use_observer_)
    data_use_aggregator_->RemoveObserver(this);

  // Delete |external_data_use_observer_bridge_| on the UI thread.
  if (!ui_task_runner_->DeleteSoon(FROM_HERE,
                                   external_data_use_observer_bridge_)) {
    NOTIMPLEMENTED()
        << " ExternalDataUseObserverBridge was not deleted successfully";
  }

  // Delete |data_use_tab_model_| on the UI thread.
  if (!ui_task_runner_->DeleteSoon(FROM_HERE, data_use_tab_model_)) {
    NOTIMPLEMENTED() << " DataUseTabModel was not deleted successfully";
  }
}

void ExternalDataUseObserver::OnReportDataUseDone(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!last_data_report_submitted_ticks_.is_null());

  if (success) {
    RecordDataUsageReportSubmission(DATAUSAGE_REPORT_SUBMISSION_SUCCESSFUL,
                                    pending_report_bytes_);
  } else {
    RecordDataUsageReportSubmission(DATAUSAGE_REPORT_SUBMISSION_FAILED,
                                    pending_report_bytes_);
  }

  last_data_report_submitted_ticks_ = base::TimeTicks();
  pending_report_bytes_ = 0;

  SubmitBufferedDataUseReport(false);
}

#if defined(OS_ANDROID)
void ExternalDataUseObserver::OnApplicationStateChange(
    base::android::ApplicationState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (new_state == base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES)
    SubmitBufferedDataUseReport(true);
}
#endif

void ExternalDataUseObserver::OnDataUse(const data_usage::DataUse& data_use) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(registered_as_data_use_observer_);

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

  scoped_ptr<std::string> label(new std::string());

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DataUseTabModel::GetLabelForTabAtTime,
                 base::Unretained(data_use_tab_model_), data_use.tab_id,
                 data_use.request_start, label.get()),
      base::Bind(&ExternalDataUseObserver::DataUseLabelApplied, GetWeakPtr(),
                 data_use, previous_report_time_, now_time,
                 base::Owned(label.release())));

  previous_report_time_ = now_time;
}

void ExternalDataUseObserver::ShouldRegisterAsDataUseObserver(
    bool should_register) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (registered_as_data_use_observer_ == should_register)
    return;

  if (!registered_as_data_use_observer_ && should_register)
    data_use_aggregator_->AddObserver(this);

  if (registered_as_data_use_observer_ && !should_register)
    data_use_aggregator_->RemoveObserver(this);

  registered_as_data_use_observer_ = should_register;
}

void ExternalDataUseObserver::DataUseLabelApplied(
    const data_usage::DataUse& data_use,
    const base::Time& start_time,
    const base::Time& end_time,
    const std::string* label,
    bool label_applied) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!label_applied)
    return;

  BufferDataUseReport(data_use, *label, start_time, end_time);
  SubmitBufferedDataUseReport(false);
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
  // Skip if the |data_use| does not report any network traffic.
  if (data_use.rx_bytes == 0 && data_use.tx_bytes == 0)
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
      RecordDataUsageReportSubmission(DATAUSAGE_REPORT_SUBMISSION_LOST,
                                      data_use.rx_bytes + data_use.tx_bytes);
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

  DCHECK_LT(0U, buffered_data_reports_.size());
  DCHECK_LE(buffered_data_reports_.size(), kMaxBufferSize);
}

void ExternalDataUseObserver::SubmitBufferedDataUseReport(bool immediate) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const base::TimeTicks ticks_now = base::TimeTicks::Now();

  // Return if a data use report has been pending for less than
  // |data_report_submit_timeout_| duration.
  if (!last_data_report_submitted_ticks_.is_null() &&
      ticks_now - last_data_report_submitted_ticks_ <
          data_report_submit_timeout_) {
    return;
  }

  if (buffered_data_reports_.empty())
    return;

  if (!immediate && total_bytes_buffered_ < data_use_report_min_bytes_)
    return;

  if (!last_data_report_submitted_ticks_.is_null()) {
    // Mark the pending DataUsage report as timed out.
    RecordDataUsageReportSubmission(DATAUSAGE_REPORT_SUBMISSION_TIMED_OUT,
                                    pending_report_bytes_);
    pending_report_bytes_ = 0;
    last_data_report_submitted_ticks_ = base::TimeTicks();
  }

  // Send one data use report.
  DataUseReports::iterator it = buffered_data_reports_.begin();
  DataUseReportKey key = it->first;
  DataUseReport report = it->second;

  DCHECK_EQ(0, pending_report_bytes_);
  DCHECK(last_data_report_submitted_ticks_.is_null());
  pending_report_bytes_ = report.bytes_downloaded + report.bytes_uploaded;
  last_data_report_submitted_ticks_ = ticks_now;

  // Remove the entry from the map.
  buffered_data_reports_.erase(it);
  total_bytes_buffered_ -= (report.bytes_downloaded + report.bytes_uploaded);

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
  return data_use_tab_model_;
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
