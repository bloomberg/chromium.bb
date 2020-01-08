// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_DNS_SD_INSTANCE_RECORD_H_
#define DISCOVERY_DNSSD_PUBLIC_DNS_SD_INSTANCE_RECORD_H_

#include "discovery/dnssd/public/dns_sd_txt_record.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace discovery {

bool IsInstanceValid(const std::string& instance);
bool IsServiceValid(const std::string& service);
bool IsDomainValid(const std::string& domain);

// Represents the data stored in DNS records of types SRV, TXT, A, and AAAA
class DnsSdInstanceRecord {
 public:
  // These ctors expect valid input, and will cause a crash if they are not.
  DnsSdInstanceRecord(std::string instance_id,
                      std::string service_id,
                      std::string domain_id,
                      IPEndpoint endpoint,
                      DnsSdTxtRecord txt);

  // NOTE: This constructor expects one endpoint to be an IPv4 address and the
  // other to be an IPv6 address.
  DnsSdInstanceRecord(std::string instance_id,
                      std::string service_id,
                      std::string domain_id,
                      IPEndpoint ipv4_endpoint,
                      IPEndpoint ipv6_endpoint,
                      DnsSdTxtRecord txt);

  // Returns the instance name for this DNS-SD record.
  const std::string& instance_id() const { return instance_id_; }

  // Returns the service id for this DNS-SD record.
  const std::string& service_id() const { return service_id_; }

  // Returns the domain id for this DNS-SD record.
  const std::string& domain_id() const { return domain_id_; }

  // Returns the address associated with this DNS-SD record. In any valid
  // record, at least one will be set.
  const IPEndpoint& address_v4() const { return address_v4_; }
  const IPEndpoint& address_v6() const { return address_v6_; }

  // Returns the TXT record associated with this DNS-SD record
  const DnsSdTxtRecord& txt() const { return txt_; }

  // Returns the port associated with this instance record.
  uint16_t port() const;

 private:
  DnsSdInstanceRecord(std::string instance_id,
                      std::string service_id,
                      std::string domain_id,
                      DnsSdTxtRecord txt);

  std::string instance_id_;
  std::string service_id_;
  std::string domain_id_;
  IPEndpoint address_v4_;
  IPEndpoint address_v6_;
  DnsSdTxtRecord txt_;

  friend bool operator<(const DnsSdInstanceRecord& lhs,
                        const DnsSdInstanceRecord& rhs);
};

bool operator<(const DnsSdInstanceRecord& lhs, const DnsSdInstanceRecord& rhs);

inline bool operator>(const DnsSdInstanceRecord& lhs,
                      const DnsSdInstanceRecord& rhs) {
  return rhs < lhs;
}

inline bool operator<=(const DnsSdInstanceRecord& lhs,
                       const DnsSdInstanceRecord& rhs) {
  return !(rhs > lhs);
}

inline bool operator>=(const DnsSdInstanceRecord& lhs,
                       const DnsSdInstanceRecord& rhs) {
  return !(rhs < lhs);
}

inline bool operator==(const DnsSdInstanceRecord& lhs,
                       const DnsSdInstanceRecord& rhs) {
  return lhs <= rhs && lhs >= rhs;
}

inline bool operator!=(const DnsSdInstanceRecord& lhs,
                       const DnsSdInstanceRecord& rhs) {
  return !(lhs == rhs);
}

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_PUBLIC_DNS_SD_INSTANCE_RECORD_H_
