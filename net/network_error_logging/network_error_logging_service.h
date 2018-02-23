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

namespace base {
class Value;
}  // namespace base

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

  static std::unique_ptr<NetworkErrorLoggingService> Create();

  virtual ~NetworkErrorLoggingService();

  // Ingests a "NEL:" header received from |orogin| with normalized value
  // |value|. May or may not actually set a policy for that origin.
  virtual void OnHeader(const url::Origin& origin, const std::string& value);

  // Considers queueing a network error report for the request described in
  // |details|. Note that Network Error Logging can report a fraction of
  // successful requests as well (to calculate error rates), so this should be
  // called on *all* requests.
  virtual void OnRequest(const RequestDetails& details);

  // Removes browsing data (origin policies) associated with any origin for
  // which |origin_filter| returns true, or for all origins if
  // |origin_filter.is_null()|.
  virtual void RemoveBrowsingData(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter);

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

 private:
  // NEL Policy set by an origin.
  struct OriginPolicy {
    // Reporting API endpoint group to which reports should be sent.
    std::string report_to;

    base::TimeTicks expires;

    double success_fraction;
    double failure_fraction;
    bool include_subdomains;
  };

  // Map from origin to origin's (owned) policy.
  // Would be unordered_map, but url::Origin has no hash.
  using PolicyMap = std::map<url::Origin, OriginPolicy>;

  // Wildcard policies are policies for which the includeSubdomains flag is set.
  //
  // Wildcard policies are accessed by domain name, not full origin, so there
  // can be multiple wildcard policies per domain name.
  //
  // This is a map from domain name to the set of pointers to wildcard policies
  // in that domain.
  //
  // Policies in the map are unowned; they are pointers to the original in the
  // PolicyMap.
  using WildcardPolicyMap =
      std::map<std::string, std::set<const OriginPolicy*>>;

  // Would be const, but base::TickClock::NowTicks isn't.
  bool ParseHeader(const std::string& json_value, OriginPolicy* policy_out);

  const OriginPolicy* FindPolicyForOrigin(const url::Origin& origin) const;
  const OriginPolicy* FindWildcardPolicyForDomain(
      const std::string& domain) const;
  void MaybeAddWildcardPolicy(const url::Origin& origin,
                              const OriginPolicy* policy);
  void MaybeRemoveWildcardPolicy(const url::Origin& origin,
                                 const OriginPolicy* policy);
  std::unique_ptr<const base::Value> CreateReportBody(
      const std::string& type,
      double sampling_fraction,
      const RequestDetails& details) const;

  base::TickClock* tick_clock_;

  // Unowned.
  ReportingService* reporting_service_;

  PolicyMap policies_;
  WildcardPolicyMap wildcard_policies_;

  DISALLOW_COPY_AND_ASSIGN(NetworkErrorLoggingService);
};

}  // namespace net

#endif  // NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_SERVICE_H_
