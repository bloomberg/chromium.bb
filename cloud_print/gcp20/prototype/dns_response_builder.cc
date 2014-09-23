// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/dns_response_builder.h"

#include "base/big_endian.h"
#include "base/logging.h"
#include "net/base/dns_util.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_response.h"

namespace {

uint16 klass = net::dns_protocol::kClassIN;

}  // namespace

DnsResponseRecord::DnsResponseRecord() : type(0), klass(0), ttl(0) {
}

DnsResponseRecord::~DnsResponseRecord() {
}

DnsResponseBuilder::DnsResponseBuilder(uint16 id) {
  header_.id = id;
  // TODO(maksymb): Check do we need AA flag enabled.
  header_.flags = net::dns_protocol::kFlagResponse |
                  net::dns_protocol::kFlagAA;
  header_.qdcount = 0;
  header_.ancount = 0;
  header_.nscount = 0;
  header_.arcount = 0;
}

DnsResponseBuilder::~DnsResponseBuilder() {
}

void DnsResponseBuilder::AppendPtr(const std::string& service_type,
                                   uint32 ttl,
                                   const std::string& service_name,
                                   bool answer) {
  std::string rdata;
  bool success = net::DNSDomainFromDot(service_name, &rdata);
  DCHECK(success);

  AddResponse(service_type, net::dns_protocol::kTypePTR, ttl, rdata, answer);
}

void DnsResponseBuilder::AppendSrv(const std::string& service_name,
                                   uint32 ttl,
                                   uint16 priority,
                                   uint16 weight,
                                   uint16 http_port,
                                   const std::string& service_domain_name,
                                   bool answer) {
  std::string domain_name;
  bool success = net::DNSDomainFromDot(service_domain_name, &domain_name);
  DCHECK(success);

  std::vector<uint8> rdata(2 + 2 + 2 + domain_name.size());

  base::BigEndianWriter writer(reinterpret_cast<char*>(rdata.data()),
                               rdata.size());
  success = writer.WriteU16(priority) &&
            writer.WriteU16(weight) &&
            writer.WriteU16(http_port) &&
            writer.WriteBytes(domain_name.data(), domain_name.size());
  DCHECK(success);
  DCHECK_EQ(writer.remaining(), 0);  // For warranty of correct size allocation.

  AddResponse(service_name, net::dns_protocol::kTypeSRV, ttl,
              std::string(rdata.begin(), rdata.end()), answer);
}

void DnsResponseBuilder::AppendA(const std::string& service_domain_name,
                                 uint32 ttl,
                                 net::IPAddressNumber http_ipv4,
                                 bool answer) {
  // TODO(maksymb): IP to send must depends on interface from where query was
  // received.
  if (http_ipv4.empty()) {
    return;
  }

  AddResponse(service_domain_name, net::dns_protocol::kTypeA, ttl,
              std::string(http_ipv4.begin(), http_ipv4.end()), answer);
}

void DnsResponseBuilder::AppendAAAA(const std::string& service_domain_name,
                                    uint32 ttl,
                                    net::IPAddressNumber http_ipv6,
                                    bool answer) {
  // TODO(maksymb): IP to send must depends on interface from where query was
  // received.
  if (http_ipv6.empty()) {
    return;
  }

  AddResponse(service_domain_name, net::dns_protocol::kTypeAAAA, ttl,
              std::string(http_ipv6.begin(), http_ipv6.end()), answer);
}

void DnsResponseBuilder::AppendTxt(const std::string& service_name,
                                   uint32 ttl,
                                   const std::vector<std::string>& metadata,
                                   bool answer) {
  std::string rdata;
  for (std::vector<std::string>::const_iterator str = metadata.begin();
       str != metadata.end(); ++str) {
    int len = static_cast<int>(str->size());
    DCHECK_LT(len, 256);
    rdata += static_cast<char>(len);  // Set length byte.
    rdata += *str;
  }

  AddResponse(service_name, net::dns_protocol::kTypeTXT, ttl, rdata, answer);
}

scoped_refptr<net::IOBufferWithSize> DnsResponseBuilder::Build() {
  size_t size = sizeof(header_);
  for (std::vector<DnsResponseRecord>::const_iterator iter = responses_.begin();
       iter != responses_.end(); ++iter) {
    size += iter->name.size() + 2 +  // Two dots: first and last.
            sizeof(iter->type) + sizeof(iter->klass) + sizeof(iter->ttl) +
            2 +  // sizeof(RDLENGTH)
            iter->rdata.size();
  }

  if (responses_.empty())
    return NULL;  // No answer.

  DCHECK_EQ(static_cast<size_t>(header_.ancount + header_.arcount),
            responses_.size());
  scoped_refptr<net::IOBufferWithSize> message(
      new net::IOBufferWithSize(static_cast<int>(size)));
  base::BigEndianWriter writer(message->data(), message->size());
  bool success = writer.WriteU16(header_.id) &&
                 writer.WriteU16(header_.flags) &&
                 writer.WriteU16(header_.qdcount) &&
                 writer.WriteU16(header_.ancount) &&
                 writer.WriteU16(header_.nscount) &&
                 writer.WriteU16(header_.arcount);
  DCHECK(success);

  std::string name_in_dns_format;
  for (std::vector<DnsResponseRecord>::const_iterator iter = responses_.begin();
       iter != responses_.end(); ++iter) {
    success = net::DNSDomainFromDot(iter->name, &name_in_dns_format);
    DCHECK(success);
    DCHECK_EQ(name_in_dns_format.size(), iter->name.size() + 2);

    success = writer.WriteBytes(name_in_dns_format.data(),
                                name_in_dns_format.size()) &&
              writer.WriteU16(iter->type) &&
              writer.WriteU16(iter->klass) &&
              writer.WriteU32(iter->ttl) &&
              writer.WriteU16(static_cast<uint16>(iter->rdata.size())) &&
              writer.WriteBytes(iter->rdata.data(), iter->rdata.size());
    DCHECK(success);
  }

  DCHECK_EQ(writer.remaining(), 0);  // For warranty of correct size allocation.

  return message;
}

void DnsResponseBuilder::AddResponse(const std::string& name,
                                     uint16 type,
                                     uint32 ttl,
                                     const std::string& rdata,
                                     bool answer) {
  DnsResponseRecord response;
  response.name = name;
  response.klass = klass;
  response.ttl = ttl;
  response.type = type;
  response.rdata = rdata;

  if (answer) {
    responses_.insert(responses_.begin() + header_.ancount,  response);
    ++header_.ancount;
  } else {
    responses_.push_back(response);
    ++header_.arcount;
  }
}

