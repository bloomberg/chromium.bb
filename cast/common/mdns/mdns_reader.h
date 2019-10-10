// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_READER_H_
#define CAST_COMMON_MDNS_MDNS_READER_H_

#include "cast/common/mdns/mdns_records.h"
#include "util/big_endian.h"

namespace cast {
namespace mdns {

class MdnsReader : public openscreen::BigEndianReader {
 public:
  using BigEndianReader::BigEndianReader;
  using BigEndianReader::Read;
  using IPAddress = openscreen::IPAddress;

  // The following methods return true if the method was able to successfully
  // read the value to |out| and advances current() to point right past the read
  // data. Returns false if the method failed to read the value to |out|,
  // current() remains unchanged.
  bool Read(absl::string_view* out);
  bool Read(DomainName* out);
  bool Read(RawRecordRdata* out);
  bool Read(SrvRecordRdata* out);
  bool Read(ARecordRdata* out);
  bool Read(AAAARecordRdata* out);
  bool Read(PtrRecordRdata* out);
  bool Read(TxtRecordRdata* out);
  // Reads a DNS resource record with its RDATA.
  // The correct type of RDATA to be read is determined by the type
  // specified in the record.
  bool Read(MdnsRecord* out);
  bool Read(MdnsQuestion* out);
  // Reads multiple mDNS questions and records that are a part of
  // a mDNS message being read.
  bool Read(MdnsMessage* out);

 private:
  bool Read(IPAddress::Version version, IPAddress* out);
  bool Read(DnsType type, Rdata* out);
  bool Read(Header* out);

  template <class ItemType>
  bool Read(uint16_t count, std::vector<ItemType>* out) {
    Cursor cursor(this);
    out->reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
      ItemType entry;
      if (!Read(&entry)) {
        return false;
      }
      out->emplace_back(std::move(entry));
    }
    cursor.Commit();
    return true;
  }

  template <class RdataType>
  bool Read(Rdata* out) {
    RdataType rdata;
    if (Read(&rdata)) {
      *out = std::move(rdata);
      return true;
    }
    return false;
  }
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_READER_H_
