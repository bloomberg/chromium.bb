// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATA_USAGE_EXTERNAL_DATA_USE_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_DATA_USAGE_EXTERNAL_DATA_USE_OBSERVER_H_

#include <jni.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/data_usage/core/data_use_aggregator.h"
#include "net/base/network_change_notifier.h"

class GURL;

namespace data_usage {
struct DataUse;
}

namespace re2 {
class RE2;
}

namespace chrome {

namespace android {

class DataUseTabModel;

// This class allows platform APIs that are external to Chromium to observe how
// much data is used by Chromium on the current Android device. It creates and
// owns a Java listener object that is notified of the data usage observations
// of Chromium. This class receives regular expressions from the Java listener
// object. It also registers as a data use observer with DataUseAggregator,
// filters the received observations by applying the regex matching to the URLs
// of the requests, and notifies the filtered data use to the Java listener. The
// Java object in turn may notify the platform APIs of the data usage
// observations.
// TODO(tbansal): Create an inner class that manages the UI and IO threads.
class ExternalDataUseObserver : public data_usage::DataUseAggregator::Observer {
 public:
  // External data use observer field trial name.
  static const char kExternalDataUseObserverFieldTrial[];

  ExternalDataUseObserver(
      data_usage::DataUseAggregator* data_use_aggregator,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~ExternalDataUseObserver() override;

  // Called by Java when new matching rules have been fetched. This may be
  // called on a different thread.  |app_package_name| is the package name of
  // the app that should be matched. |domain_path_regex| is the regex to be used
  // for matching URLs. |label| is the label that must be applied to data
  // reports corresponding to the matching rule, and must
  // uniquely identify the matching rule. Each element in |label| must have
  // non-zero length. The three vectors should have equal length. The vectors
  // may be empty which implies that no matching rules are active. Must be
  // called on UI thread.
  void FetchMatchingRulesDone(
      JNIEnv* env,
      jobject obj,
      const base::android::JavaParamRef<jobjectArray>& app_package_name,
      const base::android::JavaParamRef<jobjectArray>& domain_path_regex,
      const base::android::JavaParamRef<jobjectArray>& label);

  // Called by Java when the reporting of data usage has finished.  This may be
  // called on a different thread. |success| is true if the request was
  // successfully submitted to the external data use observer by Java. Must be
  // called on UI thread.
  void OnReportDataUseDone(JNIEnv* env, jobject obj, bool success);

  // Returns true if the |gurl| matches the registered regular expressions.
  // |label| must not be null. If a match is found, the |label| is set to the
  // matching rule's label.
  bool Matches(const GURL& gurl, std::string* label) const;

  DataUseTabModel* data_use_tab_model() const {
    return data_use_tab_model_.get();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, SingleRegex);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, TwoRegex);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, MultipleRegex);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, ChangeRegex);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest,
                           AtMostOneDataUseSubmitRequest);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, MultipleMatchingRules);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, ReportsMergedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest,
                           TimestampsMergedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, HashFunction);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, BufferSize);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest,
                           PeriodicFetchMatchingRules);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, BufferDataUseReports);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, Variations);

  // DataUseReportKey is a unique identifier for a data use report.
  struct DataUseReportKey {
    DataUseReportKey(const std::string& label,
                     net::NetworkChangeNotifier::ConnectionType connection_type,
                     const std::string& mcc_mnc);

    bool operator==(const DataUseReportKey& other) const;

    // Label provided by the matching rules.
    const std::string label;

    // Type of network used by the request.
    const net::NetworkChangeNotifier::ConnectionType connection_type;

    // mcc_mnc operator of the provider of the SIM as obtained from
    // TelephonyManager#getNetworkOperator() Java API in Android.
    const std::string mcc_mnc;
  };

  // DataUseReport is paired with a  DataUseReportKey object. DataUseReport
  // contains the bytes send/received during a specific interval. Only the bytes
  // from the data use reports that have the |label|, |connection_type|, and
  // |mcc_mnc| specified in the corresponding DataUseReportKey object are
  // counted in the DataUseReport.
  struct DataUseReport {
    // |start_time| and |end_time| are the start and end timestamps (in UTC
    // since the standard Java epoch of 1970-01-01 00:00:00) of the interval
    // that this data report covers. |bytes_downloaded| and |bytes_uploaded| are
    // the total bytes received and send during this interval.
    DataUseReport(const base::Time& start_time,
                  const base::Time& end_time,
                  int64_t bytes_downloaded,
                  int64_t bytes_uploaded);

    // Start time of |this| data report (in UTC since the standard Java epoch of
    // 1970-01-01 00:00:00).
    const base::Time start_time;

    // End time of |this| data report (in UTC since the standard Java epoch of
    // 1970-01-01 00:00:00)
    const base::Time end_time;

    // Number of bytes downloaded and uploaded by Chromium from |start_time| to
    // |end_time|.
    const int64_t bytes_downloaded;
    const int64_t bytes_uploaded;
  };

  // Class that implements hash operator on DataUseReportKey.
  class DataUseReportKeyHash {
   public:
    // A simple heuristical hash function that satisifes the property that two
    // equal data structures have the same hash value.
    size_t operator()(const DataUseReportKey& k) const;
  };

  typedef base::hash_map<DataUseReportKey, DataUseReport, DataUseReportKeyHash>
      DataUseReports;

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

  // Creates Java object. Must be called on the UI thread.
  void CreateJavaObjectOnUIThread();

  // data_usage::DataUseAggregator::Observer implementation:
  void OnDataUse(const std::vector<const data_usage::DataUse*>&
                     data_use_sequence) override;

  // Fetches matching rules from Java. Must be called on the UI thread. Returns
  // result asynchronously on UI thread via FetchMatchingRulesDone.
  void FetchMatchingRulesOnUIThread() const;

  // Called by FetchMatchingRulesDone on IO thread when new matching rules
  // Adds |data_use| to buffered reports. |data_use| is the data use report
  // received from DataUseAggregator. |data_use| should not be null. |label| is
  // a non-empty label that applies to |data_use|. |start_time| and |end_time|
  // are the start, and end times of the interval during which bytes reported in
  // |data_use| went over the network.
  void BufferDataUseReport(const data_usage::DataUse* data_use,
                           const std::string& label,
                           const base::Time& start_time,
                           const base::Time& end_time);

  // Submits the first data report among the buffered data reports in
  // |buffered_data_reports_|. Since an unordered_map is used to buffer the
  // reports, the order of reports may change. The reports are buffered in an
  // arbitrary order and there are no guarantees that the next report to be
  // submitted is the oldest one buffered.
  void SubmitBufferedDataUseReport();

  // Called by FetchMatchingRulesDone on IO thread when new matching rules have
  // been fetched.
  void FetchMatchingRulesDoneOnIOThread(
      const std::vector<std::string>& app_package_name,
      const std::vector<std::string>& domain_path_regex,
      const std::vector<std::string>& label);

  // Reports data use to Java. Must be called on the UI thread. Returns
  // result asynchronously on UI thread via OnReportDataUseDone.
  void ReportDataUseOnUIThread(const DataUseReportKey& key,
                               const DataUseReport& report) const;

  // Called by OnReportDataUseDone on IO thread when a data use report has
  // been submitted.
  void OnReportDataUseDoneOnIOThread(bool success);

  // Called by FetchMatchingRulesDoneOnIOThread to register multiple
  // case-insensitive regular expressions. If the url of the data use request
  // matches any of the regular expression, the observation is passed to the
  // Java listener.
  void RegisterURLRegexes(const std::vector<std::string>& app_package_name,
                          const std::vector<std::string>& domain_path_regex,
                          const std::vector<std::string>& label);

  // Return the weak pointer to |this| to be used on IO and UI thread,
  // respectively.
  base::WeakPtr<ExternalDataUseObserver> GetIOWeakPtr();
  base::WeakPtr<ExternalDataUseObserver> GetUIWeakPtr();

  // Aggregator that sends data use observations to |this|.
  data_usage::DataUseAggregator* data_use_aggregator_;

  // Java listener that provides regular expressions to |this|. The regular
  // expressions are applied to the request URLs, and filtered data use is
  // notified to |j_external_data_use_observer_|.
  base::android::ScopedJavaGlobalRef<jobject> j_external_data_use_observer_;

  // Maintains tab sessions.
  scoped_ptr<DataUseTabModel> data_use_tab_model_;

  // True if callback from |FetchMatchingRulesDone| is currently pending.
  bool matching_rules_fetch_pending_;

  // True if callback from |SubmitDataUseReportCallback| is currently pending.
  bool submit_data_report_pending_;

  // Contains matching rules.
  ScopedVector<MatchingRule> matching_rules_;

  // Buffered data reports that need to be submitted to the Java data use
  // observer.
  DataUseReports buffered_data_reports_;

  // True if |this| is currently registered as a data use observer.
  bool registered_as_observer_;

  // |io_task_runner_| accesses ExternalDataUseObserver members on IO thread.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // |ui_task_runner_| is used to call Java code on UI thread. This ensures
  // that Java code is safely called only on a single thread, and eliminates
  // the need for locks in Java.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Time when the data use reports were last received from DataUseAggregator.
  base::Time previous_report_time_;

  // Time when the matching rules were last fetched.
  base::TimeTicks last_matching_rules_fetch_time_;

  // Total number of bytes transmitted or received across all the buffered
  // reports.
  int64_t total_bytes_buffered_;

  // Duration after which matching rules are periodically fetched.
  const base::TimeDelta fetch_matching_rules_duration_;

  // Minimum number of bytes that should be buffered before a data use report is
  // submitted.
  const int64_t data_use_report_min_bytes_;

  base::ThreadChecker thread_checker_;

  // |io_weak_factory_| and |ui_weak_factory_| are used for posting tasks on the
  // IO and UI thread, respectively.
  base::WeakPtrFactory<ExternalDataUseObserver> io_weak_factory_;
  base::WeakPtrFactory<ExternalDataUseObserver> ui_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExternalDataUseObserver);
};

bool RegisterExternalDataUseObserver(JNIEnv* env);

}  // namespace android

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DATA_USAGE_EXTERNAL_DATA_USE_OBSERVER_H_
