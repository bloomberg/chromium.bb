// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATA_USAGE_EXTERNAL_DATA_USE_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_DATA_USAGE_EXTERNAL_DATA_USE_OBSERVER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/data_usage/core/data_use_aggregator.h"
#include "net/base/network_change_notifier.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}

namespace data_usage {
struct DataUse;
}

namespace chrome {

namespace android {

class DataUseTabModel;
class ExternalDataUseObserverBridge;

// This class allows platform APIs that are external to Chromium to observe how
// much data is used by Chromium on the current Android device. This class
// registers as a data use observer with DataUseAggregator (as long as there is
// at least one valid matching rule is present), filters the received
// observations by applying the regex matching to the URLs of the requests, and
// notifies the filtered data use to the platform. This class is not thread
// safe, and must only be accessed on IO thread.
class ExternalDataUseObserver : public data_usage::DataUseAggregator::Observer {
 public:
  // Result of data usage report submission.  This enum must remain synchronized
  // with the enum of the same name in metrics/histograms/histograms.xml.
  enum DataUsageReportSubmissionResult {
    // Submission of data use report to the external observer was successful.
    DATAUSAGE_REPORT_SUBMISSION_SUCCESSFUL = 0,
    // Submission of data use report to the external observer returned error.
    DATAUSAGE_REPORT_SUBMISSION_FAILED = 1,
    // Submission of data use report to the external observer timed out.
    DATAUSAGE_REPORT_SUBMISSION_TIMED_OUT = 2,
    // Data use report was lost before an attempt was made to submit it.
    DATAUSAGE_REPORT_SUBMISSION_LOST = 3,
    DATAUSAGE_REPORT_SUBMISSION_MAX = 4
  };

  // External data use observer field trial name.
  static const char kExternalDataUseObserverFieldTrial[];

  ExternalDataUseObserver(
      data_usage::DataUseAggregator* data_use_aggregator,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner);
  ~ExternalDataUseObserver() override;

  // Returns the pointer to the DataUseTabModel object owned by |this|. The
  // caller does not owns the returned pointer.
  DataUseTabModel* GetDataUseTabModel() const;

  // Called by ExternalDataUseObserverBridge::OnReportDataUseDone when a data
  // use report has been submitted. |success| is true if the request was
  // successfully submitted to the external data use observer by Java.
  void OnReportDataUseDone(bool success);

  // Called by ExternalDataUseObserverBridge. |should_register| is true if
  // |this| should register as a data use observer.
  void ShouldRegisterAsDataUseObserver(bool should_register);

  // Fetches the matching rules asynchronously.
  void FetchMatchingRules();

  base::WeakPtr<ExternalDataUseObserver> GetWeakPtr();

 private:
  friend class DataUseTabModelTest;
  friend class DataUseUITabModelTest;
  friend class ExternalDataUseObserverTest;
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, BufferDataUseReports);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, BufferSize);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, DataUseReportTimedOut);
#if defined(OS_ANDROID)
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest,
                           DataUseReportingOnApplicationStatusChange);
#endif
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, HashFunction);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest,
                           MatchingRuleFetchOnControlAppInstall);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, MultipleMatchingRules);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest,
                           PeriodicFetchMatchingRules);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest,
                           RegisteredAsDataUseObserver);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest, ReportsMergedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(DataUseUITabModelTest, ReportTabEventsTest);
  FRIEND_TEST_ALL_PREFIXES(ExternalDataUseObserverTest,
                           TimestampsMergedCorrectly);
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

  // Maximum buffer size. If an entry needs to be added to the buffer that has
  // size |kMaxBufferSize|, then the oldest entry will be removed.
  static const size_t kMaxBufferSize;

  // data_usage::DataUseAggregator::Observer implementation:
  void OnDataUse(const data_usage::DataUse& data_use) override;

  // Called by DataUseTabModel when a label has been applied to the |data_use|
  // object. |label_applied| is true if a label can be applied to the |data_use|
  // object. |label| is owned by the caller.
  void DataUseLabelApplied(const data_usage::DataUse& data_use,
                           const base::Time& start_time,
                           const base::Time& end_time,
                           const std::string* label,
                           bool label_applied);

  // Adds |data_use| to buffered reports. |data_use| is the data use report
  // received from DataUseAggregator. |label| is a non-empty label that applies
  // to |data_use|. |start_time| and |end_time| are the start, and end times of
  // the interval during which bytes reported in |data_use| went over the
  // network.
  void BufferDataUseReport(const data_usage::DataUse& data_use,
                           const std::string& label,
                           const base::Time& start_time,
                           const base::Time& end_time);

  // Submits the first data report among the buffered data reports in
  // |buffered_data_reports_|. Since an unordered map is used to buffer the
  // reports, the order of reports may change. The reports are buffered in an
  // arbitrary order and there are no guarantees that the next report to be
  // submitted is the oldest one buffered. |immediate| indicates whether to
  // submit the report immediately or to wait until |data_use_report_min_bytes_|
  // unreported bytes are buffered.
  void SubmitBufferedDataUseReport(bool immediate);

#if defined(OS_ANDROID)
  // Called whenever the application transitions from foreground to background
  // or vice versa.
  void OnApplicationStateChange(base::android::ApplicationState new_state);
#endif

  // Aggregator that sends data use observations to |this|.
  data_usage::DataUseAggregator* data_use_aggregator_;

  // |external_data_use_observer_bridge_| is owned by |this|, and interacts with
  // the Java code. It is created on IO thread but afterwards, should only be
  // accessed on UI thread.
  ExternalDataUseObserverBridge* external_data_use_observer_bridge_;

  // Maintains tab sessions and is owned by |this|. It is created on IO thread
  // but afterwards, should only be accessed on UI thread.
  DataUseTabModel* data_use_tab_model_;

  // Time when the currently pending data use report was submitted.
  // |last_data_report_submitted_ticks_| is null if no data use report is
  // currently pending.
  base::TimeTicks last_data_report_submitted_ticks_;

  // |pending_report_bytes_| is the total byte count in the data use report that
  // is currently pending.
  int64_t pending_report_bytes_;

  // Buffered data reports that need to be submitted to the
  // |external_data_use_observer_bridge_|.
  DataUseReports buffered_data_reports_;

  // |ui_task_runner_| is used to call methods on UI thread.
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

  // If a data use report is pending for more than |data_report_submit_timeout_|
  // duration, it is considered as timed out.
  const base::TimeDelta data_report_submit_timeout_;

#if defined(OS_ANDROID)
  // Listens to when Chromium gets backgrounded and submits buffered data use
  // reports.
  std::unique_ptr<base::android::ApplicationStatusListener> app_state_listener_;
#endif

  // True if |this| is currently registered as a data use observer.
  bool registered_as_data_use_observer_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<ExternalDataUseObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExternalDataUseObserver);
};

}  // namespace android

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DATA_USAGE_EXTERNAL_DATA_USE_OBSERVER_H_
