// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/util.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_errors.h"

namespace domain_reliability {

namespace {

class ActualTimer : public MockableTime::Timer {
 public:
  // Initialize base timer with retain_user_info and is_repeating false.
  ActualTimer() : base_timer_(false, false) {}

  virtual ~ActualTimer() {}

  // MockableTime::Timer implementation:
  virtual void Start(const tracked_objects::Location& posted_from,
                     base::TimeDelta delay,
                     const base::Closure& user_task) OVERRIDE {
    base_timer_.Start(posted_from, delay, user_task);
  }

  virtual void Stop() OVERRIDE {
    base_timer_.Stop();
  }

  virtual bool IsRunning() OVERRIDE {
    return base_timer_.IsRunning();
  }

 private:
  base::Timer base_timer_;
};

const struct NetErrorMapping {
  int net_error;
  const char* beacon_status;
} net_error_map[] = {
  { net::OK, "ok" },
  { net::ERR_TIMED_OUT, "tcp.connection.timed_out" },
  { net::ERR_CONNECTION_CLOSED, "tcp.connection.closed" },
  { net::ERR_CONNECTION_RESET, "tcp.connection.reset" },
  { net::ERR_CONNECTION_REFUSED, "tcp.connection.refused" },
  { net::ERR_CONNECTION_ABORTED, "tcp.connection.aborted" },
  { net::ERR_CONNECTION_FAILED, "tcp.connection.failed" },
  { net::ERR_NAME_NOT_RESOLVED, "dns" },
  { net::ERR_SSL_PROTOCOL_ERROR, "ssl.protocol.error" },
  { net::ERR_ADDRESS_INVALID, "tcp.connection.address_invalid" },
  { net::ERR_ADDRESS_UNREACHABLE, "tcp.connection.address_unreachable" },
  { net::ERR_CONNECTION_TIMED_OUT, "tcp.connection.timed_out" },
  { net::ERR_NAME_RESOLUTION_FAILED, "dns" },
  { net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN,
        "ssl.pinned_key_not_in_cert_chain" },
  { net::ERR_CERT_COMMON_NAME_INVALID, "ssl.cert.name_invalid" },
  { net::ERR_CERT_DATE_INVALID, "ssl.cert.date_invalid" },
  { net::ERR_CERT_AUTHORITY_INVALID, "ssl.cert.authority_invalid" },
  { net::ERR_CERT_REVOKED, "ssl.cert.revoked" },
  { net::ERR_CERT_INVALID, "ssl.cert.invalid" },
  { net::ERR_EMPTY_RESPONSE, "http.empty_response" },
  { net::ERR_SPDY_PING_FAILED, "spdy.ping_failed" },
  { net::ERR_SPDY_PROTOCOL_ERROR, "spdy.protocol" },
  { net::ERR_QUIC_PROTOCOL_ERROR, "quic.protocol" },
  { net::ERR_DNS_MALFORMED_RESPONSE, "dns.protocol" },
  { net::ERR_DNS_SERVER_FAILED, "dns.server" },
  { net::ERR_DNS_TIMED_OUT, "dns.timed_out" },
};

}  // namespace

// static
bool GetDomainReliabilityBeaconStatus(
    int net_error,
    int http_response_code,
    std::string* beacon_status_out) {
  if (net_error == net::OK) {
    if (http_response_code >= 400 && http_response_code < 600)
      *beacon_status_out = "http.error";
    else
      *beacon_status_out = "ok";
    return true;
  }

  // TODO(ttuttle): Consider sorting and using binary search?
  for (size_t i = 0; i < arraysize(net_error_map); i++) {
    if (net_error_map[i].net_error == net_error) {
      *beacon_status_out = net_error_map[i].beacon_status;
      return true;
    }
  }
  return false;
}

// TODO(ttuttle): Consider using NPN/ALPN instead, if there's a good way to
//                differentiate HTTP and HTTPS.
std::string GetDomainReliabilityProtocol(
    net::HttpResponseInfo::ConnectionInfo connection_info,
    bool ssl_info_populated) {
  switch (connection_info) {
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
      return "";
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1:
      return ssl_info_populated ? "HTTPS" : "HTTP";
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_SPDY4:
      return "SPDY";
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC1_SPDY3:
      return "QUIC";
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      NOTREACHED();
      return "";
  }
  NOTREACHED();
  return "";
}

MockableTime::Timer::~Timer() {}
MockableTime::Timer::Timer() {}

MockableTime::~MockableTime() {}
MockableTime::MockableTime() {}

ActualTime::ActualTime() {}
ActualTime::~ActualTime() {}

base::Time ActualTime::Now() { return base::Time::Now(); }
base::TimeTicks ActualTime::NowTicks() { return base::TimeTicks::Now(); }

scoped_ptr<MockableTime::Timer> ActualTime::CreateTimer() {
  return scoped_ptr<MockableTime::Timer>(new ActualTimer());
}

}  // namespace domain_reliability
