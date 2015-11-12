// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/external_data_use_observer.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/data_usage/data_use_tab_model.h"
#include "components/data_usage/core/data_use.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "jni/ExternalDataUseObserver_jni.h"
#include "third_party/re2/re2/re2.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ToJavaArrayOfStrings;

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

const char ExternalDataUseObserver::kExternalDataUseObserverFieldTrial[] =
    "ExternalDataUseObserver";

ExternalDataUseObserver::ExternalDataUseObserver(
    data_usage::DataUseAggregator* data_use_aggregator,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : data_use_aggregator_(data_use_aggregator),
      matching_rules_fetch_pending_(false),
      submit_data_report_pending_(false),
      registered_as_observer_(false),
      io_task_runner_(io_task_runner),
      ui_task_runner_(ui_task_runner),
      previous_report_time_(base::Time::Now()),
      last_matching_rules_fetch_time_(base::TimeTicks::Now()),
      total_bytes_buffered_(0),
      fetch_matching_rules_duration_(
          base::TimeDelta::FromSeconds(GetFetchMatchingRulesDurationSeconds())),
      data_use_report_min_bytes_(GetMinBytes()),
      io_weak_factory_(this),
      ui_weak_factory_(this) {
  DCHECK(data_use_aggregator_);
  DCHECK(io_task_runner_);
  DCHECK(ui_task_runner_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseObserver::CreateJavaObjectOnUIThread,
                 GetUIWeakPtr()));

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseObserver::FetchMatchingRulesOnUIThread,
                 GetUIWeakPtr()));

  // |this| owns and must outlive the |data_use_tab_model_|.
  data_use_tab_model_.reset(new DataUseTabModel(this, ui_task_runner_));

  matching_rules_fetch_pending_ = true;
  data_use_aggregator_->AddObserver(this);
  registered_as_observer_ = true;
}

void ExternalDataUseObserver::CreateJavaObjectOnUIThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  j_external_data_use_observer_.Reset(Java_ExternalDataUseObserver_create(
      env, base::android::GetApplicationContext(),
      reinterpret_cast<intptr_t>(this)));
  DCHECK(!j_external_data_use_observer_.is_null());
}

ExternalDataUseObserver::~ExternalDataUseObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  if (!j_external_data_use_observer_.is_null()) {
    Java_ExternalDataUseObserver_onDestroy(env,
                                           j_external_data_use_observer_.obj());
  }
  if (registered_as_observer_)
    data_use_aggregator_->RemoveObserver(this);
}

void ExternalDataUseObserver::FetchMatchingRulesOnUIThread() const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!j_external_data_use_observer_.is_null());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExternalDataUseObserver_fetchMatchingRules(
      env, j_external_data_use_observer_.obj());
}

void ExternalDataUseObserver::FetchMatchingRulesDone(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jobjectArray>& app_package_name,
    const base::android::JavaParamRef<jobjectArray>& domain_path_regex,
    const base::android::JavaParamRef<jobjectArray>& label) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  // Convert to native objects.
  std::vector<std::string> app_package_name_native;
  std::vector<std::string> domain_path_regex_native;
  std::vector<std::string> label_native;

  if (app_package_name && domain_path_regex && label) {
    base::android::AppendJavaStringArrayToStringVector(
        env, app_package_name, &app_package_name_native);
    base::android::AppendJavaStringArrayToStringVector(
        env, domain_path_regex, &domain_path_regex_native);
    base::android::AppendJavaStringArrayToStringVector(env, label,
                                                       &label_native);
  }

  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseObserver::FetchMatchingRulesDoneOnIOThread,
                 GetIOWeakPtr(), app_package_name_native,
                 domain_path_regex_native, label_native));
}

void ExternalDataUseObserver::FetchMatchingRulesDoneOnIOThread(
    const std::vector<std::string>& app_package_name,
    const std::vector<std::string>& domain_path_regex,
    const std::vector<std::string>& label) {
  DCHECK(thread_checker_.CalledOnValidThread());

  RegisterURLRegexes(app_package_name, domain_path_regex, label);
  matching_rules_fetch_pending_ = false;
  // Process buffered reports.
}

void ExternalDataUseObserver::OnReportDataUseDone(JNIEnv* env,
                                                  jobject obj,
                                                  bool success) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseObserver::OnReportDataUseDoneOnIOThread,
                 GetIOWeakPtr(), success));
}

base::WeakPtr<ExternalDataUseObserver> ExternalDataUseObserver::GetIOWeakPtr() {
  return io_weak_factory_.GetWeakPtr();
}

base::WeakPtr<ExternalDataUseObserver> ExternalDataUseObserver::GetUIWeakPtr() {
  return ui_weak_factory_.GetWeakPtr();
}

void ExternalDataUseObserver::OnReportDataUseDoneOnIOThread(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!buffered_data_reports_.empty());
  DCHECK(submit_data_report_pending_);

  // TODO(tbansal): If not successful, record UMA.

  submit_data_report_pending_ = false;

  SubmitBufferedDataUseReport();
}

