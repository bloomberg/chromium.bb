// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_RECORDS_H_
#define CAST_COMMON_MDNS_MDNS_RECORDS_H_

#include <initializer_list>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "cast/common/mdns/mdns_constants.h"
#include "osp_base/ip_address.h"
#include "platform/api/logging.h"

namespace cast {
namespace mdns {

using IPAddress = openscreen::IPAddress;

bool IsValidDomainLabel(absl::string_view label);

// Represents domain name as a collection of labels, ensures label length and
// domain name length requirements are met.
class DomainName {
 public:
  DomainName() = default;

  template <typename IteratorType>
  DomainName(IteratorType first, IteratorType last) {
    for (IteratorType i = first; i != last; ++i) {
      auto& label = *i;
      OSP_DCHECK(IsValidDomainLabel(label));
      // Include the label length byte in the size calculation.
      OSP_DCHECK(max_wire_size_ + label.size() + 1 <= kMaxDomainNameLength);
      labels_.emplace_back(static_cast<std::string>(label));
      // Update the size of the full name in wire format. Include the length
      // byte in the size calculation.
      max_wire_size_ += label.size() + 1;
    }
  }

  DomainName(std::initializer_list<absl::string_view> labels)
      : DomainName(labels.begin(), labels.end()) {}

  DomainName(const DomainName& other) = default;
  DomainName(DomainName&& other) noexcept = default;
  ~DomainName() = default;

  DomainName& operator=(const DomainName& other) = default;
  DomainName& operator=(DomainName&& other) noexcept = default;

  bool operator==(const DomainName& rhs) const;
  bool operator!=(const DomainName& rhs) const;

  // Returns a reference to the label at specified label_index. No bounds
  // checking is performed.
  const std::string& Label(size_t label_index) const;
  std::string ToString() const;

  // Returns the maximum space that the domain name could take up in its
  // on-the-wire format. This is an upper bound based on the length of the
  // labels that make up the domain name. It's possible that with domain name
  // compression the actual space taken in on-the-wire format is smaller.
  size_t max_wire_size() const { return max_wire_size_; }
  bool empty() const { return labels_.empty(); }
  size_t label_count() const { return labels_.size(); }

 private:
  // wire_size_ starts at 1 for the terminating character length.
  size_t max_wire_size_ = 1;
  std::vector<std::string> labels_;
};

std::ostream& operator<<(std::ostream& stream, const DomainName& domain_name);

// Parsed represenation of the extra data in a record. Does not include standard
// DNS record data such as TTL, Name, Type and Class. We use it to distinguish
// a raw record type that we do not know the identity of.
class RawRecordRdata {
 public:
  RawRecordRdata() = default;
  explicit RawRecordRdata(std::vector<uint8_t> rdata);
  RawRecordRdata(const uint8_t* begin, size_t size);
  RawRecordRdata(const RawRecordRdata& other) = default;
  RawRecordRdata(RawRecordRdata&& other) noexcept = default;
  ~RawRecordRdata() = default;

  RawRecordRdata& operator=(const RawRecordRdata& other) = default;
  RawRecordRdata& operator=(RawRecordRdata&& other) noexcept = default;

  bool operator==(const RawRecordRdata& rhs) const;
  bool operator!=(const RawRecordRdata& rhs) const;

  size_t max_wire_size() const { return rdata_.size(); }
  uint16_t size() const { return rdata_.size(); }
  const uint8_t* data() const { return rdata_.data(); }

 private:
  std::vector<uint8_t> rdata_;
};

// SRV record format (http://www.ietf.org/rfc/rfc2782.txt):
// 2 bytes network-order unsigned priority
// 2 bytes network-order unsigned weight
// 2 bytes network-order unsigned port
// target: domain name (on-the-wire representation)
class SrvRecordRdata {
 public:
  SrvRecordRdata() = default;
  SrvRecordRdata(uint16_t priority,
                 uint16_t weight,
                 uint16_t port,
                 DomainName target);
  SrvRecordRdata(const SrvRecordRdata& other) = default;
  SrvRecordRdata(SrvRecordRdata&& other) noexcept = default;
  ~SrvRecordRdata() = default;

  SrvRecordRdata& operator=(const SrvRecordRdata& other) = default;
  SrvRecordRdata& operator=(SrvRecordRdata&& other) noexcept = default;

  bool operator==(const SrvRecordRdata& rhs) const;
  bool operator!=(const SrvRecordRdata& rhs) const;

  size_t max_wire_size() const;
  uint16_t priority() const { return priority_; }
  uint16_t weight() const { return weight_; }
  uint16_t port() const { return port_; }
  const DomainName& target() const { return target_; }

 private:
  uint16_t priority_ = 0;
  uint16_t weight_ = 0;
  uint16_t port_ = 0;
  DomainName target_;
};

// A Record format (http://www.ietf.org/rfc/rfc1035.txt):
// 4 bytes for IP address.
class ARecordRdata {
 public:
  ARecordRdata() = default;
  explicit ARecordRdata(IPAddress ipv4_address);
  ARecordRdata(const ARecordRdata& other) = default;
  ARecordRdata(ARecordRdata&& other) noexcept = default;
  ~ARecordRdata() = default;

