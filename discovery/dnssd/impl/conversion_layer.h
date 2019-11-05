// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_CONVERSION_LAYER_H_
#define DISCOVERY_DNSSD_IMPL_CONVERSION_LAYER_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/public/instance_record.h"
#include "discovery/dnssd/public/txt_record.h"
#include "discovery/mdns/mdns_records.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {

// *** Key Generation functions ***

// Helper functions to get the Key associated with a given DNS Record.
ErrorOr<InstanceKey> GetInstanceKey(const MdnsRecord& record);
ErrorOr<ServiceKey> GetServiceKey(const MdnsRecord& record);

// Creates the ServiceKey associated with the provided service and dotted fully
// qualified domain names. NOTE: The provided parameters must be valid service
// and domain ids.
ServiceKey GetServiceKey(absl::string_view service, absl::string_view domain);

// Gets the ServiceKey of the service with which this InstanceKey is associated.
ServiceKey GetServiceKey(const InstanceKey& key);

// *** Conversions from DNS entities to DNS-SD Entities ***

// Attempts to create a new TXT record from the provided set of strings,
// returning a TxtRecord on success or an error if the provided strings are
// not valid.
ErrorOr<DnsSdTxtRecord> CreateFromDnsTxt(const TxtRecordRdata& txt);

bool IsPtrRecord(const MdnsRecord& record);

//*** Conversions to DNS entities from DNS-SD Entities ***

// Returns the query required to get all instance information about the service
// instances described by the provided ServiceKey.
DnsQueryInfo GetInstanceQueryInfo(const InstanceKey& key);

// Returns the query required to get all service information that matches the
// provided key.
DnsQueryInfo GetPtrQueryInfo(const ServiceKey& key);

// Returns all MdnsRecord entites generated from this InstanceRecord.
// TODO(rwkeane): Implement this method.
std::vector<MdnsRecord> GetDnsRecords(const DnsSdInstanceRecord& record);

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_CONVERSION_LAYER_H_
