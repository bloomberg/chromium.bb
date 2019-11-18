// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_DNS_DATA_H_
#define DISCOVERY_DNSSD_IMPL_DNS_DATA_H_

#include "absl/types/optional.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/impl/instance_key.h"
#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "discovery/mdns/mdns_record_changed_callback.h"
#include "discovery/mdns/mdns_records.h"

namespace openscreen {
namespace discovery {

// This is the set of DNS data that can be associated with a single PTR record.
class DnsData {
 public:
  explicit DnsData(const InstanceKey& instance_id);

  // Converts this DnsData to an InstanceRecord if enough data has been
  // populated to create a valid InstanceRecord. Specifically, this means that
  // the srv, txt, and either a or aaaa fields have been populated. In all other
  // cases, returns an error.
  ErrorOr<DnsSdInstanceRecord> CreateRecord();

  // Modifies this entity with the provided DnsRecord. If called with a valid
  // record type, the provided change will always be applied. The returned
  // result will be an error if the change does not make sense from our current
  // data state, and Error::None() otherwise. Valid record types with which this
  // method can be called are SRV, TXT, A, and AAAA record types.
  Error ApplyDataRecordChange(const MdnsRecord& record,
                              RecordChangedEvent event);

 private:
  absl::optional<SrvRecordRdata> srv_;
  absl::optional<TxtRecordRdata> txt_;
  absl::optional<ARecordRdata> a_;
  absl::optional<AAAARecordRdata> aaaa_;

  InstanceKey instance_id_;

  // Used in dns_data_unittest.cc.
  friend class DnsDataTesting;

  // Used in querier_impl_unittest.cc.
  friend class DnsDataAccessor;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_DNS_DATA_H_