void ExternalDataUseObserver::OnDataUse(
    const std::vector<const data_usage::DataUse*>& data_use_sequence) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If the time when the matching rules were last fetched is more than
  // |fetch_matching_rules_duration_|, fetch them again.
  if (base::TimeTicks::Now() - last_matching_rules_fetch_time_ >=
      fetch_matching_rules_duration_) {
    last_matching_rules_fetch_time_ = base::TimeTicks::Now();
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ExternalDataUseObserver::FetchMatchingRulesOnUIThread,
                   GetUIWeakPtr()));
  }

  if (matching_rules_fetch_pending_) {
    // TODO(tbansal): Buffer reports.
  }

  std::string label;

  for (const data_usage::DataUse* data_use : data_use_sequence) {
    if (!Matches(data_use->url, &label))
      continue;

    BufferDataUseReport(data_use, label, previous_report_time_,
                        base::Time::Now());
  }
  previous_report_time_ = base::Time::Now();

  // TODO(tbansal): Post SubmitBufferedDataUseReport on IO thread once the
  // task runners are plumbed in.
  SubmitBufferedDataUseReport();
}

void ExternalDataUseObserver::BufferDataUseReport(
    const data_usage::DataUse* data_use,
    const std::string& label,
    const base::Time& start_time,
    const base::Time& end_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!label.empty());
  DCHECK_LE(0, data_use->rx_bytes);
  DCHECK_LE(0, data_use->tx_bytes);
  if (data_use->rx_bytes < 0 || data_use->tx_bytes < 0)
    return;

  DataUseReportKey data_use_report_key =
      DataUseReportKey(label, data_use->connection_type, data_use->mcc_mnc);

  DataUseReport report = DataUseReport(start_time, end_time, data_use->rx_bytes,
                                       data_use->tx_bytes);

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
  total_bytes_buffered_ += (data_use->rx_bytes + data_use->tx_bytes);

  DCHECK_LE(buffered_data_reports_.size(), static_cast<size_t>(kMaxBufferSize));
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

  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ExternalDataUseObserver::ReportDataUseOnUIThread,
                            GetIOWeakPtr(), key, report));
}

void ExternalDataUseObserver::ReportDataUseOnUIThread(
    const DataUseReportKey& key,
    const DataUseReport& report) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(!j_external_data_use_observer_.is_null());

  // End time should be greater than start time.
  int64_t start_time_milliseconds = report.start_time.ToJavaTime();
  int64_t end_time_milliseconds = report.end_time.ToJavaTime();
  if (start_time_milliseconds >= end_time_milliseconds)
    start_time_milliseconds = end_time_milliseconds - 1;

  Java_ExternalDataUseObserver_reportDataUse(
      env, j_external_data_use_observer_.obj(),
      ConvertUTF8ToJavaString(env, key.label).obj(), key.connection_type,
      ConvertUTF8ToJavaString(env, key.mcc_mnc).obj(), start_time_milliseconds,
      end_time_milliseconds, report.bytes_downloaded, report.bytes_uploaded);
}

void ExternalDataUseObserver::RegisterURLRegexes(
    const std::vector<std::string>& app_package_name,
    const std::vector<std::string>& domain_path_regex,
    const std::vector<std::string>& label) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(app_package_name.size(), domain_path_regex.size());
  DCHECK_EQ(app_package_name.size(), label.size());

  matching_rules_.clear();
  re2::RE2::Options options(re2::RE2::DefaultOptions);
  options.set_case_sensitive(false);

  for (size_t i = 0; i < domain_path_regex.size(); ++i) {
    const std::string& url_regex = domain_path_regex[i];
    if (url_regex.empty())
      continue;
    scoped_ptr<re2::RE2> pattern(new re2::RE2(url_regex, options));
    if (!pattern->ok())
      continue;
    DCHECK(!label[i].empty());
    matching_rules_.push_back(
        new MatchingRule(app_package_name[i], pattern.Pass(), label[i]));
  }

  if (matching_rules_.size() == 0 && registered_as_observer_) {
    // Unregister as an observer if no regular expressions were received.
    data_use_aggregator_->RemoveObserver(this);
    registered_as_observer_ = false;
  } else if (matching_rules_.size() > 0 && !registered_as_observer_) {
    // Register as an observer if regular expressions were received.
    data_use_aggregator_->AddObserver(this);
    registered_as_observer_ = true;
  }
}

bool ExternalDataUseObserver::Matches(const GURL& gurl,
                                      std::string* label) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  *label = "";

  if (!gurl.is_valid() || gurl.is_empty())
    return false;

  for (size_t i = 0; i < matching_rules_.size(); ++i) {
    const re2::RE2* pattern = matching_rules_[i]->pattern();
    if (re2::RE2::FullMatch(gurl.spec(), *pattern)) {
      *label = matching_rules_[i]->label();
      return true;
    }
  }

  return false;
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

ExternalDataUseObserver::MatchingRule::MatchingRule(
    const std::string& app_package_name,
    scoped_ptr<re2::RE2> pattern,
    const std::string& label)
    : app_package_name_(app_package_name),
      pattern_(pattern.Pass()),
      label_(label) {}

ExternalDataUseObserver::MatchingRule::~MatchingRule() {}

const re2::RE2* ExternalDataUseObserver::MatchingRule::pattern() const {
  return pattern_.get();
}

const std::string& ExternalDataUseObserver::MatchingRule::label() const {
  return label_;
}

bool RegisterExternalDataUseObserver(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android

}  // namespace chrome
