// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_PROBE_H_
#define DISCOVERY_MDNS_MDNS_PROBE_H_

#include <vector>

#include "discovery/mdns/mdns_records.h"
#include "platform/base/ip_address.h"

namespace openscreen {

class TaskRunner;

namespace discovery {

class MdnsQuerier;
class MdnsRandom;
class MdnsSender;

// Implements the probing method as described in RFC 6762 section 8.1 to claim a
// provided domain name. In place of the MdnsRecord(s) that will be published, a
// 'fake' mDNS record of type A or AAAA will be generated from provided endpoint
// variable with TTL 2 seconds. 0 or 1 seconds are not used because these
// constants are used as part of goodbye records, so poorly written receivers
// may handle these cases in unexpected ways. Caching of probe queries is not
// supported for mDNS probes (else, in a probe which failed, invalid records
// would be cached). If for some reason this did occur, though, it should be a
// non-issue because the probe record will expire after 2 seconds.
//
// During probe query conflict resolution, these fake records will be compared
// with the records provided by another mDNS endpoint. As 2 different mDNS
// endpoints of the same service type cannot have the same endpoint, these
// fake mDNS records should never match the real or fake records provided by
// the other mDNS endpoint, so lexicographic comparison as described in RFC
// 6762 section 8.2.1 can proceed as described.
class MdnsProbe {
 public:
  // The observer class is responsible for returning the result of an ongoing
  // probe query to the caller.
  class Observer {
   public:
    virtual ~Observer();

    // Called once the probing phase has been completed successfully.
    virtual void OnProbeSuccess(MdnsProbe* probe) = 0;

    // Called once the probing phase fails.
    virtual void OnProbeFailure(MdnsProbe* probe) = 0;
  };

  // |sender|, |querier|, |random_delay|, |task_runner|, and |observer| must all
  // persist for the duration of this object's lifetime.
  MdnsProbe(MdnsSender* sender,
            MdnsQuerier* querier,
            MdnsRandom* random_delay,
            TaskRunner* task_runner,
            Observer* observer,
            DomainName target_name,
            IPEndpoint endpoint);
  MdnsProbe(const MdnsProbe& other) = delete;
  MdnsProbe(MdnsProbe&& other) = delete;
  ~MdnsProbe();

  MdnsProbe& operator=(const MdnsProbe& other) = delete;
  MdnsProbe& operator=(MdnsProbe&& other) = delete;

  const DomainName& target_name() const { return target_name_; }

  const IPEndpoint& endpoint() const { return endpoint_; }

 private:
  // Performs the probe query as described in the class-level comment.
  void ProbeOnce();

  MdnsSender* const sender_;
  MdnsQuerier* const querier_;
  MdnsRandom* const random_delay_;
  TaskRunner* const task_runner_;
  Observer* const observer_;
  DomainName target_name_;
  IPEndpoint endpoint_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_PROBE_H_