  ARecordRdata& operator=(const ARecordRdata& other) = default;
  ARecordRdata& operator=(ARecordRdata&& other) noexcept = default;

  bool operator==(const ARecordRdata& rhs) const;
  bool operator!=(const ARecordRdata& rhs) const;

  size_t max_wire_size() const { return IPAddress::kV4Size; };
  const IPAddress& ipv4_address() const { return ipv4_address_; }

 private:
  IPAddress ipv4_address_;
};

// AAAA Record format (http://www.ietf.org/rfc/rfc1035.txt):
// 16 bytes for IP address.
class AAAARecordRdata {
 public:
  AAAARecordRdata() = default;
  explicit AAAARecordRdata(IPAddress ipv6_address);
  AAAARecordRdata(const AAAARecordRdata& other) = default;
  AAAARecordRdata(AAAARecordRdata&& other) noexcept = default;
  ~AAAARecordRdata() = default;

  AAAARecordRdata& operator=(const AAAARecordRdata& other) = default;
  AAAARecordRdata& operator=(AAAARecordRdata&& other) noexcept = default;

  bool operator==(const AAAARecordRdata& rhs) const;
  bool operator!=(const AAAARecordRdata& rhs) const;

  size_t max_wire_size() const { return IPAddress::kV6Size; }
  const IPAddress& ipv6_address() const { return ipv6_address_; }

 private:
  IPAddress ipv6_address_;
};

// PTR record format (http://www.ietf.org/rfc/rfc1035.txt):
// domain: On the wire representation of domain name.
class PtrRecordRdata {
 public:
  PtrRecordRdata() = default;
  explicit PtrRecordRdata(DomainName ptr_domain);
  PtrRecordRdata(const PtrRecordRdata& other) = default;
  PtrRecordRdata(PtrRecordRdata&& other) noexcept = default;
  ~PtrRecordRdata() = default;

  PtrRecordRdata& operator=(const PtrRecordRdata& other) = default;
  PtrRecordRdata& operator=(PtrRecordRdata&& other) noexcept = default;

  bool operator==(const PtrRecordRdata& rhs) const;
  bool operator!=(const PtrRecordRdata& rhs) const;

  size_t max_wire_size() const { return ptr_domain_.max_wire_size(); }
  const DomainName& ptr_domain() const { return ptr_domain_; }

 private:
  DomainName ptr_domain_;
};

// TXT record format (http://www.ietf.org/rfc/rfc1035.txt):
// texts: One or more <character-string>s.
// a <character-string> is a length octet followed by as many characters.
class TxtRecordRdata {
 public:
  TxtRecordRdata() = default;
  explicit TxtRecordRdata(std::vector<std::string> texts);
  TxtRecordRdata(const TxtRecordRdata& other) = default;
  TxtRecordRdata(TxtRecordRdata&& other) noexcept = default;
  ~TxtRecordRdata() = default;

  TxtRecordRdata& operator=(const TxtRecordRdata& other) = default;
  TxtRecordRdata& operator=(TxtRecordRdata&& other) noexcept = default;

  bool operator==(const TxtRecordRdata& rhs) const;
  bool operator!=(const TxtRecordRdata& rhs) const;

  size_t max_wire_size() const;
  const std::vector<std::string>& texts() const { return texts_; }

 private:
  std::vector<std::string> texts_;
};

using Rdata = absl::variant<RawRecordRdata,
                            SrvRecordRdata,
                            ARecordRdata,
                            AAAARecordRdata,
                            PtrRecordRdata,
                            TxtRecordRdata>;

// Resource record top level format (http://www.ietf.org/rfc/rfc1035.txt):
// name: the name of the node to which this resource record pertains.
// type: 2 bytes network-order RR TYPE code.
// class: 2 bytes network-order RR CLASS code.
// ttl: 4 bytes network-order cache time interval.
// rdata:  RDATA describing the resource.  The format of this information varies
// according to the TYPE and CLASS of the resource record.
class MdnsRecord {
 public:
  MdnsRecord() = default;
  MdnsRecord(DomainName name,
             uint16_t type,
             uint16_t record_class,
             uint32_t ttl,
             Rdata rdata);
  MdnsRecord(const MdnsRecord& other) = default;
  MdnsRecord(MdnsRecord&& other) noexcept = default;
  ~MdnsRecord() = default;

  MdnsRecord& operator=(const MdnsRecord& other) = default;
  MdnsRecord& operator=(MdnsRecord&& other) noexcept = default;

  bool operator==(const MdnsRecord& other) const;
  bool operator!=(const MdnsRecord& other) const;

  size_t max_wire_size() const;
  const DomainName& name() const { return name_; }
  uint16_t type() const { return type_; }
  uint16_t record_class() const { return record_class_; }
  uint32_t ttl() const { return ttl_; }
  const Rdata& rdata() const { return rdata_; }

 private:
  DomainName name_;
  uint16_t type_ = 0;
  uint16_t record_class_ = 0;
  uint32_t ttl_ = 0;
  // Default-constructed Rdata contains default-constructed RawRecordRdata
  // as it is the first alternative type and it is default-constructible.
  Rdata rdata_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_RECORDS_H_
