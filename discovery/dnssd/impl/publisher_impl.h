// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_PUBLISHER_IMPL_H_
#define DISCOVERY_DNSSD_IMPL_PUBLISHER_IMPL_H_

#include "absl/strings/string_view.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "discovery/dnssd/public/dns_sd_publisher.h"
#include "discovery/mdns/mdns_domain_confirmed_provider.h"
#include "discovery/mdns/public/mdns_service.h"

namespace openscreen {
namespace discovery {

class PublisherImpl : public DnsSdPublisher,
                      public MdnsDomainConfirmedProvider {
 public:
  PublisherImpl(MdnsService* publisher);
  ~PublisherImpl() override;

  // DnsSdPublisher overrides.
  Error Register(const DnsSdInstanceRecord& record) override;
  Error UpdateRegistration(const DnsSdInstanceRecord& record) override;
  int DeregisterAll(const std::string& service) override;

 private:
  Error UpdatePublishedRegistration(const DnsSdInstanceRecord& record);

  // MdnsDomainConfirmedProvider overrides.
  void OnDomainFound(const DomainName& requested_name,
                     const DomainName& confirmed_name) override;

  // The set of records which will be published once the mDNS Probe phase
  // completes.
  std::vector<DnsSdInstanceRecord> pending_records_;

  // Maps from the requested record to the record which was published after
  // the mDNS Probe phase was completed. The only difference between these
  // records should be the instance name.
  std::map<DnsSdInstanceRecord, DnsSdInstanceRecord> published_records_;

  MdnsService* const mdns_publisher_;

  friend class PublisherTesting;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_PUBLISHER_IMPL_H_
