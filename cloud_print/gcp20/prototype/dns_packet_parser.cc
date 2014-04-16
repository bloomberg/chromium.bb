// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/dns_packet_parser.h"

#include "base/big_endian.h"
#include "base/logging.h"

DnsPacketParser::DnsPacketParser(const char* packet, size_t length)
    : packet_(packet),
      length_(length),
      record_parser_(packet, length, sizeof(header_)) {
  base::BigEndianReader reader(packet, length);
  is_header_read_ = reader.ReadU16(&header_.id) &&
                    reader.ReadU16(&header_.flags) &&
                    reader.ReadU16(&header_.qdcount) &&
                    reader.ReadU16(&header_.ancount) &&
                    reader.ReadU16(&header_.nscount) &&
                    reader.ReadU16(&header_.arcount);
}

bool DnsPacketParser::ReadRecord(DnsQueryRecord* out) {
  DCHECK(packet_);
  DnsQueryRecord result;
  size_t consumed = ReadName(&result.qname);
  if (!consumed)
    return false;
  base::BigEndianReader reader(packet_ + GetOffset() + consumed,
                               length_ - (GetOffset() + consumed));
  if (reader.ReadU16(&result.qtype) && reader.ReadU16(&result.qclass) &&
    record_parser_.SkipQuestion()) {  // instead of |cur_ = reader.ptr();|
      *out = result;
      return true;
  }

  return false;
}

