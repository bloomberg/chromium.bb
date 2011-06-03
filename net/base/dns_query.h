// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DNS_QUERY_H_
#define NET_BASE_DNS_QUERY_H_
#pragma once

#include <string>

#include "net/base/address_family.h"
#include "net/base/io_buffer.h"
#include "net/base/net_util.h"

namespace net{

// A class that encapsulates bits and pieces related to DNS request processing.
class DnsQuery {
 public:
  // Constructs an object containing an IOBuffer with raw DNS query string
  // for |hostname| with the given |address_family|; |port| is here due to
  // legacy -- getaddrinfo() takes service name, which is mapped to some
  // port and returns sockaddr_in structures with ports filled in, so do we
  // -- look at DnsResponse::Parse() to see where it is used.

  // Every generated object has a random ID, hence two objects generated
  // with the same set of constructor arguments are generally not equal;
  // there is a 1/2^16 chance of them being equal due to size of |id_|.
  DnsQuery(const std::string& hostname, AddressFamily address_family, int port);
  ~DnsQuery();

  // Returns true if the constructed object was valid.
  bool IsValid() const { return io_buffer_.get() != NULL; }

  // DnsQuery field accessors.  These should only be called on an object
  // for whom |IsValid| is true.
  int port() const;
  uint16 id() const;
  uint16 qtype() const;
  AddressFamily address_family() const;
  const std::string& hostname() const;

  // IOBuffer accessor to be used for writing out the query.
  IOBufferWithSize* io_buffer() const;

 private:
  // Port to be used by corresponding DnsResponse when filling sockaddr_ins
  // to be returned.
  int port_;

  // ID of the query.
  uint16 id_;

  // Type of query, currently, either A or AAAA.
  uint16 qtype_;

  // Address family of the query; used when constructing new object from
  // this one.
  AddressFamily address_family_;

  // Hostname that we are trying to resolve.
  std::string hostname_;

  // Contains query bytes to be consumed by higher level Write() call.
  scoped_refptr<IOBufferWithSize> io_buffer_;

  DISALLOW_COPY_AND_ASSIGN(DnsQuery);
};

}  // namespace net

#endif  // NET_BASE_DNS_QUERY_H_
