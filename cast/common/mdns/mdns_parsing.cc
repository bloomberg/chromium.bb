// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_parsing.h"

#include "absl/hash/hash.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "cast/common/mdns/mdns_constants.h"
#include "platform/api/logging.h"

namespace cast {
namespace mdns {

bool IsValidDomainLabel(const std::string& label) {
  const size_t label_size = label.size();
  return label_size > 0 && label_size <= kMaxLabelLength;
}

void DomainName::Clear() {
  max_wire_size_ = 1;
  labels_.clear();
}

bool DomainName::PushLabel(const std::string& label) {
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

  labels_.push_back(label);

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
  if (labels_.size() != rhs.labels_.size()) {
    return false;
  }
  for (size_t i = 0; i < labels_.size(); ++i) {
    if (!absl::EqualsIgnoreCase(labels_[i], rhs.labels_[i])) {
      return false;
    }
  }
  return true;
}

bool DomainName::operator!=(const DomainName& rhs) const {
  return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& stream, const DomainName& domain_name) {
  return stream << domain_name.ToString();
}

MdnsReader::MdnsReader(const uint8_t* buffer, size_t length)
    : BigEndianReader(buffer, length) {}

// RFC 1035: https://www.ietf.org/rfc/rfc1035.txt
// See section 4.1.4. Message compression
bool MdnsReader::ReadDomainName(DomainName* out) {
  OSP_DCHECK(out);
  out->Clear();
  const uint8_t* position = current();
  // The number of bytes consumed reading from the starting position to either
  // the first label pointer or the final termination byte, including the
  // pointer or the termination byte. This is equal to the actual wire size of
  // the DomainName accounting for compression.
  size_t bytes_consumed = 0;
  // The number of bytes that was processed when reading the DomainName,
  // including all label pointers and direct labels. It is used to detect
  // circular compression. The number of processed bytes cannot be possibly
  // greater than the length of the buffer.
  size_t bytes_processed = 0;

  // If we are pointing before the beginning or past the end of the buffer, we
  // hit a malformed pointer. If we have processed more bytes than there are in
  // the buffer, we are in a circular compression loop.
  while (position >= begin() && position < end() &&
         bytes_processed <= length()) {
    const uint8_t label_type = openscreen::ReadBigEndian<uint8_t>(position);
    if (label_type == kLabelTermination) {
      if (bytes_consumed == 0) {
        bytes_consumed = position + sizeof(uint8_t) - current();
      }
      return Skip(bytes_consumed);
    } else if ((label_type & kLabelMask) == kLabelPointer) {
      if (position + sizeof(uint16_t) > end()) {
        return false;
      }
      const uint16_t label_offset =
          openscreen::ReadBigEndian<uint16_t>(position) & kLabelOffsetMask;
      if (bytes_consumed == 0) {
        bytes_consumed = position + sizeof(uint16_t) - current();
      }
      bytes_processed += sizeof(uint16_t);
      position = begin() + label_offset;
    } else if ((label_type & kLabelMask) == kLabelDirect) {
      const uint8_t label_length = label_type & ~kLabelMask;
      OSP_DCHECK_NE(label_length, 0);
      position += sizeof(uint8_t);
      bytes_processed += sizeof(uint8_t);
      if (position + label_length >= end()) {
        return false;
      }
      if (!out->PushLabel(std::string(reinterpret_cast<const char*>(position),
                                      label_length))) {
        return false;
      }
      bytes_processed += label_length;
      position += label_length;
    } else {
      return false;
    }
  }
  return false;
}

MdnsWriter::MdnsWriter(uint8_t* buffer, size_t length)
    : BigEndianWriter(buffer, length) {}

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

}  // namespace

// RFC 1035: https://www.ietf.org/rfc/rfc1035.txt
// See section 4.1.4. Message compression
bool MdnsWriter::WriteDomainName(const DomainName& name) {
  if (name.empty()) {
    return false;
  }

  uint8_t* const rollback_position = current();
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
        set_current(rollback_position);
        return false;
      }
      dictionary_.insert(tentative_dictionary.begin(),
                         tentative_dictionary.end());
      return true;
    }
    // Only add a pointer_label for compression if the offset into the buffer
    // fits into the bits available to store it.
    if (current() - begin() <= kLabelOffsetMask) {
      const uint16_t pointer_label = (uint16_t(kLabelPointer) << 8) |
                               ((current() - begin()) & kLabelOffsetMask);
      tentative_dictionary.insert(std::make_pair(subhashes[i], pointer_label));
    }
    const std::string& label = name.Label(i);
    const uint8_t direct_label =
        (static_cast<uint8_t>(label.size()) & ~kLabelMask) | kLabelDirect;
    if (!Write<uint8_t>(direct_label) ||
        !WriteBytes(label.data(), label.size())) {
      set_current(rollback_position);
      return false;
    }
  }
  if (!Write<uint8_t>(kLabelTermination)) {
    set_current(rollback_position);
    return false;
  }
  // The probability of a collision is extremely low in this application, as the
  // number of domain names compressed is insignificant in comparison to the
  // hash function image.
  dictionary_.insert(tentative_dictionary.begin(), tentative_dictionary.end());
  return true;
}

}  // namespace mdns
}  // namespace cast
