// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_SERVICE_H_
#define NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/socket/next_proto.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class ReportingService;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace features {
extern const base::Feature NET_EXPORT kNetworkErrorLogging;
}  // namespace features

namespace net {

class NetworkErrorLoggingDelegate;

class NET_EXPORT NetworkErrorLoggingService {
 public:
  // The details of a network error that are included in an NEL report.
  //
  // See http://wicg.github.io/network-error-logging/#dfn-network-error-object
  // for details on the semantics of each field.
  struct NET_EXPORT RequestDetails {
    RequestDetails();
    RequestDetails(const RequestDetails& other);
    ~RequestDetails();

    GURL uri;
    GURL referrer;
    IPAddress server_ip;
    NextProto protocol;
    int status_code;
    base::TimeDelta elapsed_time;
    Error type;

    bool is_reporting_upload;
  };

  static const char kHeaderName[];

  static const char kReportType[];

  // Keys for data included in report bodies. Exposed for tests.

  static const char kUriKey[];
  static const char kReferrerKey[];
  static const char kSamplingFractionKey[];
  static const char kServerIpKey[];
  static const char kProtocolKey[];
  static const char kStatusCodeKey[];
  static const char kElapsedTimeKey[];
  static const char kTypeKey[];

  static std::unique_ptr<NetworkErrorLoggingService> Create(
      std::unique_ptr<NetworkErrorLoggingDelegate> delegate);

  virtual ~NetworkErrorLoggingService();

  // Ingests a "NEL:" header received from |orogin| with normalized value
  // |value|. May or may not actually set a policy for that origin.
  virtual void OnHeader(const url::Origin& origin,
                        const std::string& value) = 0;

  // Considers queueing a network error report for the request described in
  // |details|. Note that Network Error Logging can report a fraction of
  // successful requests as well (to calculate error rates), so this should be
  // called on *all* requests.
  virtual void OnRequest(const RequestDetails& details) = 0;

  // Removes browsing data (origin policies) associated with any origin for
  // which |origin_filter| returns true, or for all origins if
  // |origin_filter.is_null()|.
  virtual void RemoveBrowsingData(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter) = 0;

  // Sets the ReportingService that will be used to queue network error reports.
  // If |nullptr| is passed, reports will be queued locally or discarded.
  // |reporting_service| must outlive the NetworkErrorLoggingService.
  void SetReportingService(ReportingService* reporting_service);

  // Sets a base::TickClock (used to track policy expiration) for tests.
  // |tick_clock| must outlive the NetworkErrorLoggingService, and cannot be
  // nullptr.
  void SetTickClockForTesting(base::TickClock* tick_clock);

 protected:
  NetworkErrorLoggingService();

  // Unowned:
  base::TickClock* tick_clock_;
  ReportingService* reporting_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkErrorLoggingService);
};

}  // namespace net

#endif  // NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_SERVICE_H_
