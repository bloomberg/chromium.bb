// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/datausage/external_data_use_observer.h"

#include "base/android/jni_string.h"
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
      registered_as_observer_(false) {
  DCHECK(data_use_aggregator_);

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

void ExternalDataUseObserver::FetchMatchingRulesCallback(JNIEnv* env,
                                                         jobject obj) {
  DCHECK(thread_checker_.CalledOnValidThread());

  matching_rules_fetch_pending_ = false;
  // TODO(tbansal): Update local state.
  // Process buffered reports.
}

void ExternalDataUseObserver::SubmitDataUseReportCallback(JNIEnv* env,
                                                          jobject obj) {
  DCHECK(thread_checker_.CalledOnValidThread());

  submit_data_report_pending_ = false;
  // TODO(tbansal): Update local state.
}

void ExternalDataUseObserver::OnDataUse(
    const std::vector<data_usage::DataUse>& data_use_sequence) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!j_external_data_use_observer_.is_null());

  JNIEnv* env = base::android::AttachCurrentThread();

  if (matching_rules_fetch_pending_) {
    // TODO(tbansal): Buffer reports.
  }

  for (const auto& data_use : data_use_sequence) {
    if (!Matches(data_use.url))
      continue;

    std::string tag = "";  // data_use.tab_id;
    int64_t bytes_downloaded = data_use.rx_bytes;
    int64_t bytes_uploaded = data_use.tx_bytes;
    buffered_data_reports_.push_back(
        DataReport(tag, bytes_downloaded, bytes_uploaded));

    // TODO(tbansal): Limit the buffer size.
    if (buffered_data_reports_.size() > static_cast<size_t>(kMaxBufferSize))
      buffered_data_reports_.erase(buffered_data_reports_.begin());
    DCHECK_LE(buffered_data_reports_.size(),
              static_cast<size_t>(kMaxBufferSize));

    if (submit_data_report_pending_)
      continue;

    // TODO(tbansal): Use buffering to avoid frequent JNI calls.
    submit_data_report_pending_ = true;
    DCHECK_GT(buffered_data_reports_.size(), 0U);
    DataReport earliest_report = buffered_data_reports_[0];
    Java_ExternalDataUseObserver_submitDataUseReport(
        env, j_external_data_use_observer_.obj(),
        ConvertUTF8ToJavaString(env, earliest_report.tag).obj(),
        earliest_report.bytes_downloaded, earliest_report.bytes_uploaded);
    buffered_data_reports_.erase(buffered_data_reports_.begin());
  }
}

void ExternalDataUseObserver::RegisterURLRegexes(
    const std::vector<std::string>& url_regexes) {
  DCHECK(thread_checker_.CalledOnValidThread());

  url_patterns_.clear();
  re2::RE2::Options options(re2::RE2::DefaultOptions);
  options.set_case_sensitive(false);

  for (const std::string& url_regex : url_regexes) {
    if (url_regex.empty())
      continue;
    scoped_ptr<re2::RE2> pattern(new re2::RE2(url_regex, options));
    if (!pattern->ok())
      continue;
    url_patterns_.push_back(pattern.Pass());
  }

  if (url_patterns_.size() == 0 && registered_as_observer_) {
    // Unregister as an observer if no regular expressions were received.
    data_use_aggregator_->RemoveObserver(this);
    registered_as_observer_ = false;
  } else if (url_patterns_.size() > 0 && !registered_as_observer_) {
    // Register as an observer if regular expressions were received.
    data_use_aggregator_->AddObserver(this);
    registered_as_observer_ = true;
  }
}

bool ExternalDataUseObserver::Matches(const GURL& gurl) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!gurl.is_valid() || gurl.is_empty())
    return false;

  for (const re2::RE2* pattern : url_patterns_) {
    if (re2::RE2::FullMatch(gurl.spec(), *pattern))
      return true;
  }

  return false;
}

bool RegisterExternalDataUseObserver(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android

}  // namespace chrome
