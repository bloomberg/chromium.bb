// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_WRITER_H_
#define CAST_COMMON_MDNS_MDNS_WRITER_H_

#include <unordered_map>

#include "absl/strings/string_view.h"
#include "cast/common/mdns/mdns_records.h"
#include "osp_base/big_endian.h"

namespace cast {
namespace mdns {

class MdnsWriter : public openscreen::BigEndianWriter {
 public:
  MdnsWriter(uint8_t* buffer, size_t length);

  // The following methods return true if the method was able to successfully
  // write the value to the underlying buffer and advances current() to point
  // right past the written data. Returns false if the method failed to write
  // the value to the underlying buffer, current() remains unchanged.
  bool WriteCharacterString(absl::string_view value);
  bool WriteDomainName(const DomainName& name);
  bool WriteRawRecordRdata(const RawRecordRdata& rdata);
  bool WriteSrvRecordRdata(const SrvRecordRdata& rdata);
  bool WriteARecordRdata(const ARecordRdata& rdata);
  bool WriteAAAARecordRdata(const AAAARecordRdata& rdata);
  bool WritePtrRecordRdata(const PtrRecordRdata& rdata);
  bool WriteTxtRecordRdata(const TxtRecordRdata& rdata);

 private:
  bool WriteIPAddress(const IPAddress& address);

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

#endif  // CAST_COMMON_MDNS_MDNS_WRITER_H_
