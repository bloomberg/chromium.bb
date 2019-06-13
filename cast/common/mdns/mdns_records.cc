// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_records.h"

#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "platform/api/logging.h"

namespace cast {
namespace mdns {

bool IsValidDomainLabel(absl::string_view label) {
  const size_t label_size = label.size();
  return label_size > 0 && label_size <= kMaxLabelLength;
}

void DomainName::Clear() {
  max_wire_size_ = 1;
  labels_.clear();
}

bool DomainName::PushLabel(absl::string_view label) {
  if (!IsValidDomainLabel(label)) {
    OSP_DLOG_ERROR << "Invalid domain name label: \"" << label << "\"";
    return false;
  }
  // Include the label length byte in the size calculation.
  // Add terminating character byte to maximum domain length, as it only
  // limits label bytes and label length bytes.
  if (max_wire_size_ + label.size() + 1 > kMaxDomainNameLength + 1) {
    OSP_DLOG_ERROR << "Name too long. Cannot push label: \"" << label << "\"";
    return false;
  }

  labels_.push_back(static_cast<std::string>(label));

  // Update the size of the full name in wire format. Include the length byte in
  // the size calculation.
  max_wire_size_ += label.size() + 1;
  return true;
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

RawRecordRdata::RawRecordRdata(uint16_t type, std::vector<uint8_t> rdata)
    : type_(type), rdata_(std::move(rdata)) {
  // Ensure RDATA length does not exceed the maximum allowed.
  OSP_DCHECK(rdata.size() <= std::numeric_limits<uint16_t>::max());
  // Ensure known RDATA types never generate raw record instance.
  OSP_DCHECK(std::none_of(std::begin(kSupportedRdataTypes),
                          std::end(kSupportedRdataTypes),
                          [type](uint16_t entry) { return entry == type; }));
}

bool RawRecordRdata::operator==(const RawRecordRdata& rhs) const {
  return (type_ == rhs.type_) && (rdata_ == rhs.rdata_);
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

}  // namespace mdns
}  // namespace cast
