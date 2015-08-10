// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/error_page/common/net_error_info.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

namespace error_page {

const char kDnsProbeErrorDomain[] = "dnsprobe";

const char* DnsProbeStatusToString(int status) {
  switch (status) {
    case chrome_common_net::DNS_PROBE_POSSIBLE:
      return "DNS_PROBE_POSSIBLE";
    case chrome_common_net::DNS_PROBE_NOT_RUN:
      return "DNS_PROBE_NOT_RUN";
    case chrome_common_net::DNS_PROBE_STARTED:
      return "DNS_PROBE_STARTED";
    case chrome_common_net::DNS_PROBE_FINISHED_INCONCLUSIVE:
      return "DNS_PROBE_FINISHED_INCONCLUSIVE";
    case chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET:
      return "DNS_PROBE_FINISHED_NO_INTERNET";
    case chrome_common_net::DNS_PROBE_FINISHED_BAD_CONFIG:
      return "DNS_PROBE_FINISHED_BAD_CONFIG";
    case chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN:
      return "DNS_PROBE_FINISHED_NXDOMAIN";
    default:
      NOTREACHED();
      return "";
  }
}

bool DnsProbeStatusIsFinished(chrome_common_net::DnsProbeStatus status) {
  return status >= chrome_common_net::DNS_PROBE_FINISHED_INCONCLUSIVE &&
         status < chrome_common_net::DNS_PROBE_MAX;
}

void RecordEvent(chrome_common_net::NetworkErrorPageEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Net.ErrorPageCounts", event,
                            chrome_common_net::NETWORK_ERROR_PAGE_EVENT_MAX);
}

}  // namespace error_page
