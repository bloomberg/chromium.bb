// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_records.h"

#include "absl/strings/match.h"
#include "absl/strings/str_join.h"

namespace cast {
namespace mdns {

DomainName::DomainName(const std::vector<absl::string_view>& labels)
    : DomainName(labels.begin(), labels.end()) {}

DomainName::DomainName(std::initializer_list<absl::string_view> labels)
    : DomainName(labels.begin(), labels.end()) {}

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

size_t DomainName::MaxWireSize() const {
  return max_wire_size_;
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

size_t RawRecordRdata::MaxWireSize() const {
  // max_wire_size includes uint16_t record length field.
  return sizeof(uint16_t) + rdata_.size();
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
  return priority_ == rhs.priority_ && weight_ == rhs.weight_ &&
         port_ == rhs.port_ && target_ == rhs.target_;
}

bool SrvRecordRdata::operator!=(const SrvRecordRdata& rhs) const {
  return !(*this == rhs);
}

size_t SrvRecordRdata::MaxWireSize() const {
  // max_wire_size includes uint16_t record length field.
  return sizeof(uint16_t) + sizeof(priority_) + sizeof(weight_) +
         sizeof(port_) + target_.MaxWireSize();
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

size_t ARecordRdata::MaxWireSize() const {
  // max_wire_size includes uint16_t record length field.
  return sizeof(uint16_t) + IPAddress::kV4Size;
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

size_t AAAARecordRdata::MaxWireSize() const {
  // max_wire_size includes uint16_t record length field.
  return sizeof(uint16_t) + IPAddress::kV6Size;
}

PtrRecordRdata::PtrRecordRdata(DomainName ptr_domain)
    : ptr_domain_(ptr_domain) {}

bool PtrRecordRdata::operator==(const PtrRecordRdata& rhs) const {
  return ptr_domain_ == rhs.ptr_domain_;
}

bool PtrRecordRdata::operator!=(const PtrRecordRdata& rhs) const {
  return !(*this == rhs);
}

size_t PtrRecordRdata::MaxWireSize() const {
  // max_wire_size includes uint16_t record length field.
  return sizeof(uint16_t) + ptr_domain_.MaxWireSize();
}

TxtRecordRdata::TxtRecordRdata(const std::vector<absl::string_view>& texts)
    : TxtRecordRdata(texts.begin(), texts.end()) {}

TxtRecordRdata::TxtRecordRdata(std::initializer_list<absl::string_view> texts)
    : TxtRecordRdata(texts.begin(), texts.end()) {}

bool TxtRecordRdata::operator==(const TxtRecordRdata& rhs) const {
  return texts_ == rhs.texts_;
}

bool TxtRecordRdata::operator!=(const TxtRecordRdata& rhs) const {
  return !(*this == rhs);
}

size_t TxtRecordRdata::MaxWireSize() const {
  return max_wire_size_;
}

MdnsRecord::MdnsRecord(DomainName name,
                       DnsType type,
                       DnsClass record_class,
                       bool cache_flush,
                       uint32_t ttl,
                       Rdata rdata)
    : name_(std::move(name)),
      type_(type),
      record_class_(record_class),
      cache_flush_(cache_flush),
      ttl_(ttl),
      rdata_(std::move(rdata)) {
  OSP_DCHECK(!name_.empty());
  OSP_DCHECK(
      (type == DnsType::kSRV &&
       absl::holds_alternative<SrvRecordRdata>(rdata_)) ||
      (type == DnsType::kA && absl::holds_alternative<ARecordRdata>(rdata_)) ||
      (type == DnsType::kAAAA &&
       absl::holds_alternative<AAAARecordRdata>(rdata_)) ||
      (type == DnsType::kPTR &&
       absl::holds_alternative<PtrRecordRdata>(rdata_)) ||
      (type == DnsType::kTXT &&
       absl::holds_alternative<TxtRecordRdata>(rdata_)) ||
      absl::holds_alternative<RawRecordRdata>(rdata_));
}

bool MdnsRecord::operator==(const MdnsRecord& rhs) const {
  return type_ == rhs.type_ && record_class_ == rhs.record_class_ &&
         cache_flush_ == rhs.cache_flush_ && ttl_ == rhs.ttl_ &&
         name_ == rhs.name_ && rdata_ == rhs.rdata_;
}

bool MdnsRecord::operator!=(const MdnsRecord& rhs) const {
  return !(*this == rhs);
}

size_t MdnsRecord::MaxWireSize() const {
  auto wire_size_visitor = [](auto&& arg) { return arg.MaxWireSize(); };
  return name_.MaxWireSize() + sizeof(type_) + sizeof(record_class_) +
         sizeof(ttl_) + absl::visit(wire_size_visitor, rdata_);
}

MdnsQuestion::MdnsQuestion(DomainName name,
                           DnsType type,
                           DnsClass record_class,
                           bool unicast_response)
    : name_(std::move(name)),
      type_(type),
      record_class_(record_class),
      unicast_response_(unicast_response) {
  OSP_CHECK(!name_.empty());
}

bool MdnsQuestion::operator==(const MdnsQuestion& rhs) const {
  return type_ == rhs.type_ && record_class_ == rhs.record_class_ &&
         unicast_response_ == rhs.unicast_response_ && name_ == rhs.name_;
}

bool MdnsQuestion::operator!=(const MdnsQuestion& rhs) const {
  return !(*this == rhs);
}

size_t MdnsQuestion::MaxWireSize() const {
  return name_.MaxWireSize() + sizeof(type_) + sizeof(record_class_);
}

MdnsMessage::MdnsMessage(uint16_t id, uint16_t flags)
    : id_(id), flags_(flags) {}

MdnsMessage::MdnsMessage(uint16_t id,
                         uint16_t flags,
                         std::vector<MdnsQuestion> questions,
                         std::vector<MdnsRecord> answers,
                         std::vector<MdnsRecord> authority_records,
                         std::vector<MdnsRecord> additional_records)
    : id_(id),
      flags_(flags),
      questions_(std::move(questions)),
      answers_(std::move(answers)),
      authority_records_(std::move(authority_records)),
      additional_records_(std::move(additional_records)) {
  OSP_DCHECK(questions_.size() < std::numeric_limits<uint16_t>::max());
  OSP_DCHECK(answers_.size() < std::numeric_limits<uint16_t>::max());
  OSP_DCHECK(authority_records_.size() < std::numeric_limits<uint16_t>::max());
  OSP_DCHECK(additional_records_.size() < std::numeric_limits<uint16_t>::max());

  for (const MdnsQuestion& question : questions_) {
    max_wire_size_ += question.MaxWireSize();
  }
  for (const MdnsRecord& record : answers_) {
    max_wire_size_ += record.MaxWireSize();
  }
  for (const MdnsRecord& record : authority_records_) {
    max_wire_size_ += record.MaxWireSize();
  }
  for (const MdnsRecord& record : additional_records_) {
    max_wire_size_ += record.MaxWireSize();
  }
}

bool MdnsMessage::operator==(const MdnsMessage& rhs) const {
  return id_ == rhs.id_ && flags_ == rhs.flags_ &&
         questions_ == rhs.questions_ && answers_ == rhs.answers_ &&
         authority_records_ == rhs.authority_records_ &&
         additional_records_ == rhs.additional_records_;
}

bool MdnsMessage::operator!=(const MdnsMessage& rhs) const {
  return !(*this == rhs);
}

size_t MdnsMessage::MaxWireSize() const {
  return max_wire_size_;
}

void MdnsMessage::AddQuestion(MdnsQuestion question) {
  OSP_DCHECK(questions_.size() < std::numeric_limits<uint16_t>::max());
  max_wire_size_ += question.MaxWireSize();
  questions_.emplace_back(std::move(question));
}

void MdnsMessage::AddAnswer(MdnsRecord record) {
  OSP_DCHECK(answers_.size() < std::numeric_limits<uint16_t>::max());
  max_wire_size_ += record.MaxWireSize();
  answers_.emplace_back(std::move(record));
}

void MdnsMessage::AddAuthorityRecord(MdnsRecord record) {
  OSP_DCHECK(authority_records_.size() < std::numeric_limits<uint16_t>::max());
  max_wire_size_ += record.MaxWireSize();
  authority_records_.emplace_back(std::move(record));
}

void MdnsMessage::AddAdditionalRecord(MdnsRecord record) {
  OSP_DCHECK(additional_records_.size() < std::numeric_limits<uint16_t>::max());
  max_wire_size_ += record.MaxWireSize();
  additional_records_.emplace_back(std::move(record));
}

}  // namespace mdns
}  // namespace cast
