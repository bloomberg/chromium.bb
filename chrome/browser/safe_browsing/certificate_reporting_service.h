// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/certificate_reporting/error_reporter.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class Clock;
}

// This service initiates uploads of invalid certificate reports and retries any
// failed uploads.
class CertificateReportingService : public KeyedService {
 public:
  // Represent a report to be sent.
  struct Report {
    int report_id;
    base::Time creation_time;
    std::string serialized_report;
    Report(int report_id,
           base::Time creation_time,
           const std::string& serialized_report)
        : report_id(report_id),
          creation_time(creation_time),
          serialized_report(serialized_report) {}
  };

  // This class contains a number of reports, sorted by the first time the
  // report was to be sent. Oldest reports are at the end of the list. The
  // number of reports are bounded by |max_size|. The implementation sorts all
  // items in the list whenever a new item is added. This should be fine for
  // small values of |max_size| (e.g. fewer than 100 items). In case this is not
  // sufficient in the future, an array based implementation should be
  // considered where the array is maintained as a heap.
  class BoundedReportList {
   public:
    explicit BoundedReportList(size_t max_size);
    ~BoundedReportList();

    void Add(const Report& report);
    void Clear();

    const std::vector<Report>& items() const;

   private:
    // Maximum number of reports in the list. If the number of reports in the
    // list is smaller than this number, a new item is immediately added to the
    // list. Otherwise, the item is compared to the items in the list and only
    // added when it's newer than the oldest item in the list.
    const size_t max_size_;

    std::vector<Report> items_;
    base::ThreadChecker thread_checker_;
  };

  // Class that handles report uploads and implements the upload retry logic.
  class Reporter {
   public:
    Reporter(
        std::unique_ptr<certificate_reporting::ErrorReporter> error_reporter_,
        std::unique_ptr<BoundedReportList> retry_list,
        base::Clock* clock,
        base::TimeDelta report_ttl);
    ~Reporter();

    // Sends a report. If the send fails, the report will be added to the retry
    // list.
    void Send(const std::string& serialized_report);

    // Sends all pending reports. Skips reports older than the |report_ttl|
    // provided in the constructor. Failed reports will be added to the retry
    // list.
    void SendPending();

    // Getter and setters for testing:
    size_t inflight_report_count_for_testing() const;
    BoundedReportList* GetQueueForTesting() const;

   private:
    void SendInternal(const Report& report);
    void ErrorCallback(int report_id, const GURL& url, int error);
    void SuccessCallback(int report_id);

    std::unique_ptr<certificate_reporting::ErrorReporter> error_reporter_;
    std::unique_ptr<BoundedReportList> retry_list_;
    base::Clock* test_clock_;
    const base::TimeDelta report_ttl_;
    int current_report_id_;

    std::map<int, Report> inflight_reports_;

    base::WeakPtrFactory<Reporter> weak_factory_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(Reporter);
  };
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_H_
