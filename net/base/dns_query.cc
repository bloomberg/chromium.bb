// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/dns_query.h"

#include <string>

#include "base/rand_util.h"
#include "net/base/address_family.h"
#include "net/base/dns_util.h"

namespace net {

namespace {

void PackUint16BE(char buf[2], uint16 v) {
  buf[0] = v >> 8;
  buf[1] = v & 0xff;
}

uint16 QTypeFromAddressFamily(AddressFamily address_family) {
  switch (address_family) {
    case ADDRESS_FAMILY_IPV4:
      return kDNS_A;
    case ADDRESS_FAMILY_IPV6:
      return kDNS_AAAA;
    default:
      NOTREACHED() << "Bad address family";
      return kDNS_A;
  }
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
static const size_t kHeaderLen = arraysize(kHeader);

DnsQuery::DnsQuery(const std::string& hostname,
                   AddressFamily address_family,
                   int port)
    : port_(port),
      id_(0),
      qtype_(QTypeFromAddressFamily(address_family)),
      hostname_(hostname) {
  std::string qname;
  if (!net::DNSDomainFromDot(hostname, &qname))
    return;

  size_t query_size = kHeaderLen + qname.size() +
    sizeof(qtype_) + sizeof(kClassIN);

  io_buffer_ = new IOBufferWithSize(query_size);

  int byte_offset = 0;
  char* buffer_head = io_buffer_->data();
  memcpy(&buffer_head[byte_offset], kHeader, kHeaderLen);
  byte_offset += kHeaderLen;
  memcpy(&buffer_head[byte_offset], &qname[0], qname.size());
  byte_offset += qname.size();
  PackUint16BE(&buffer_head[byte_offset], qtype_);
  byte_offset += sizeof(qtype_);
  PackUint16BE(&buffer_head[byte_offset], kClassIN);

  // Randomize ID, first two bytes.
  id_ = base::RandUint64() & 0xffff;
  PackUint16BE(&buffer_head[0], id_);
}

int DnsQuery::port() const {
  DCHECK(IsValid());
  return port_;
}

uint16 DnsQuery::id() const {
  DCHECK(IsValid());
  return id_;
}

uint16 DnsQuery::qtype() const {
  DCHECK(IsValid());
  return qtype_;
}

AddressFamily DnsQuery::address_family() const {
  DCHECK(IsValid());
  return address_family_;
}

const std::string& DnsQuery::hostname() const {
  DCHECK(IsValid());
  return hostname_;
}

IOBufferWithSize* DnsQuery::io_buffer() const {
  DCHECK(IsValid());
  return io_buffer_.get();
}

}  // namespace net
