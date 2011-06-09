// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/dns_query.h"

#include <string>

#include "base/rand_util.h"
#include "net/base/dns_util.h"

namespace net {

namespace {

void PackUint16BE(char buf[2], uint16 v) {
  buf[0] = v >> 8;
  buf[1] = v & 0xff;
}

uint16 UnpackUint16BE(char buf[2]) {
  return static_cast<uint8>(buf[0]) << 8 | static_cast<uint8>(buf[1]);
}

}  // namespace

// DNS query consists of a 12-byte header followed by a question section.
// For details, see RFC 1035 section 4.1.1.  This header template sets RD
// bit, which directs the name server to pursue query recursively, and sets
// the QDCOUNT to 1, meaning the question section has a single entry.  The
// first two bytes of the header form a 16-bit random query ID to be copied
// in the corresponding reply by the name server -- randomized during
// DnsQuery construction.
static const char kHeader[] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x01,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const size_t kHeaderSize = arraysize(kHeader);

DnsQuery::DnsQuery(const std::string& dns_name, uint16 qtype)
    : dns_name_size_(dns_name.size()) {
  DCHECK(DnsResponseBuffer(reinterpret_cast<const uint8*>(dns_name.c_str()),
                           dns_name.size()).DNSName(NULL));
  DCHECK(qtype == kDNS_A || qtype == kDNS_AAAA);

  io_buffer_ = new IOBufferWithSize(kHeaderSize + question_size());

  int byte_offset = 0;
  char* buffer_head = io_buffer_->data();
  memcpy(&buffer_head[byte_offset], kHeader, kHeaderSize);
  byte_offset += kHeaderSize;
  memcpy(&buffer_head[byte_offset], &dns_name[0], dns_name_size_);
  byte_offset += dns_name_size_;
  PackUint16BE(&buffer_head[byte_offset], qtype);
  byte_offset += sizeof(qtype);
  PackUint16BE(&buffer_head[byte_offset], kClassIN);
  RandomizeId();
}

DnsQuery::DnsQuery(const DnsQuery& rhs) : dns_name_size_(rhs.dns_name_size_) {
  io_buffer_ = new IOBufferWithSize(rhs.io_buffer()->size());
  memcpy(io_buffer_->data(), rhs.io_buffer()->data(), rhs.io_buffer()->size());
  RandomizeId();
}

DnsQuery::~DnsQuery() {
}

uint16 DnsQuery::id() const {
  return UnpackUint16BE(&io_buffer_->data()[0]);
}

uint16 DnsQuery::qtype() const {
  return UnpackUint16BE(&io_buffer_->data()[kHeaderSize + dns_name_size_]);
}

DnsQuery* DnsQuery::CloneWithNewId() const {
  return new DnsQuery(*this);
}

size_t DnsQuery::question_size() const {
  return dns_name_size_       // QNAME
    + sizeof(uint16)          // QTYPE
    + sizeof(uint16);         // QCLASS
}

const char* DnsQuery::question_data() const {
  return &io_buffer_->data()[kHeaderSize];
}

void DnsQuery::RandomizeId() {
  PackUint16BE(&io_buffer_->data()[0], base::RandUint64() & 0xffff);
}

}  // namespace net
