// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_records.h"

#include <atomic>
#include <cctype>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "discovery/mdns/mdns_writer.h"

namespace openscreen {
namespace discovery {

namespace {

inline int CompareIgnoreCase(const std::string& x, const std::string& y) {
  size_t i = 0;
  for (; i < x.size(); i++) {
    if (i == y.size()) {
      return 1;
    }
    const char& x_char = std::tolower(x[i]);
    const char& y_char = std::tolower(y[i]);
    if (x_char < y_char) {
      return -1;
    } else if (y_char < x_char) {
      return 1;
    }
  }
  return i == y.size() ? 0 : -1;
}

template <typename RDataType>
bool IsGreaterThan(const Rdata& lhs, const Rdata& rhs) {
  const RDataType& lhs_cast = absl::get<RDataType>(lhs);
  const RDataType& rhs_cast = absl::get<RDataType>(rhs);

  size_t lhs_size = lhs_cast.MaxWireSize();
  size_t rhs_size = rhs_cast.MaxWireSize();
  size_t min_size = std::min(lhs_size, rhs_size);

  uint8_t lhs_bytes[lhs_size];
  uint8_t rhs_bytes[rhs_size];
  MdnsWriter lhs_writer(lhs_bytes, lhs_size);
  MdnsWriter rhs_writer(rhs_bytes, rhs_size);

  lhs_writer.Write(lhs_cast);
  rhs_writer.Write(rhs_cast);
  for (size_t i = 0; i < min_size; i++) {
    if (lhs_bytes[i] != rhs_bytes[i]) {
      return lhs_bytes[i] > rhs_bytes[i];
    }
  }

  if (lhs_size == rhs_size) {
    return false;
  }

  return lhs_size > rhs_size;
}

bool IsGreaterThan(DnsType type, const Rdata& lhs, const Rdata& rhs) {
  switch (type) {
    case DnsType::kA:
      return IsGreaterThan<ARecordRdata>(lhs, rhs);
    case DnsType::kPTR:
      return IsGreaterThan<PtrRecordRdata>(lhs, rhs);
    case DnsType::kTXT:
      return IsGreaterThan<TxtRecordRdata>(lhs, rhs);
    case DnsType::kAAAA:
      return IsGreaterThan<AAAARecordRdata>(lhs, rhs);
    case DnsType::kSRV:
      return IsGreaterThan<SrvRecordRdata>(lhs, rhs);
    case DnsType::kNSEC:
      return IsGreaterThan<NsecRecordRdata>(lhs, rhs);
    default:
      return IsGreaterThan<RawRecordRdata>(lhs, rhs);
  }
}

}  // namespace

bool IsValidDomainLabel(absl::string_view label) {
  const size_t label_size = label.size();
  return label_size > 0 && label_size <= kMaxLabelLength;
}

DomainName::DomainName() = default;

DomainName::DomainName(std::vector<std::string> labels)
    : DomainName(labels.begin(), labels.end()) {}

DomainName::DomainName(const std::vector<absl::string_view>& labels)
    : DomainName(labels.begin(), labels.end()) {}

DomainName::DomainName(std::initializer_list<absl::string_view> labels)
    : DomainName(labels.begin(), labels.end()) {}

DomainName::DomainName(const DomainName& other) = default;

DomainName::DomainName(DomainName&& other) = default;

DomainName& DomainName::operator=(const DomainName& rhs) = default;

DomainName& DomainName::operator=(DomainName&& rhs) = default;

std::string DomainName::ToString() const {
  return absl::StrJoin(labels_, ".");
}

bool DomainName::operator<(const DomainName& rhs) const {
  size_t i = 0;
  for (; i < labels_.size(); i++) {
    if (i == rhs.labels_.size()) {
      return false;
    } else {
      int result = CompareIgnoreCase(labels_[i], rhs.labels_[i]);
      if (result < 0) {
        return true;
      } else if (result > 0) {
        return false;
      }
    }
  }
  return i < rhs.labels_.size();
}

bool DomainName::operator<=(const DomainName& rhs) const {
  return (*this < rhs) || (*this == rhs);
}

bool DomainName::operator>(const DomainName& rhs) const {
  return !(*this < rhs) && !(*this == rhs);
}

bool DomainName::operator>=(const DomainName& rhs) const {
  return !(*this < rhs);
}

bool DomainName::operator==(const DomainName& rhs) const {
  if (labels_.size() != rhs.labels_.size()) {
    return false;
  }
  for (size_t i = 0; i < labels_.size(); i++) {
    if (CompareIgnoreCase(labels_[i], rhs.labels_[i]) != 0) {
      return false;
    }
  }
  return true;
}

bool DomainName::operator!=(const DomainName& rhs) const {
  return !(*this == rhs);
}

size_t DomainName::MaxWireSize() const {
  return max_wire_size_;
}

RawRecordRdata::RawRecordRdata() = default;

RawRecordRdata::RawRecordRdata(std::vector<uint8_t> rdata)
    : rdata_(std::move(rdata)) {
  // Ensure RDATA length does not exceed the maximum allowed.
  OSP_DCHECK(rdata_.size() <= std::numeric_limits<uint16_t>::max());
}

RawRecordRdata::RawRecordRdata(const uint8_t* begin, size_t size)
    : RawRecordRdata(std::vector<uint8_t>(begin, begin + size)) {}

RawRecordRdata::RawRecordRdata(const RawRecordRdata& other) = default;

RawRecordRdata::RawRecordRdata(RawRecordRdata&& other) = default;

RawRecordRdata& RawRecordRdata::operator=(const RawRecordRdata& rhs) = default;

RawRecordRdata& RawRecordRdata::operator=(RawRecordRdata&& rhs) = default;

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

SrvRecordRdata::SrvRecordRdata() = default;

SrvRecordRdata::SrvRecordRdata(uint16_t priority,
                               uint16_t weight,
                               uint16_t port,
                               DomainName target)
    : priority_(priority),
      weight_(weight),
      port_(port),
      target_(std::move(target)) {}

SrvRecordRdata::SrvRecordRdata(const SrvRecordRdata& other) = default;

SrvRecordRdata::SrvRecordRdata(SrvRecordRdata&& other) = default;

SrvRecordRdata& SrvRecordRdata::operator=(const SrvRecordRdata& rhs) = default;

SrvRecordRdata& SrvRecordRdata::operator=(SrvRecordRdata&& rhs) = default;

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

ARecordRdata::ARecordRdata() = default;

ARecordRdata::ARecordRdata(IPAddress ipv4_address)
    : ipv4_address_(std::move(ipv4_address)) {
  OSP_CHECK(ipv4_address_.IsV4());
}

ARecordRdata::ARecordRdata(const ARecordRdata& other) = default;

ARecordRdata::ARecordRdata(ARecordRdata&& other) = default;

ARecordRdata& ARecordRdata::operator=(const ARecordRdata& rhs) = default;

ARecordRdata& ARecordRdata::operator=(ARecordRdata&& rhs) = default;

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

AAAARecordRdata::AAAARecordRdata() = default;

AAAARecordRdata::AAAARecordRdata(IPAddress ipv6_address)
    : ipv6_address_(std::move(ipv6_address)) {
  OSP_CHECK(ipv6_address_.IsV6());
}

AAAARecordRdata::AAAARecordRdata(const AAAARecordRdata& other) = default;

AAAARecordRdata::AAAARecordRdata(AAAARecordRdata&& other) = default;

AAAARecordRdata& AAAARecordRdata::operator=(const AAAARecordRdata& rhs) =
    default;

AAAARecordRdata& AAAARecordRdata::operator=(AAAARecordRdata&& rhs) = default;

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

PtrRecordRdata::PtrRecordRdata() = default;

PtrRecordRdata::PtrRecordRdata(DomainName ptr_domain)
    : ptr_domain_(ptr_domain) {}

PtrRecordRdata::PtrRecordRdata(const PtrRecordRdata& other) = default;

PtrRecordRdata::PtrRecordRdata(PtrRecordRdata&& other) = default;

PtrRecordRdata& PtrRecordRdata::operator=(const PtrRecordRdata& rhs) = default;

PtrRecordRdata& PtrRecordRdata::operator=(PtrRecordRdata&& rhs) = default;

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

TxtRecordRdata::TxtRecordRdata() = default;

TxtRecordRdata::TxtRecordRdata(std::vector<Entry> texts) {
  if (texts.size() > 0) {
    texts_.reserve(texts.size());
    // max_wire_size includes uint16_t record length field.
    max_wire_size_ = sizeof(uint16_t);
    for (const auto& text : texts) {
      OSP_DCHECK(!text.empty());
      texts_.push_back(
          std::string(reinterpret_cast<const char*>(text.data()), text.size()));
      // Include the length byte in the size calculation.
      max_wire_size_ += text.size() + 1;
    }
  }
}

TxtRecordRdata::TxtRecordRdata(const TxtRecordRdata& other) = default;

TxtRecordRdata::TxtRecordRdata(TxtRecordRdata&& other) = default;

TxtRecordRdata& TxtRecordRdata::operator=(const TxtRecordRdata& rhs) = default;

TxtRecordRdata& TxtRecordRdata::operator=(TxtRecordRdata&& rhs) = default;

bool TxtRecordRdata::operator==(const TxtRecordRdata& rhs) const {
  return texts_ == rhs.texts_;
}

bool TxtRecordRdata::operator!=(const TxtRecordRdata& rhs) const {
  return !(*this == rhs);
}

size_t TxtRecordRdata::MaxWireSize() const {
  return max_wire_size_;
}

NsecRecordRdata::NsecRecordRdata() = default;

NsecRecordRdata::NsecRecordRdata(DomainName next_domain_name,
                                 std::vector<DnsType> types)
    : types_(std::move(types)), next_domain_name_(std::move(next_domain_name)) {
  // Sort the types_ array for easier comparison later.
  std::sort(types_.begin(), types_.end());

  // Calculate the bitmaps as described in RFC 4034 Section 4.1.2.
  std::vector<uint8_t> block_contents;
  uint8_t current_block = 0;
  for (auto type : types_) {
    const uint16_t type_int = static_cast<uint16_t>(type);
    const uint8_t block = static_cast<uint8_t>(type_int >> 8);
    const uint8_t block_position = static_cast<uint8_t>(type_int & 0xFF);
    const uint8_t byte_bit_is_at = block_position >> 3;         // First 5 bits.
    const uint8_t byte_mask = 0x80 >> (block_position & 0x07);  // Last 3 bits.

    // If the block has changed, write the previous block's info and all of its
    // contents to the |encoded_types_| vector.
    if (block > current_block) {
      if (!block_contents.empty()) {
        encoded_types_.push_back(current_block);
        encoded_types_.push_back(static_cast<uint8_t>(block_contents.size()));
        encoded_types_.insert(encoded_types_.end(), block_contents.begin(),
                              block_contents.end());
      }
      block_contents = std::vector<uint8_t>();
      current_block = block;
    }

    // Make sure |block_contents| is large enough to hold the bit representing
    // the new type , then set it.
    if (block_contents.size() <= byte_bit_is_at) {
      block_contents.insert(block_contents.end(),
                            byte_bit_is_at - block_contents.size() + 1, 0x00);
    }

    block_contents[byte_bit_is_at] |= byte_mask;
  }

  if (!block_contents.empty()) {
    encoded_types_.push_back(current_block);
    encoded_types_.push_back(static_cast<uint8_t>(block_contents.size()));
    encoded_types_.insert(encoded_types_.end(), block_contents.begin(),
                          block_contents.end());
  }
}

NsecRecordRdata::NsecRecordRdata(const NsecRecordRdata& other) = default;

NsecRecordRdata::NsecRecordRdata(NsecRecordRdata&& other) = default;

NsecRecordRdata& NsecRecordRdata::operator=(const NsecRecordRdata& rhs) =
    default;

NsecRecordRdata& NsecRecordRdata::operator=(NsecRecordRdata&& rhs) = default;

bool NsecRecordRdata::operator==(const NsecRecordRdata& rhs) const {
  return types_ == rhs.types_ && next_domain_name_ == rhs.next_domain_name_;
}

bool NsecRecordRdata::operator!=(const NsecRecordRdata& rhs) const {
  return !(*this == rhs);
}

size_t NsecRecordRdata::MaxWireSize() const {
  return next_domain_name_.MaxWireSize() + encoded_types_.size();
}

MdnsRecord::MdnsRecord() = default;

MdnsRecord::MdnsRecord(DomainName name,
                       DnsType dns_type,
                       DnsClass dns_class,
                       RecordType record_type,
                       std::chrono::seconds ttl,
                       Rdata rdata)
    : name_(std::move(name)),
      dns_type_(dns_type),
      dns_class_(dns_class),
      record_type_(record_type),
      ttl_(ttl),
      rdata_(std::move(rdata)) {
  OSP_DCHECK(!name_.empty());
  OSP_DCHECK_LE(ttl_.count(), std::numeric_limits<uint32_t>::max());
  OSP_DCHECK((dns_type == DnsType::kSRV &&
              absl::holds_alternative<SrvRecordRdata>(rdata_)) ||
             (dns_type == DnsType::kA &&
              absl::holds_alternative<ARecordRdata>(rdata_)) ||
             (dns_type == DnsType::kAAAA &&
              absl::holds_alternative<AAAARecordRdata>(rdata_)) ||
             (dns_type == DnsType::kPTR &&
              absl::holds_alternative<PtrRecordRdata>(rdata_)) ||
             (dns_type == DnsType::kTXT &&
              absl::holds_alternative<TxtRecordRdata>(rdata_)) ||
             (dns_type == DnsType::kNSEC &&
              absl::holds_alternative<NsecRecordRdata>(rdata_)) ||
             absl::holds_alternative<RawRecordRdata>(rdata_));
}

MdnsRecord::MdnsRecord(const MdnsRecord& other) = default;

MdnsRecord::MdnsRecord(MdnsRecord&& other) = default;

MdnsRecord& MdnsRecord::operator=(const MdnsRecord& rhs) = default;

MdnsRecord& MdnsRecord::operator=(MdnsRecord&& rhs) = default;

bool MdnsRecord::operator==(const MdnsRecord& rhs) const {
  return dns_type_ == rhs.dns_type_ && dns_class_ == rhs.dns_class_ &&
         record_type_ == rhs.record_type_ && ttl_ == rhs.ttl_ &&
         name_ == rhs.name_ && rdata_ == rhs.rdata_;
}

bool MdnsRecord::operator!=(const MdnsRecord& rhs) const {
  return !(*this == rhs);
}

bool MdnsRecord::operator>(const MdnsRecord& rhs) const {
  // Returns the record which is lexicographically later. The determination of
  // "lexicographically later" is performed by first comparing the record class,
  // then the record type, then raw comparison of the binary content of the
  // rdata without regard for meaning or structure.
  if (dns_class() != rhs.dns_class()) {
    return dns_class() > rhs.dns_class();
  }

  uint16_t this_type = static_cast<uint16_t>(dns_type()) & kClassMask;
  uint16_t other_type = static_cast<uint16_t>(rhs.dns_type()) & kClassMask;
  if (this_type != other_type) {
    return this_type > other_type;
  }

  return IsGreaterThan(dns_type(), rdata(), rhs.rdata());
}

bool MdnsRecord::operator<(const MdnsRecord& rhs) const {
  return rhs > *this;
}

bool MdnsRecord::operator<=(const MdnsRecord& rhs) const {
  return !(*this > rhs);
}

bool MdnsRecord::operator>=(const MdnsRecord& rhs) const {
  return !(*this < rhs);
}

size_t MdnsRecord::MaxWireSize() const {
  auto wire_size_visitor = [](auto&& arg) { return arg.MaxWireSize(); };
  // NAME size, 2-byte TYPE, 2-byte CLASS, 4-byte TTL, RDATA size
  return name_.MaxWireSize() + absl::visit(wire_size_visitor, rdata_) + 8;
}

MdnsRecord CreateAddressRecord(DomainName name, const IPAddress& address) {
  Rdata rdata;
  DnsType type;
  std::chrono::seconds ttl;
  if (address.IsV4()) {
    type = DnsType::kA;
    rdata = ARecordRdata(address);
    ttl = kARecordTtl;
  } else {
    type = DnsType::kAAAA;
    rdata = AAAARecordRdata(address);
    ttl = kAAAARecordTtl;
  }

  return MdnsRecord(std::move(name), type, DnsClass::kIN, RecordType::kUnique,
                    ttl, std::move(rdata));
}

MdnsQuestion::MdnsQuestion(DomainName name,
                           DnsType dns_type,
                           DnsClass dns_class,
                           ResponseType response_type)
    : name_(std::move(name)),
      dns_type_(dns_type),
      dns_class_(dns_class),
      response_type_(response_type) {
  OSP_CHECK(!name_.empty());
}

bool MdnsQuestion::operator==(const MdnsQuestion& rhs) const {
  return dns_type_ == rhs.dns_type_ && dns_class_ == rhs.dns_class_ &&
         response_type_ == rhs.response_type_ && name_ == rhs.name_;
}

bool MdnsQuestion::operator!=(const MdnsQuestion& rhs) const {
  return !(*this == rhs);
}

size_t MdnsQuestion::MaxWireSize() const {
  // NAME size, 2-byte TYPE, 2-byte CLASS
  return name_.MaxWireSize() + 4;
}

MdnsMessage::MdnsMessage(uint16_t id, MessageType type)
    : id_(id), type_(type) {}

MdnsMessage::MdnsMessage(uint16_t id,
                         MessageType type,
                         std::vector<MdnsQuestion> questions,
                         std::vector<MdnsRecord> answers,
                         std::vector<MdnsRecord> authority_records,
                         std::vector<MdnsRecord> additional_records)
    : id_(id),
      type_(type),
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
  return id_ == rhs.id_ && type_ == rhs.type_ && questions_ == rhs.questions_ &&
         answers_ == rhs.answers_ &&
         authority_records_ == rhs.authority_records_ &&
         additional_records_ == rhs.additional_records_;
}

bool MdnsMessage::operator!=(const MdnsMessage& rhs) const {
  return !(*this == rhs);
}

bool MdnsMessage::IsProbeQuery() const {
  // A message is a probe query if it contains records in the authority section
  // which answer the question being asked.
  if (questions().empty() || authority_records().empty()) {
    return false;
  }

  for (const MdnsQuestion& question : questions_) {
    for (const MdnsRecord& record : authority_records_) {
      if (question.name() == record.name() &&
          ((question.dns_type() == record.dns_type()) ||
           (question.dns_type() == DnsType::kANY)) &&
          ((question.dns_class() == record.dns_class()) ||
           (question.dns_class() == DnsClass::kANY))) {
        return true;
      }
    }
  }

  return false;
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

bool MdnsMessage::CanAddRecord(const MdnsRecord& record) {
  return (max_wire_size_ + record.MaxWireSize()) < kMaxMulticastMessageSize;
}

uint16_t CreateMessageId() {
  static std::atomic<uint16_t> id(0);
  return id++;
}

}  // namespace discovery
}  // namespace openscreen
