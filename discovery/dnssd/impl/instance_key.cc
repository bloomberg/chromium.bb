// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/instance_key.h"

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/impl/service_key.h"
#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/public/mdns_constants.h"

namespace openscreen {
namespace discovery {

InstanceKey::InstanceKey(const MdnsRecord& record)
    : InstanceKey(GetDomainName(record)) {}

InstanceKey::InstanceKey(const DomainName& domain)
    : ServiceKey(domain), instance_id_(domain.labels()[0]) {
  OSP_DCHECK(IsInstanceValid(instance_id_));
}

InstanceKey::InstanceKey(const DnsSdInstanceRecord& record)
    : InstanceKey(record.instance_id(),
                  record.service_id(),
                  record.domain_id()) {}

InstanceKey::InstanceKey(absl::string_view instance,
                         absl::string_view service,
                         absl::string_view domain)
    : ServiceKey(service, domain), instance_id_(instance) {
  OSP_DCHECK(IsInstanceValid(instance_id_));
}

InstanceKey::InstanceKey(const InstanceKey& other) = default;
InstanceKey::InstanceKey(InstanceKey&& other) = default;

InstanceKey& InstanceKey::operator=(const InstanceKey& rhs) = default;
InstanceKey& InstanceKey::operator=(InstanceKey&& rhs) = default;

}  // namespace discovery
}  // namespace openscreen
