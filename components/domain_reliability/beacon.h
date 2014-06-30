// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_BEACON_H_
#define COMPONENTS_DOMAIN_RELIABILITY_BEACON_H_

#include <string>

#include "base/time/time.h"
#include "components/domain_reliability/domain_reliability_export.h"

namespace base {
class Value;
}  // namespace base

namespace domain_reliability {

// The per-request data that is uploaded to the Domain Reliability collector.
struct DOMAIN_RELIABILITY_EXPORT DomainReliabilityBeacon {
 public:
  DomainReliabilityBeacon();
  ~DomainReliabilityBeacon();

  // Converts the Beacon to JSON format for uploading. Calculates the age
  // relative to an upload time of |upload_time|.
  base::Value* ToValue(base::TimeTicks upload_time) const;

  // Status string (e.g. "ok", "dns.nxdomain", "http.403").
  std::string status;
  // Net error code.  Encoded as a string in the final JSON.
  int chrome_error;
  // IP address of the server the request went to.
  std::string server_ip;
  // Protocol used to make the request.
  std::string protocol;
  // HTTP response code returned by the server, or -1 if none was received.
  int http_response_code;
  // Elapsed time between starting and completing the request.
  base::TimeDelta elapsed;
  // Start time of the request.  Encoded as the request age in the final JSON.
  base::TimeTicks start_time;

  // Okay to copy and assign.
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_BEACON_H_
