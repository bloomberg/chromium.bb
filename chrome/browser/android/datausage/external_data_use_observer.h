// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATAUSAGE_EXTERNAL_DATA_USE_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_DATAUSAGE_EXTERNAL_DATA_USE_OBSERVER_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/thread_task_runner_handle.h"
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

  // Called by Java when new matching rules have been fetched. This may be
  // called on a different thread.  |app_package_name| is the package name of
  // the app that should be matched. |domain_path_regex| is the regex to be used
  // for matching URLs. |label| is the label that must be applied to data
  // reports corresponding to the matching rule, and must
  // uniquely identify the matching rule. Each element in |label| must have
  // non-zero length. The three vectors should have equal length. The vectors
  // may be empty which implies that no matching rules are active.
  void FetchMatchingRulesCallback(
      JNIEnv* env,
      jobject obj,
      const base::android::JavaParamRef<jobjectArray>& app_package_name,
      const base::android::JavaParamRef<jobjectArray>& domain_path_regex,
      const base::android::JavaParamRef<jobjectArray>& label);

  // Called by Java when the reporting of data usage has finished.  This may be
  // called on a different thread. |success| is true if the request was
  // successfully submitted to the external data use observer by Java.
  void OnReportDataUseDone(JNIEnv* env, jobject obj, bool success);

 private:
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, SingleRegex);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, TwoRegex);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, MultipleRegex);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, ChangeRegex);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest,
                           AtMostOneDataUseSubmitRequest);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, MultipleMatchingRules);

  // Data report that is sent to the external observer.
  struct DataReport {
    DataReport(const std::string& label,
               int64_t bytes_downloaded,
               int64_t bytes_uploaded)
        : label(label),
          bytes_downloaded(bytes_downloaded),
          bytes_uploaded(bytes_uploaded) {}
    std::string label;
    int64_t bytes_downloaded;
    int64_t bytes_uploaded;
  };

  // Stores the matching rules.
  class MatchingRule {
   public:
    MatchingRule(const std::string& app_package_name,
                 scoped_ptr<re2::RE2> pattern,
                 const std::string& label);
    ~MatchingRule();

    const re2::RE2* pattern() const;
    const std::string& label() const;

   private:
    // Package name of the app that should be matched.
    const std::string app_package_name_;

    // RE2 pattern to match against URLs.
    scoped_ptr<re2::RE2> pattern_;

    // Opaque label that uniquely identifies this matching rule.
    const std::string label_;

    DISALLOW_COPY_AND_ASSIGN(MatchingRule);
  };

  // Maximum buffer size. If an entry needs to be added to the buffer that has
  // size |kMaxBufferSize|, then the oldest entry will be removed.
  static const size_t kMaxBufferSize = 100;

  // data_usage::DataUseAggregator::Observer implementation:
  void OnDataUse(
      const std::vector<data_usage::DataUse>& data_use_sequence) override;

  // Called by |FetchMatchingRulesCallback| on IO thread when new matching rules
  // have been fetched.
  void FetchMatchingRulesCallbackOnIOThread(
      const std::vector<std::string>& app_package_name,
      const std::vector<std::string>& domain_path_regex,
      const std::vector<std::string>& label);

  // Called by |OnReportDataUseDone| on IO thread when a data use report has
  // been submitted.
  void OnReportDataUseDoneOnIOThread(bool success);

  // Called by FetchMatchingRulesCallbackIO to register multiple
  // case-insensitive regular expressions. If the url of the data use request
  // matches any of the regular expression, the observation is passed to the
  // Java listener.
  void RegisterURLRegexes(const std::vector<std::string>& app_package_name,
                          const std::vector<std::string>& domain_path_regex,
                          const std::vector<std::string>& label);

  // Returns true if the |gurl| matches the registered regular expressions.
  // |label| must not be null. If a match is found, the |label| is set to the
  // matching rule's label.
  bool Matches(const GURL& gurl, std::string* label) const;

  // Aggregator that sends data use observations to |this|.
  data_usage::DataUseAggregator* data_use_aggregator_;

  // Java listener that provides regular expressions to |this|. The regular
  // expressions are applied to the request URLs, and filtered data use is
  // notified to |j_external_data_use_observer_|.
  base::android::ScopedJavaGlobalRef<jobject> j_external_data_use_observer_;

  // True if callback from |FetchMatchingRulesCallback| is currently pending.
  bool matching_rules_fetch_pending_;

  // True if callback from |SubmitDataUseReportCallback| is currently pending.
  bool submit_data_report_pending_;

  // Contains matching rules.
  ScopedVector<MatchingRule> matching_rules_;

  // Buffered data reports that need to be submitted to the Java data use
  // observer.
  std::vector<DataReport> buffered_data_reports_;

  // True if |this| is currently registered as a data use observer.
  bool registered_as_observer_;

  // |task_runner_| accesses ExternalDataUseObserver members on IO thread.
  scoped_refptr<base::TaskRunner> task_runner_;

  base::ThreadChecker thread_checker_;

  // |weak_factory_| is used for posting tasks on the IO thread.
  base::WeakPtrFactory<ExternalDataUseObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExternalDataUseObserver);
};

bool RegisterExternalDataUseObserver(JNIEnv* env);

}  // namespace android

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DATAUSAGE_EXTERNAL_DATA_USE_OBSERVER_H_
