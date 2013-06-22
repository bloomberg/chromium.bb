// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_DNS_PACKET_PARSER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_DNS_PACKET_PARSER_H_

#include <string>

#include "net/dns/dns_protocol.h"
#include "net/dns/dns_response.h"

// Parsed response record.
struct DnsQueryRecord {
  DnsQueryRecord() : qtype(0), qclass(0) {}
  ~DnsQueryRecord() {}

  std::string qname;  // in dotted form
  uint16 qtype;
  uint16 qclass;
};

// Iterator to walk over records of the DNS response packet. Encapsulates
// |DnsRecordParser| object for using its functionality.
class DnsPacketParser {
 public:
  // Constructs an iterator to process the |packet| of given |length|.
  DnsPacketParser(const char* packet, size_t length);

  // Destroys the object.
  ~DnsPacketParser() {}

  bool IsValid() const { return record_parser_.IsValid() && is_header_read_; }

  // Returns |true| if no more bytes remain in the packet.
  bool AtEnd() const { return record_parser_.AtEnd(); }

  // Returns header of DNS packet.
  const net::dns_protocol::Header& header() const { return header_; }

  // Parses the next query record into |record|. Returns true if succeeded.
  bool ReadRecord(DnsQueryRecord* record);

  // Parses the next resource record into |record|. Returns true if succeeded.
  bool ReadRecord(net::DnsResourceRecord* record) {
    return record_parser_.ReadRecord(record);
  }

 private:
  // Returns current offset into the packet.
  size_t GetOffset() const { return record_parser_.GetOffset(); }

  // Parses a (possibly compressed) DNS name from the packet starting at
  // |pos|. Stores output (even partial) in |out| unless |out| is NULL. |out|
  // is stored in the dotted form, e.g., "example.com". Returns number of bytes
  // consumed or 0 on failure.
  // This is exposed to allow parsing compressed names within RRDATA for TYPEs
  // such as NS, CNAME, PTR, MX, SOA.
  // See RFC 1035 section 4.1.4.
  unsigned ReadName(std::string* out) const {
    return record_parser_.ReadName(packet_ + GetOffset(), out);
  }

  const char* packet_;
  size_t length_;

  // Contents parsed header_;
  net::dns_protocol::Header header_;
  bool is_header_read_;

  // Encapsulated parser.
  net::DnsRecordParser record_parser_;

  DISALLOW_COPY_AND_ASSIGN(DnsPacketParser);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_DNS_PACKET_PARSER_H_

