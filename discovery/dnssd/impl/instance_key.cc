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

InstanceKey::InstanceKey(const MdnsRecord& record) {
  ErrorOr<InstanceKey> key = TryCreate(record);
  OSP_DCHECK(key.is_value());
  *this = std::move(key.value());
}

InstanceKey::InstanceKey(const DomainName& domain) {
  ErrorOr<InstanceKey> key = TryCreate(domain);
  OSP_DCHECK(key.is_value());
  *this = std::move(key.value());
}

InstanceKey::InstanceKey(const DnsSdInstanceRecord& record)
    : InstanceKey(record.instance_id(),
                  record.service_id(),
                  record.domain_id()) {}

InstanceKey::InstanceKey(absl::string_view instance,
                         absl::string_view service,
                         absl::string_view domain)
    : instance_id_(instance), service_id_(service), domain_id_(domain) {
  OSP_DCHECK(IsInstanceValid(instance_id_));
  OSP_DCHECK(IsServiceValid(service_id_));
  OSP_DCHECK(IsDomainValid(domain_id_));
}

InstanceKey::InstanceKey(const InstanceKey& other) = default;
InstanceKey::InstanceKey(InstanceKey&& other) = default;

InstanceKey& InstanceKey::operator=(const InstanceKey& rhs) = default;
InstanceKey& InstanceKey::operator=(InstanceKey&& rhs) = default;

bool InstanceKey::IsInstanceOf(const ServiceKey& service_key) const {
  return service_id_ == service_key.service_id() &&
         domain_id_ == service_key.domain_id();
}

// static
ErrorOr<InstanceKey> InstanceKey::TryCreate(const MdnsRecord& record) {
  const DomainName& names =
      IsPtrRecord(record)
          ? absl::get<PtrRecordRdata>(record.rdata()).ptr_domain()
          : record.name();

  return TryCreate(names);
}

// static
ErrorOr<InstanceKey> InstanceKey::TryCreate(const DomainName& names) {
  if (names.labels().size() < 4) {
    return Error::Code::kParameterInvalid;
  }

  auto it = names.labels().begin();
  std::string instance_id = *it++;
  if (!IsInstanceValid(instance_id)) {
    return Error::Code::kParameterInvalid;
  }

  std::string service_name = *it++;
  std::string protocol = *it++;
  std::string service_id = service_name.append(".").append(protocol);
  if (!IsServiceValid(service_id)) {
    return Error::Code::kParameterInvalid;
  }

  std::string domain_id = absl::StrJoin(it, names.labels().end(), ".");
  if (!IsDomainValid(domain_id)) {
    return Error::Code::kParameterInvalid;
  }

  return InstanceKey(instance_id, service_id, domain_id);
}

}  // namespace discovery
}  // namespace openscreen
