// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_DNS_RESPONSE_BUILDER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_DNS_RESPONSE_BUILDER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/base/net_util.h"
#include "net/dns/dns_protocol.h"

namespace net {

class IOBufferWithSize;

}  // namespace net

// Record for storing response data.
struct DnsResponseRecord {
  DnsResponseRecord();
  ~DnsResponseRecord();

  std::string name;  // in dotted form
  uint16 type;
  uint16 klass;
  uint32 ttl;
  std::string rdata;
};

// Class for building service-specified responses.
class DnsResponseBuilder {
 public:
  // Initializes builder.
  explicit DnsResponseBuilder(uint16 id);

  // Destroys the object.
  ~DnsResponseBuilder();

  // Methods for appending different types of responses to packet.
  void AppendPtr(const std::string& service_type,
                 uint32 ttl,
                 const std::string& service_name,
                 bool answer);

  void AppendSrv(const std::string& service_name,
                 uint32 ttl,
                 uint16 priority,
                 uint16 weight, uint16 http_port,
                 const std::string& service_domain_name,
                 bool answer);

  void AppendA(const std::string& service_domain_name,
               uint32 ttl,
               net::IPAddressNumber http_ipv4,
               bool answer);

  void AppendAAAA(const std::string& service_domain_name,
                  uint32 ttl,
                  net::IPAddressNumber http_ipv6,
                  bool answer);

  void AppendTxt(const std::string& service_name,
                 uint32 ttl,
                 const std::vector<std::string>& metadata,
                 bool answer);

  // Serializes packet to byte sequence.
  scoped_refptr<net::IOBufferWithSize> Build();

 private:
  // Appends response to packet.
  void AddResponse(const std::string& name,
                   uint16 type,
                   uint32 ttl,
                   const std::string& rdata,
                   bool answer);

  std::vector<DnsResponseRecord> responses_;

  // Header of response package.
  net::dns_protocol::Header header_;

  DISALLOW_COPY_AND_ASSIGN(DnsResponseBuilder);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_DNS_RESPONSE_BUILDER_H_

