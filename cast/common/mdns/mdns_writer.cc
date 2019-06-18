// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_writer.h"

#include "absl/hash/hash.h"
#include "absl/strings/ascii.h"
#include "cast/common/mdns/mdns_constants.h"
#include "platform/api/logging.h"

namespace cast {
namespace mdns {

namespace {

std::vector<uint64_t> ComputeDomainNameSubhashes(const DomainName& name) {
  // Based on absl Hash128to64 that combines two 64-bit hashes into one
  auto hash_combiner = [](uint64_t seed, const std::string& value) -> uint64_t {
    static const uint64_t kMultiplier = UINT64_C(0x9ddfea08eb382d69);
    const uint64_t hash_value = absl::Hash<std::string>{}(value);
    uint64_t a = (hash_value ^ seed) * kMultiplier;
    a ^= (a >> 47);
    uint64_t b = (seed ^ a) * kMultiplier;
    b ^= (b >> 47);
    b *= kMultiplier;
    return b;
  };

  // Use a large prime between 2^63 and 2^64 as a starting value.
  // This is taken from absl::Hash implementation.
  uint64_t hash_value = UINT64_C(0xc3a5c85c97cb3127);
  std::vector<uint64_t> subhashes(name.label_count());
  for (size_t i = name.label_count(); i-- > 0;) {
    hash_value =
        hash_combiner(hash_value, absl::AsciiStrToLower(name.Label(i)));
    subhashes[i] = hash_value;
  }
  return subhashes;
}

// This helper method writes the number of bytes between |begin| and |end| minus
// the size of the uint16_t into the uint16_t length field at |begin|. The
// method returns true if the number of bytes between |begin| and |end| fits in
// uint16_t type, returns false otherwise.
bool UpdateRecordLength(const uint8_t* end, uint8_t* begin) {
  OSP_DCHECK_LE(begin + sizeof(uint16_t), end);
  ptrdiff_t record_length = end - begin - sizeof(uint16_t);
  if (record_length <= std::numeric_limits<uint16_t>::max()) {
    openscreen::WriteBigEndian<uint16_t>(record_length, begin);
    return true;
  }
  return false;
}

}  // namespace

MdnsWriter::MdnsWriter(uint8_t* buffer, size_t length)
    : BigEndianWriter(buffer, length) {}

bool MdnsWriter::WriteCharacterString(absl::string_view value) {
  if (value.length() > std::numeric_limits<uint8_t>::max()) {
    return false;
  }
  Cursor cursor(this);
  if (Write<uint8_t>(value.length()) &&
      WriteBytes(value.data(), value.length())) {
    cursor.Commit();
    return true;
  }
  return false;
}

// RFC 1035: https://www.ietf.org/rfc/rfc1035.txt
// See section 4.1.4. Message compression
bool MdnsWriter::WriteDomainName(const DomainName& name) {
  if (name.empty()) {
    return false;
  }

  Cursor cursor(this);
  const std::vector<uint64_t> subhashes = ComputeDomainNameSubhashes(name);
  // Tentative dictionary contains label pointer entries to be added to the
  // compression dictionary after successfully writing the domain name.
  std::unordered_map<uint64_t, uint16_t> tentative_dictionary;
  for (size_t i = 0; i < name.label_count(); ++i) {
    OSP_DCHECK(IsValidDomainLabel(name.Label(i)));
    // We only need to do a look up in the compression dictionary and not in the
    // tentative dictionary. The tentative dictionary cannot possibly contain a
    // valid label pointer as all the entries previously added to it are for
    // names that are longer than the currently processed sub-name.
    auto find_result = dictionary_.find(subhashes[i]);
    if (find_result != dictionary_.end()) {
      if (!Write<uint16_t>(find_result->second)) {
        return false;
      }
      dictionary_.insert(tentative_dictionary.begin(),
                         tentative_dictionary.end());
      cursor.Commit();
      return true;
    }
    // Only add a pointer_label for compression if the offset into the buffer
    // fits into the bits available to store it.
    if (IsValidPointerLabelOffset(current() - begin())) {
      tentative_dictionary.insert(
          std::make_pair(subhashes[i], MakePointerLabel(current() - begin())));
    }
    const std::string& label = name.Label(i);
    if (!Write<uint8_t>(MakeDirectLabel(label.size())) ||
        !WriteBytes(label.data(), label.size())) {
      return false;
    }
  }
  if (!Write<uint8_t>(kLabelTermination)) {
    return false;
  }
  // The probability of a collision is extremely low in this application, as the
  // number of domain names compressed is insignificant in comparison to the
  // hash function image.
  dictionary_.insert(tentative_dictionary.begin(), tentative_dictionary.end());
  cursor.Commit();
  return true;
}

bool MdnsWriter::WriteRawRecordRdata(const RawRecordRdata& rdata) {
  Cursor cursor(this);
  if (Write<uint16_t>(rdata.size()) && WriteBytes(rdata.data(), rdata.size())) {
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsWriter::WriteSrvRecordRdata(const SrvRecordRdata& rdata) {
  Cursor cursor(this);
  // Leave space at the beginning at |rollback_position| to write the record
  // length. Cannot write it upfront, since the exact space taken by the target
  // domain name is not known as it might be compressed.
  if (Skip(sizeof(uint16_t)) && Write<uint16_t>(rdata.priority()) &&
      Write<uint16_t>(rdata.weight()) && Write<uint16_t>(rdata.port()) &&
      WriteDomainName(rdata.target()) &&
      UpdateRecordLength(current(), cursor.origin())) {
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsWriter::WriteARecordRdata(const ARecordRdata& rdata) {
  Cursor cursor(this);
  if (Write<uint16_t>(IPAddress::kV4Size) &&
      WriteIPAddress(rdata.ipv4_address())) {
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsWriter::WriteAAAARecordRdata(const AAAARecordRdata& rdata) {
  Cursor cursor(this);
  if (Write<uint16_t>(IPAddress::kV6Size) &&
      WriteIPAddress(rdata.ipv6_address())) {
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsWriter::WritePtrRecordRdata(const PtrRecordRdata& rdata) {
  Cursor cursor(this);
  // Leave space at the beginning at |rollback_position| to write the record
  // length. Cannot write it upfront, since the exact space taken by the target
  // domain name is not known as it might be compressed.
  if (Skip(sizeof(uint16_t)) && WriteDomainName(rdata.ptr_domain()) &&
      UpdateRecordLength(current(), cursor.origin())) {
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsWriter::WriteTxtRecordRdata(const TxtRecordRdata& rdata) {
  Cursor cursor(this);
  // Leave space at the beginning at |rollback_position| to write the record
  // length. It's cheaper to update it at the end than precompute the length.
  if (!Skip(sizeof(uint16_t))) {
    return false;
  }
  for (const std::string& entry : rdata.texts()) {
    if (!entry.empty() && !WriteCharacterString(entry)) {
      return false;
    }
  }
  // If the delta still equals the space reserved for the record length then
  // no strings were written, an empty collection or all strings are empty.
  if (cursor.delta() == sizeof(uint16_t) && !Write<uint8_t>(kTXTEmptyRdata)) {
    return false;
  }
  if (!UpdateRecordLength(current(), cursor.origin())) {
    return false;
  }
  cursor.Commit();
  return true;
}

bool MdnsWriter::WriteMdnsRecord(const MdnsRecord& record) {
  Cursor cursor(this);
  if (WriteDomainName(record.name()) && Write<uint16_t>(record.type()) &&
      Write<uint16_t>(record.record_class()) && Write<uint32_t>(record.ttl()) &&
      WriteRdata(record.rdata())) {
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsWriter::WriteMdnsQuestion(const MdnsQuestion& question) {
  Cursor cursor(this);
  if (WriteDomainName(question.name()) && Write<uint16_t>(question.type()) &&
      Write<uint16_t>(question.record_class())) {
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsWriter::WriteMdnsMessage(const MdnsMessage& message) {
  Cursor cursor(this);
  Header header;
  header.id = message.id();
  header.flags = message.flags();
  header.question_count = message.questions().size();
  header.answer_count = message.answers().size();
  header.authority_record_count = message.authority_records().size();
  header.additional_record_count = message.additional_records().size();
  if (WriteMdnsMessageHeader(header) && WriteCollection(message.questions()) &&
      WriteCollection(message.answers()) &&
      WriteCollection(message.authority_records()) &&
      WriteCollection(message.additional_records())) {
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsWriter::WriteIPAddress(const IPAddress& address) {
  uint8_t bytes[IPAddress::kV6Size];
  size_t size;
  if (address.IsV6()) {
    address.CopyToV6(bytes);
    size = IPAddress::kV6Size;
  } else {
    address.CopyToV4(bytes);
    size = IPAddress::kV4Size;
  }
  return WriteBytes(bytes, size);
}

bool MdnsWriter::WriteRdata(const Rdata& rdata) {
  class RdataWriter {
   public:
    explicit RdataWriter(MdnsWriter* writer) : writer_(writer) {}
    bool operator()(const RawRecordRdata& value) const {
      return writer_->WriteRawRecordRdata(value);
    }
    bool operator()(const SrvRecordRdata& value) const {
      return writer_->WriteSrvRecordRdata(value);
    }
    bool operator()(const ARecordRdata& value) const {
      return writer_->WriteARecordRdata(value);
    }
    bool operator()(const AAAARecordRdata& value) const {
      return writer_->WriteAAAARecordRdata(value);
    }
    bool operator()(const PtrRecordRdata& value) const {
      return writer_->WritePtrRecordRdata(value);
    }
    bool operator()(const TxtRecordRdata& value) const {
      return writer_->WriteTxtRecordRdata(value);
    }

   private:
    MdnsWriter* writer_;
  };

  RdataWriter rdata_writer(this);
  return absl::visit(rdata_writer, rdata);
}

bool MdnsWriter::WriteMdnsMessageHeader(const Header& header) {
  Cursor cursor(this);
  if (Write<uint16_t>(header.id) && Write<uint16_t>(header.flags) &&
      Write<uint16_t>(header.question_count) &&
      Write<uint16_t>(header.answer_count) &&
      Write<uint16_t>(header.authority_record_count) &&
      Write<uint16_t>(header.additional_record_count)) {
    cursor.Commit();
    return true;
  }
  return false;
}

}  // namespace mdns
}  // namespace cast
