// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_QUERIER_IMPL_H_
#define DISCOVERY_DNSSD_IMPL_QUERIER_IMPL_H_

#include <map>
#include <unordered_map>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/impl/dns_data.h"
#include "discovery/dnssd/impl/instance_key.h"
#include "discovery/dnssd/impl/service_key.h"
#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "discovery/dnssd/public/dns_sd_querier.h"
#include "discovery/mdns/mdns_record_changed_callback.h"
#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/public/mdns_service.h"

namespace openscreen {
namespace discovery {

class QuerierImpl : public DnsSdQuerier, public MdnsRecordChangedCallback {
 public:
  // |querier| must outlive the QuerierImpl instance constructed.
  explicit QuerierImpl(MdnsService* querier);
  ~QuerierImpl() override;

  bool IsQueryRunning(absl::string_view service) const;

  // DnsSdQuerier overrides.
  void StartQuery(absl::string_view service, Callback* callback) override;
  void StopQuery(absl::string_view service, Callback* callback) override;

  // MdnsRecordChangedCallback overrides.
  // TODO(rwkeane): Ensure this is run on the TaskRunner thread once the
  // underlying mDNS implementation can be overridden.
  void OnRecordChanged(const MdnsRecord& record,
                       RecordChangedEvent event) override;

 private:
  // Process an OnRecordChanged event for a PTR record.
  Error HandlePtrRecordChange(const MdnsRecord& record,
                              RecordChangedEvent event);

  // Process an OnRecordChanged event for non-PTR records (SRV, TXT, A, and AAAA
  // records).
  Error HandleNonPtrRecordChange(const MdnsRecord& record,
                                 RecordChangedEvent event);

  inline bool IsQueryRunning(const ServiceKey& key) const {
    return callback_map_.find(key) != callback_map_.end();
  }

  // Initiates or terminates queries on the mdns_querier_ object.
  void StartDnsQuery(const DnsQueryInfo& query);
  void StopDnsQuery(const DnsQueryInfo& query);

  // Erases all instance records describing services matching the provided key
  // and informs all callbacks associated with the given key of their deletion.
  void EraseInstancesOf(const ServiceKey& service);

  // Calls the appropriate callback method based on the provided Instance Record
  // values.
  void NotifyCallbacks(const std::vector<Callback*>& callbacks,
                       const ErrorOr<DnsSdInstanceRecord>& old_record,
                       const ErrorOr<DnsSdInstanceRecord>& new_record);

  // Returns all InstanceKeys received so far which represent instances of
  // the service described by the provided ServiceKey.
  std::vector<InstanceKey> GetMatchingInstances(const ServiceKey& key);

  // Map from a specific service instance to the data received so far about
  // that instance.
  std::unordered_map<InstanceKey, DnsData, absl::Hash<InstanceKey>>
      received_records_;

  // Map from the (service, domain) pairs currently being queried for to the
  // callbacks to call when new InstanceRecords are available.
  std::map<ServiceKey, std::vector<Callback*>> callback_map_;

  MdnsService* const mdns_querier_;

  friend class QuerierImplTesting;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_QUERIER_IMPL_H_
