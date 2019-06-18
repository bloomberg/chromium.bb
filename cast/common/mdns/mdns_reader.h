// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_READER_H_
#define CAST_COMMON_MDNS_MDNS_READER_H_

#include "cast/common/mdns/mdns_records.h"
#include "osp_base/big_endian.h"

namespace cast {
namespace mdns {

class MdnsReader : public openscreen::BigEndianReader {
 public:
  MdnsReader(const uint8_t* buffer, size_t length);

  // The following methods return true if the method was able to successfully
  // read the value to |out| and advances current() to point right past the read
  // data. Returns false if the method failed to read the value to |out|,
  // current() remains unchanged.
  bool ReadCharacterString(std::string* out);
  bool ReadDomainName(DomainName* out);
  bool ReadRawRecordRdata(RawRecordRdata* out);
  bool ReadSrvRecordRdata(SrvRecordRdata* out);
  bool ReadARecordRdata(ARecordRdata* out);
  bool ReadAAAARecordRdata(AAAARecordRdata* out);
  bool ReadPtrRecordRdata(PtrRecordRdata* out);
  bool ReadTxtRecordRdata(TxtRecordRdata* out);
  // This method reads a DNS resource record with its RDATA.
  // The correct type of RDATA to be read is determined by the type
  // specified in the record.
  bool ReadMdnsRecord(MdnsRecord* out);
  bool ReadMdnsQuestion(MdnsQuestion* out);

private:
  bool ReadIPAddress(IPAddress::Version version, IPAddress* out);
  bool ReadRdata(uint16_t type, Rdata* out);
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_READER_H_
