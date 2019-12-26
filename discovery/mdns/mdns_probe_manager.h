// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_PROBE_MANAGER_H_
#define DISCOVERY_MDNS_MDNS_PROBE_MANAGER_H_

#include <memory>
#include <vector>

#include "discovery/mdns/mdns_probe.h"
#include "discovery/mdns/mdns_records.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"

namespace openscreen {

class TaskRunner;

namespace discovery {

class MdnsQuerier;
class MdnsRandom;
class MdnsSender;

// Interface for maintaining ownership of mDNS Domains.
class MdnsProbeManager {
 public:
  virtual ~MdnsProbeManager();

  // Returns whether the provided domain name has been claimed as owned by this
  // mDNS Probe Manager.
  virtual bool IsDomainClaimed(const DomainName& domain) const = 0;

  // |message| is a considered a probe query for a specific domain if it
  // contains a query for a specific domain which is answered by mDNS Records in
  // the 'authority records' section of |message|. If a probe for the provided
  // domain name is ongoing, an MdnsMessage is sent to the provided endpoint as
  // described in RFC 6762 section 8.2 to allow for conflict resolution. If the
  // requested name has already been claimed, a message to specify this will be
  // sent as described in RFC 6762 section 8.1. The |src| argument is the
  // address from which the message was originally sent, so that the response
  // message may be sent as a unicast response.
  virtual void RespondToProbeQuery(const MdnsMessage& message,
                                   const IPEndpoint& src) = 0;
};

// This class is responsible for managing all ongoing probes for claiming domain
// names, as described in RFC 6762 Section 8.1's probing phase. If one such
// probe fails due to a conflict detection, this class will modify the domain
// name as described in RFC 6762 section 9 and re-initiate probing for the new
// name.
class MdnsProbeManagerImpl : public MdnsProbe::Observer,
                             public MdnsProbeManager {
 public:
  class Callback {
   public:
    virtual ~Callback();

    // Called once the probing phase has been completed, and a DomainName has
    // been confirmed. The callee is expected to register records for the
    // newly confirmed name in this callback. Note that the requested name and
    // the confirmed name may differ if conflict resolution has occurred.
    virtual void OnDomainFound(const DomainName& requested_name,
                               const DomainName& confirmed_name) = 0;
  };

  // |sender|, |querier|, |random_delay|, and |task_runner|, must all persist
  // for the duration of this object's lifetime.
  MdnsProbeManagerImpl(MdnsSender* sender,
                       MdnsQuerier* querier,
                       MdnsRandom* random_delay,
                       TaskRunner* task_runner);
  MdnsProbeManagerImpl(const MdnsProbeManager& other) = delete;
  MdnsProbeManagerImpl(MdnsProbeManager&& other) = delete;
  ~MdnsProbeManagerImpl() override;

  MdnsProbeManagerImpl& operator=(const MdnsProbeManagerImpl& other) = delete;
  MdnsProbeManagerImpl& operator=(MdnsProbeManagerImpl&& other) = delete;

  // Starts probing for a valid domain name based on the given one. This may
  // only be called once per MdnsProbe instance. |observer| must persist until
  // a valid domain is discovered and the observer's OnDomainFound method is
  // called.
  // NOTE: |endpoint| is used to generate a 'fake' address record to use for
  // the probe query. See MdnsProbe::PerformProbeIteration() for further
  // details.
  Error StartProbe(Callback* callback,
                   DomainName requested_name,
                   IPEndpoint endpoint);

  // Stops probing for the requested domain name.
  Error StopProbe(const DomainName& requested_name);

  // MdnsDomainOwnershipManager overrides.
  bool IsDomainClaimed(const DomainName& domain) const override;
  void RespondToProbeQuery(const MdnsMessage& message,
                           const IPEndpoint& src) override;

 private:
  std::unique_ptr<MdnsProbe> CreateProbe(DomainName name, IPEndpoint endpoint) {
    return std::make_unique<MdnsProbe>(sender_, querier_, random_delay_,
                                       task_runner_, this, std::move(name),
                                       std::move(endpoint));
  }

  struct OngoingProbe {
    OngoingProbe(std::unique_ptr<MdnsProbe> probe,
                 DomainName name,
                 Callback* callback);

    // NOTE: unique_ptr objects are used to avoid issues when the container
    // holding this object is resized.
    std::unique_ptr<MdnsProbe> probe;
    DomainName requested_name;
    Callback* callback;
  };

  // MdnsProbe::Observer overrides.
  void OnProbeSuccess(MdnsProbe* probe) override;
  void OnProbeFailure(MdnsProbe* probe) override;

  MdnsSender* const sender_;
  MdnsQuerier* const querier_;
  MdnsRandom* const random_delay_;
  TaskRunner* const task_runner_;

  // The set of all probes which have completed successfully. This set is
  // expected to remain small. unique_ptrs are used for storing the probes to
  // avoid issues when the vector is resized.
  std::vector<std::unique_ptr<MdnsProbe>> completed_probes_;

  // The set of all currently ongoing probes. This set is expected to remain
  // small.
  std::vector<OngoingProbe> ongoing_probes_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_PROBE_MANAGER_H_
