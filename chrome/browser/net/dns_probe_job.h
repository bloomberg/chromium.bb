// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DNS_PROBE_JOB_H_
#define CHROME_BROWSER_NET_DNS_PROBE_JOB_H_

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"

namespace net {
class DnsClient;
class NetLog;
}

namespace chrome_browser_net {

// A job to probe the health of a DNS configuration.
class DnsProbeJob {
 public:
  enum Result {
    SERVERS_UNKNOWN,
    SERVERS_CORRECT,  // Server responds with correct answers.
    SERVERS_INCORRECT,  // Server responds with success but incorrect answer.
    // TODO(ttuttle): Do we want an "unreliable" result, for e.g. servers that
    // lie about NXDOMAIN?
    SERVERS_FAILING,  // Server responds with errors.
    SERVERS_UNREACHABLE,  // Server doesn't respond (or never got our packets)
    MAX_RESULT
  };
  typedef base::Callback<void(DnsProbeJob* job, Result result)> CallbackType;

  virtual ~DnsProbeJob() { }

  // Creates and starts a probe job.
  //
  // |dns_client| should be a DnsClient with the DnsConfig already set.
  // |callback| will be called when the probe finishes, which may happen
  // before the constructor returns (for example, if we can't create the DNS
  // transactions).
  static scoped_ptr<DnsProbeJob> CreateJob(
      scoped_ptr<net::DnsClient> dns_client,
      const CallbackType& callback,
      net::NetLog* net_log);

 protected:
  DnsProbeJob() { }

  DISALLOW_COPY_AND_ASSIGN(DnsProbeJob);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_PROBE_JOB_H_
