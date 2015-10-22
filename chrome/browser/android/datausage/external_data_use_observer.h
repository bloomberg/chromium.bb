// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATAUSAGE_EXTERNAL_DATA_USE_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_DATAUSAGE_EXTERNAL_DATA_USE_OBSERVER_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "components/data_usage/core/data_use.h"
#include "components/data_usage/core/data_use_aggregator.h"

namespace re2 {
class RE2;
}

namespace chrome {

namespace android {

// This class allows platform APIs that are external to Chromium to observe how
// much data is used by Chromium on the current Android device. It creates and
// owns a Java listener object that is notified of the data usage observations
// of Chromium. This class receives regular expressions from the Java listener
// object. It also registers as a data use observer with DataUseAggregator,
// filters the received observations by applying the regex matching to the URLs
// of the requests, and notifies the filtered data use to the Java listener. The
// Java object in turn may notify the platform APIs of the data usage
// observations.
class ExternalDataUseObserver : public data_usage::DataUseAggregator::Observer {
 public:
  explicit ExternalDataUseObserver(
      data_usage::DataUseAggregator* data_use_aggregator);
  ~ExternalDataUseObserver() override;

  // Called by Java when new matching rules have been fetched.
  void FetchMatchingRulesCallback(JNIEnv* env, jobject obj);

  // Called by Java when the sending of data usage report has finished.
  void SubmitDataUseReportCallback(JNIEnv* env, jobject obj);

 private:
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, SingleRegex);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, TwoRegex);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, MultipleRegex);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, ChangeRegex);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest,
                           AtMostOneDataUseSubmitRequest);

  // Data report that is sent to the external observer.
  struct DataReport {
    DataReport(const std::string& tag,
               int64_t bytes_downloaded,
               int64_t bytes_uploaded)
        : tag(tag),
          bytes_downloaded(bytes_downloaded),
          bytes_uploaded(bytes_uploaded) {}
    std::string tag;
    int64_t bytes_downloaded;
    int64_t bytes_uploaded;
  };

  // Maximum buffer size. If an entry needs to be added to the buffer that has
  // size |kMaxBufferSize|, then the oldest entry will be removed.
  static const size_t kMaxBufferSize = 100;

  // data_usage::DataUseAggregator::Observer implementation:
  void OnDataUse(
      const std::vector<data_usage::DataUse>& data_use_sequence) override;

  // Called by FetchMatchingRulesCallback to register multiple
  // case-insensitive regular expressions. If the url of the data use request
  // matches any of the regular expression, the observation is passed to the
  // Java listener.
  void RegisterURLRegexes(const std::vector<std::string>& url_regexes);

  // Returns true if the |gurl| matches the registered regular expressions.
  bool Matches(const GURL& gurl) const;

  // Aggregator that sends data use observations.
  data_usage::DataUseAggregator* data_use_aggregator_;

  // Java listener that provides regular expressions to this class. The
  // regular expressions are applied to the request URLs, and filtered
  // data use is notified to |j_external_data_use_observer_|.
  base::android::ScopedJavaGlobalRef<jobject> j_external_data_use_observer_;

  // True if callback from |FetchMatchingRulesCallback| is currently pending.
  bool matching_rules_fetch_pending_;

  // True if callback from |SubmitDataUseReportCallback| is currently pending.
  bool submit_data_report_pending_;

  // Registered RE2 patterns that are matched against the URLs.
  ScopedVector<re2::RE2> url_patterns_;

  // Buffered data reports that need to be submitted to the Java data use
  // observer.
  std::vector<DataReport> buffered_data_reports_;

  // True if |this| is currently registered as a data use observer.
  bool registered_as_observer_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ExternalDataUseObserver);
};

bool RegisterExternalDataUseObserver(JNIEnv* env);

}  // namespace android

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DATAUSAGE_EXTERNAL_DATA_USE_OBSERVER_H_
