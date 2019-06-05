// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_PARSING_H_
#define CAST_COMMON_MDNS_MDNS_PARSING_H_

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/string_view.h"
#include "osp_base/big_endian.h"

namespace cast {
namespace mdns {

bool IsValidDomainLabel(const std::string& label);

// Represents domain name as a collection of labels, ensures label length and
// domain name length requirements are met.
class DomainName {
 public:
  DomainName() = default;
  DomainName(const DomainName& other) = default;
  DomainName(DomainName&& other) = default;
  ~DomainName() = default;

  DomainName& operator=(const DomainName& other) = default;
  DomainName& operator=(DomainName&& other) = default;

  // Clear removes all previously pushed labels and puts DomainName in its
  // initial state.
  void Clear();
  // Appends the given label to the end of the domain name and returns true if
  // the label is a valid domain label and the domain name does not exceed the
  // maximum length. Returns false otherwise.
  bool PushLabel(const std::string& label);
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

  bool operator==(const DomainName& rhs) const;
  bool operator!=(const DomainName& rhs) const;

 private:
  // wire_size_ starts at 1 for the terminating character length.
  size_t max_wire_size_ = 1;
  std::vector<std::string> labels_;
};

std::ostream& operator<<(std::ostream& stream, const DomainName& domain_name);

class MdnsReader : public openscreen::BigEndianReader {
 public:
  MdnsReader(const uint8_t* buffer, size_t length);
  // Returns true if the method was able to successfully read DomainName to
  // |out| and advances current() to point right past the read data. Returns
  // false if the method failed to read DomainName to |out|, current() remains
  // unchanged.
  bool ReadDomainName(DomainName* out);
};

class MdnsWriter : public openscreen::BigEndianWriter {
 public:
  MdnsWriter(uint8_t* buffer, size_t length);
  // Returns true if the method was able to successfully write DomainName |name|
  // to the underlying buffer and advances current() to point right past the
  // written data. Returns false if the method failed to write DomainName |name|
  // to the underlying buffer, current() remains unchanged.
  bool WriteDomainName(const DomainName& name);

 private:
  // Domain name compression dictionary.
  // Maps hashes of previously written domain (sub)names
  // to the label pointers of the first occurences in the underlying buffer.
  // Compression of multiple domain names is supported on the same instance of
  // the MdnsWriter. Underlying buffer may contain other data in addition to the
  // domain names. The compression dictionary persists between calls to
  // WriteDomainName.
  // Label pointer is only 16 bits in size as per RFC 1035. Only lower 14 bits
  // are allocated for storing the offset.
  std::unordered_map<uint64_t, uint16_t> dictionary_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_PARSING_H_
