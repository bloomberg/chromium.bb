// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_records.h"

#include "absl/strings/match.h"
#include "absl/strings/str_join.h"

namespace cast {
namespace mdns {

bool IsValidDomainLabel(absl::string_view label) {
  const size_t label_size = label.size();
  return label_size > 0 && label_size <= kMaxLabelLength;
}

const std::string& DomainName::Label(size_t label_index) const {
  OSP_DCHECK(label_index < labels_.size());
  return labels_[label_index];
}

std::string DomainName::ToString() const {
  return absl::StrJoin(labels_, ".");
}

bool DomainName::operator==(const DomainName& rhs) const {
  auto predicate = [](const std::string& left, const std::string& right) {
    return absl::EqualsIgnoreCase(left, right);
  };
  return std::equal(labels_.begin(), labels_.end(), rhs.labels_.begin(),
                    rhs.labels_.end(), predicate);
}

bool DomainName::operator!=(const DomainName& rhs) const {
  return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& stream, const DomainName& domain_name) {
  return stream << domain_name.ToString();
}

RawRecordRdata::RawRecordRdata(std::vector<uint8_t> rdata)
    : rdata_(std::move(rdata)) {
  // Ensure RDATA length does not exceed the maximum allowed.
  OSP_DCHECK(rdata_.size() <= std::numeric_limits<uint16_t>::max());
}

RawRecordRdata::RawRecordRdata(const uint8_t* begin, size_t size)
    : RawRecordRdata(std::vector<uint8_t>(begin, begin + size)) {}

bool RawRecordRdata::operator==(const RawRecordRdata& rhs) const {
  return rdata_ == rhs.rdata_;
}

bool RawRecordRdata::operator!=(const RawRecordRdata& rhs) const {
  return !(*this == rhs);
}

SrvRecordRdata::SrvRecordRdata(uint16_t priority,
                               uint16_t weight,
                               uint16_t port,
                               DomainName target)
    : priority_(priority),
      weight_(weight),
      port_(port),
      target_(std::move(target)) {}

bool SrvRecordRdata::operator==(const SrvRecordRdata& rhs) const {
  return (priority_ == rhs.priority_) && (weight_ == rhs.weight_) &&
         (port_ == rhs.port_) && (target_ == rhs.target_);
}

bool SrvRecordRdata::operator!=(const SrvRecordRdata& rhs) const {
  return !(*this == rhs);
}

size_t SrvRecordRdata::max_wire_size() const {
  return sizeof(priority_) + sizeof(weight_) + sizeof(port_) +
         target_.max_wire_size();
}

ARecordRdata::ARecordRdata(IPAddress ipv4_address)
    : ipv4_address_(std::move(ipv4_address)) {
  OSP_CHECK(ipv4_address_.IsV4());
}

bool ARecordRdata::operator==(const ARecordRdata& rhs) const {
  return ipv4_address_ == rhs.ipv4_address_;
}

bool ARecordRdata::operator!=(const ARecordRdata& rhs) const {
  return !(*this == rhs);
}

AAAARecordRdata::AAAARecordRdata(IPAddress ipv6_address)
    : ipv6_address_(std::move(ipv6_address)) {
  OSP_CHECK(ipv6_address_.IsV6());
}

bool AAAARecordRdata::operator==(const AAAARecordRdata& rhs) const {
  return ipv6_address_ == rhs.ipv6_address_;
}

bool AAAARecordRdata::operator!=(const AAAARecordRdata& rhs) const {
  return !(*this == rhs);
}

PtrRecordRdata::PtrRecordRdata(DomainName ptr_domain)
    : ptr_domain_(ptr_domain) {}

bool PtrRecordRdata::operator==(const PtrRecordRdata& rhs) const {
  return ptr_domain_ == rhs.ptr_domain_;
}

bool PtrRecordRdata::operator!=(const PtrRecordRdata& rhs) const {
  return !(*this == rhs);
}

TxtRecordRdata::TxtRecordRdata(std::vector<std::string> texts)
    : texts_(std::move(texts)) {
  OSP_DCHECK(
      std::none_of(texts_.begin(), texts_.end(),
                   [](const std::string& entry) { return entry.empty(); }));
}

bool TxtRecordRdata::operator==(const TxtRecordRdata& rhs) const {
  return texts_ == rhs.texts_;
}

bool TxtRecordRdata::operator!=(const TxtRecordRdata& rhs) const {
  return !(*this == rhs);
}

size_t TxtRecordRdata::max_wire_size() const {
  size_t total_size = 0;
  for (const auto& entry : texts_) {
    if (!entry.empty()) {
      total_size += entry.size() + 1;  // Don't forget the length byte.
    }
  }
  // We will always return at least 1 NULL byte.
  return total_size ? total_size : 1;
}

MdnsRecord::MdnsRecord(DomainName name,
                       uint16_t type,
                       uint16_t record_class,
                       uint32_t ttl,
                       Rdata rdata)
    : name_(std::move(name)),
      type_(type),
      record_class_(record_class),
      ttl_(ttl),
      rdata_(std::move(rdata)) {
  OSP_DCHECK(!name_.empty());
  OSP_DCHECK(
      (type == kTypeSRV && absl::holds_alternative<SrvRecordRdata>(rdata_)) ||
      (type == kTypeA && absl::holds_alternative<ARecordRdata>(rdata_)) ||
      (type == kTypeAAAA && absl::holds_alternative<AAAARecordRdata>(rdata_)) ||
      (type == kTypePTR && absl::holds_alternative<PtrRecordRdata>(rdata_)) ||
      (type == kTypeTXT && absl::holds_alternative<TxtRecordRdata>(rdata_)) ||
      absl::holds_alternative<RawRecordRdata>(rdata_));
}

bool MdnsRecord::operator==(const MdnsRecord& rhs) const {
  return type_ == rhs.type_ && record_class_ == rhs.record_class_ &&
         ttl_ == rhs.ttl_ && name_ == rhs.name_ && rdata_ == rhs.rdata_;
}

bool MdnsRecord::operator!=(const MdnsRecord& rhs) const {
  return !(*this == rhs);
}

size_t MdnsRecord::max_wire_size() const {
  auto wire_size_visitor = [](auto&& arg) {
    return arg.max_wire_size();
  };
  return name_.max_wire_size() + sizeof(type_) + sizeof(record_class_) +
         sizeof(ttl_) + absl::visit(wire_size_visitor, rdata_);
}

}  // namespace mdns
}  // namespace cast
