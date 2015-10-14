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
    : data_use_aggregator_(data_use_aggregator) {
  DCHECK(data_use_aggregator_);

  JNIEnv* env = base::android::AttachCurrentThread();
  j_external_data_use_observer_.Reset(Java_ExternalDataUseObserver_create(
      env, base::android::GetApplicationContext(),
      reinterpret_cast<intptr_t>(this)));
  DCHECK(!j_external_data_use_observer_.is_null());
}

ExternalDataUseObserver::~ExternalDataUseObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());

  JNIEnv* env = base::android::AttachCurrentThread();

  Java_ExternalDataUseObserver_onDestroy(env,
                                         j_external_data_use_observer_.obj());

  if (url_patterns_.size() > 0)
    data_use_aggregator_->RemoveObserver(this);
}

bool ExternalDataUseObserver::Matches(const GURL& gurl) const {
  if (!gurl.is_valid() || gurl.is_empty())
    return false;

  for (const re2::RE2* pattern : url_patterns_) {
    if (re2::RE2::FullMatch(gurl.spec(), *pattern))
      return true;
  }

  return false;
}

void ExternalDataUseObserver::OnDataUse(
    const std::vector<data_usage::DataUse>& data_use_sequence) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!j_external_data_use_observer_.is_null());

  JNIEnv* env = base::android::AttachCurrentThread();

  for (const auto& data_use : data_use_sequence) {
    if (!Matches(data_use.url))
      continue;

    // TODO(tbansal): Use buffering to avoid frequent JNI calls.
    std::string tag = "";  // data_use.tab_id;
    int64_t bytes_downloaded = data_use.rx_bytes;
    int64_t bytes_uploaded = data_use.tx_bytes;
    Java_ExternalDataUseObserver_onDataUse(
        env, j_external_data_use_observer_.obj(),
        ConvertUTF8ToJavaString(env, tag).obj(), bytes_downloaded,
        bytes_uploaded);
  }
}

void ExternalDataUseObserver::RegisterURLRegexes(
    const std::vector<std::string>& url_regexes) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool registered_as_observer = url_patterns_.size() > 0;
  if (!registered_as_observer && url_regexes.size() > 0) {
    registered_as_observer = true;
    data_use_aggregator_->AddObserver(this);
  }

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

  // Unregister as an observer if no regular expressions were received.
  if (url_patterns_.size() == 0 && registered_as_observer)
    data_use_aggregator_->RemoveObserver(this);
}

bool RegisterExternalDataUseObserver(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android

}  // namespace chrome
