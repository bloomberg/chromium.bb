// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/datausage/external_data_use_observer.h"

#include "base/android/jni_string.h"
#include "base/message_loop/message_loop.h"
#include "components/data_usage/core/data_use.h"
#include "content/public/browser/browser_thread.h"
#include "jni/ExternalDataUseObserver_jni.h"
#include "third_party/re2/re2/re2.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;

namespace chrome {

namespace android {

ExternalDataUseObserver::ExternalDataUseObserver(
    data_usage::DataUseAggregator* data_use_aggregator)
    : data_use_aggregator_(data_use_aggregator),
      matching_rules_fetch_pending_(false),
      submit_data_report_pending_(false),
      registered_as_observer_(false),
      task_runner_(nullptr),
      weak_factory_(this) {
  DCHECK(data_use_aggregator_);

  // TODO(tbansal): Remove this check.
  if (base::MessageLoop::current())
    task_runner_ = base::MessageLoop::current()->task_runner();

  JNIEnv* env = base::android::AttachCurrentThread();
  j_external_data_use_observer_.Reset(Java_ExternalDataUseObserver_create(
      env, base::android::GetApplicationContext(),
      reinterpret_cast<intptr_t>(this)));
  DCHECK(!j_external_data_use_observer_.is_null());

  if (Java_ExternalDataUseObserver_fetchMatchingRules(
          env, j_external_data_use_observer_.obj())) {
    matching_rules_fetch_pending_ = true;
    data_use_aggregator_->AddObserver(this);
    registered_as_observer_ = true;
  }
}

ExternalDataUseObserver::~ExternalDataUseObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());

  JNIEnv* env = base::android::AttachCurrentThread();

  Java_ExternalDataUseObserver_onDestroy(env,
                                         j_external_data_use_observer_.obj());

  if (registered_as_observer_)
    data_use_aggregator_->RemoveObserver(this);
}

void ExternalDataUseObserver::FetchMatchingRulesCallback(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jobjectArray>& app_package_name,
    const base::android::JavaParamRef<jobjectArray>& domain_path_regex,
    const base::android::JavaParamRef<jobjectArray>& label) {
  if (!task_runner_)
    return;

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

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseObserver::FetchMatchingRulesCallbackOnIOThread,
                 weak_factory_.GetWeakPtr(), app_package_name_native,
                 domain_path_regex_native, label_native));
}

void ExternalDataUseObserver::FetchMatchingRulesCallbackOnIOThread(
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
  if (!task_runner_)
    return;

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseObserver::OnReportDataUseDoneOnIOThread,
                 weak_factory_.GetWeakPtr(), success));
}

void ExternalDataUseObserver::OnReportDataUseDoneOnIOThread(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!buffered_data_reports_.empty());
  DCHECK(submit_data_report_pending_);

  // TODO(tbansal): If not successful, retry.

  submit_data_report_pending_ = false;

  // TODO(tbansal): Submit one more report from |buffered_data_reports_|.
}

void ExternalDataUseObserver::OnDataUse(
    const std::vector<const data_usage::DataUse*>& data_use_sequence) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!j_external_data_use_observer_.is_null());

  JNIEnv* env = base::android::AttachCurrentThread();

  if (matching_rules_fetch_pending_) {
    // TODO(tbansal): Buffer reports.
  }

  std::string label;
  for (const data_usage::DataUse* data_use : data_use_sequence) {
    if (!Matches(data_use->url, &label))
      continue;

    int64_t bytes_downloaded = data_use->rx_bytes;
    int64_t bytes_uploaded = data_use->tx_bytes;
    buffered_data_reports_.push_back(
        DataReport(label, bytes_downloaded, bytes_uploaded));

    // Limit the buffer size.
    if (buffered_data_reports_.size() > static_cast<size_t>(kMaxBufferSize)) {
      // TODO(tbansal): Add UMA to track impact of lost reports.
      buffered_data_reports_.erase(buffered_data_reports_.begin());
    }

    DCHECK_LE(buffered_data_reports_.size(),
              static_cast<size_t>(kMaxBufferSize));

    if (submit_data_report_pending_)
      continue;

    // TODO(tbansal): Use buffering to avoid frequent JNI calls.
    submit_data_report_pending_ = true;
    DCHECK_GT(buffered_data_reports_.size(), 0U);
    DataReport earliest_report = buffered_data_reports_[0];
    // TODO(tbansal): Get real values, instead of using 0s.
    Java_ExternalDataUseObserver_reportDataUse(
        env, j_external_data_use_observer_.obj(),
        ConvertUTF8ToJavaString(env, earliest_report.label).obj(), 0,
        ConvertUTF8ToJavaString(env, "").obj(), 0, 0,
        earliest_report.bytes_downloaded, earliest_report.bytes_uploaded);
    buffered_data_reports_.erase(buffered_data_reports_.begin());
  }
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
