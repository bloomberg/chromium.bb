// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_CONSTANTS_H_
#define DISCOVERY_DNSSD_IMPL_CONSTANTS_H_

#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "cast/common/mdns/mdns_records.h"

namespace openscreen {
namespace discovery {

// This class is intended to be used as the key of a std::unordered_map when
// referencing data related to an SRV record.
struct InstanceKey {
  std::string service_id;
  std::string domain_id;
  std::string instance_id;
};

// This class is intended to be used as the key of a std::unordered_map when
// referencing data related to a PTR record.
struct ServiceKey {
  std::string service_id;
  std::string domain_id;
};

// This is the set of DNS data that can be associated with a single PTR record.
struct DnsData {
  absl::optional<cast::mdns::SrvRecordRdata> srv;
  absl::optional<cast::mdns::TxtRecordRdata> txt;
  absl::optional<cast::mdns::ARecordRdata> a;
  absl::optional<cast::mdns::AAAARecordRdata> aaaa;
};

// Hashing functions to allow for using with absl::Hash<...>.
template <typename H>
H AbslHashValue(H h, const ServiceKey& key) {
  return H::combine(std::move(h), key.service_id, key.domain_id);
}

template <typename H>
H AbslHashValue(H h, const InstanceKey& key) {
  return H::combine(std::move(h), key.service_id, key.domain_id,
                    key.instance_id);
}

// Comparison operators for using above keys with an std::map
inline bool operator<(const ServiceKey& lhs, const ServiceKey& rhs) {
  int comp = lhs.domain_id.compare(rhs.domain_id);
  if (comp != 0) {
    return comp < 0;
  }
  return lhs.service_id < rhs.service_id;
}

inline bool operator>(const ServiceKey& lhs, const ServiceKey& rhs) {
  return rhs < lhs;
}

inline bool operator<=(const ServiceKey& lhs, const ServiceKey& rhs) {
  return !(rhs > lhs);
}

inline bool operator>=(const ServiceKey& lhs, const ServiceKey& rhs) {
  return !(rhs < lhs);
}

inline bool operator==(const ServiceKey& lhs, const ServiceKey& rhs) {
  return lhs <= rhs && lhs >= rhs;
}

inline bool operator!=(const ServiceKey& lhs, const ServiceKey& rhs) {
  return !(lhs == rhs);
}

inline bool operator<(const InstanceKey& lhs, const InstanceKey& rhs) {
  int comp = lhs.domain_id.compare(rhs.domain_id);
  if (comp != 0) {
    return comp < 0;
  }

  comp = lhs.service_id.compare(rhs.service_id);
  if (comp != 0) {
    return comp < 0;
  }

  return lhs.instance_id < rhs.instance_id;
}

inline bool operator>(const InstanceKey& lhs, const InstanceKey& rhs) {
  return rhs < lhs;
}

inline bool operator<=(const InstanceKey& lhs, const InstanceKey& rhs) {
  return !(rhs > lhs);
}

inline bool operator>=(const InstanceKey& lhs, const InstanceKey& rhs) {
  return !(rhs < lhs);
}

inline bool operator==(const InstanceKey& lhs, const InstanceKey& rhs) {
  return lhs <= rhs && lhs >= rhs;
}

inline bool operator!=(const InstanceKey& lhs, const InstanceKey& rhs) {
  return !(lhs == rhs);
}

// Comparison between ServiceKey and InstanceKey types.
inline bool IsInstanceOf(const ServiceKey& ptr, const InstanceKey& srv) {
  return ptr.service_id == srv.service_id && ptr.domain_id == srv.domain_id;
}

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_CONSTANTS_H_
