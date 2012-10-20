// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DNS_PROBE_JOB_H_
#define CHROME_BROWSER_NET_DNS_PROBE_JOB_H_

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_log.h"
#include "net/dns/dns_transaction.h"

namespace net {
class DnsClient;
struct DnsConfig;
};

namespace chrome_browser_net {

// A job to probe the health of a DNS configuration.
class DnsProbeJob {
 public:
  enum Result {
    DNS_UNKNOWN,
    DNS_WORKING,  // Server responds with correct answers.
    // TODO(ttuttle): Do we want an "unreliable" result, for e.g. servers that
    // lie about NXDOMAIN?
    DNS_BROKEN,  // Server responds with errors.
    DNS_UNREACHABLE,  // Server doesn't respond (or never got our packets)
  };
  typedef base::Callback<void(DnsProbeJob* job, Result result)> CallbackType;

  // Creates and starts a probe job.
  //
  // |dns_client| should be a DnsClient with the DnsConfig alreay set.
  // |callback| will be called when the probe finishes, which may happen
  // before the constructor returns (for example, if we can't create the DNS
  // transactions).
  DnsProbeJob(scoped_ptr<net::DnsClient> dns_client,
              const CallbackType& callback,
              net::NetLog* net_log);
  ~DnsProbeJob();

 private:
  enum QueryStatus {
    QUERY_UNKNOWN,
    QUERY_CORRECT,
    QUERY_INCORRECT,
    QUERY_ERROR,
    QUERY_RUNNING,
  };

  void MaybeFinishProbe();
  scoped_ptr<net::DnsTransaction> CreateTransaction(
      const std::string& hostname);
  void StartTransaction(net::DnsTransaction* transaction);
  void OnTransactionComplete(net::DnsTransaction* transaction,
                             int net_error,
                             const net::DnsResponse* response);
  void RunCallback(Result result);

  net::BoundNetLog bound_net_log_;
  scoped_ptr<net::DnsClient> dns_client_;
  const CallbackType callback_;
  bool probe_running_;
  scoped_ptr<net::DnsTransaction> good_transaction_;
  scoped_ptr<net::DnsTransaction> bad_transaction_;
  QueryStatus good_status_;
  QueryStatus bad_status_;

  DISALLOW_COPY_AND_ASSIGN(DnsProbeJob);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_PROBE_JOB_H_
