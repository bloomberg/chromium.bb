// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_REPORT_H_
#define NET_REPORTING_REPORTING_REPORT_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace net {

// An undelivered report.
struct NET_EXPORT ReportingReport {
 public:
  // Used in histograms; please add new items at end and do not reorder.
  enum class Outcome {
    UNKNOWN = 0,
    DISCARDED_NO_URL_REQUEST_CONTEXT = 1,
    DISCARDED_NO_REPORTING_SERVICE = 2,
    ERASED_FAILED = 3,
    ERASED_EXPIRED = 4,
    ERASED_EVICTED = 5,
    ERASED_NETWORK_CHANGED = 6,
    ERASED_BROWSING_DATA_REMOVED = 7,
    ERASED_REPORTING_SHUT_DOWN = 8,
    DELIVERED = 9,
    ERASED_NO_BACKGROUND_SYNC_PERMISSION = 10,

    MAX
  };

  ReportingReport(const GURL& url,
                  const std::string& group,
                  const std::string& type,
                  std::unique_ptr<const base::Value> body,
                  base::TimeTicks queued,
                  int attempts);
  ~ReportingReport();

  static void RecordReportDiscardedForNoURLRequestContext();
  static void RecordReportDiscardedForNoReportingService();

  void RecordOutcome(base::TimeTicks now);

  // The URL of the document that triggered the report. (Included in the
  // delivered report.)
  GURL url;

  // The endpoint group that should be used to deliver the report. (Not included
  // in the delivered report.)
  std::string group;

  // The type of the report. (Included in the delivered report.)
  std::string type;

  // The body of the report. (Included in the delivered report.)
  std::unique_ptr<const base::Value> body;

  // When the report was queued. (Included in the delivered report as an age
  // relative to the time of the delivery attempt.)
  base::TimeTicks queued;

  // The number of delivery attempts made so far, not including an active
  // attempt. (Not included in the delivered report.)
  int attempts = 0;

  Outcome outcome;

 private:
  bool recorded_outcome;

  DISALLOW_COPY_AND_ASSIGN(ReportingReport);
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_REPORT_H_
