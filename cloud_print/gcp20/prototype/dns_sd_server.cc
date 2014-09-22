// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/dns_sd_server.h"

#include <string.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "cloud_print/gcp20/prototype/dns_packet_parser.h"
#include "cloud_print/gcp20/prototype/dns_response_builder.h"
#include "cloud_print/gcp20/prototype/gcp20_switches.h"
#include "net/base/dns_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/dns/dns_protocol.h"

namespace {

const char kDefaultIpAddressMulticast[] = "224.0.0.251";
const uint16 kDefaultPortMulticast = 5353;

const double kTimeToNextAnnouncement = 0.8;  // relatively to TTL
const int kDnsBufSize = 65537;

const uint16 kSrvPriority = 0;
const uint16 kSrvWeight = 0;

void DoNothingAfterSendToSocket(int /*val*/) {
  NOTREACHED();
  // TODO(maksymb): Delete this function once empty callback for SendTo() method
  // will be allowed.
}

}  // namespace

DnsSdServer::DnsSdServer()
    : recv_buf_(new net::IOBufferWithSize(kDnsBufSize)),
      full_ttl_(0) {
}

DnsSdServer::~DnsSdServer() {
  Shutdown();
}

bool DnsSdServer::Start(const ServiceParameters& serv_params, uint32 full_ttl,
                        const std::vector<std::string>& metadata) {
  if (IsOnline())
    return true;

  if (!CreateSocket())
    return false;

  // Initializing server with parameters from arguments.
  serv_params_ = serv_params;
  full_ttl_ = full_ttl;
  metadata_ = metadata;

  VLOG(0) << "DNS server started";
  LOG(WARNING) << "DNS server does not support probing";

  SendAnnouncement(full_ttl_);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DnsSdServer::OnDatagramReceived, AsWeakPtr()));

  return true;
}

void DnsSdServer::Update() {
  if (!IsOnline())
    return;

  SendAnnouncement(full_ttl_);
}

void DnsSdServer::Shutdown() {
  if (!IsOnline())
    return;

  SendAnnouncement(0);  // TTL is 0
  socket_->Close();
  socket_.reset(NULL);
  VLOG(0) << "DNS server stopped";
}

void DnsSdServer::UpdateMetadata(const std::vector<std::string>& metadata) {
  if (!IsOnline())
    return;

  metadata_ = metadata;

  // TODO(maksymb): If less than 20% of full TTL left before next announcement
  // then send it now.

  uint32 current_ttl = GetCurrentTLL();
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoAnnouncement)) {
    DnsResponseBuilder builder(current_ttl);

    builder.AppendTxt(serv_params_.service_name_, current_ttl, metadata_, true);
    scoped_refptr<net::IOBufferWithSize> buffer(builder.Build());

    DCHECK(buffer.get() != NULL);

    socket_->SendTo(buffer.get(), buffer.get()->size(), multicast_address_,
                    base::Bind(&DoNothingAfterSendToSocket));
  }
}

bool DnsSdServer::CreateSocket() {
  net::IPAddressNumber local_ip_any;
  bool success = net::ParseIPLiteralToNumber("0.0.0.0", &local_ip_any);
  DCHECK(success);

  net::IPAddressNumber multicast_dns_ip_address;
  success = net::ParseIPLiteralToNumber(kDefaultIpAddressMulticast,
                                        &multicast_dns_ip_address);
  DCHECK(success);


  socket_.reset(new net::UDPSocket(net::DatagramSocket::DEFAULT_BIND,
                                   net::RandIntCallback(), NULL,
                                   net::NetLog::Source()));

  net::IPEndPoint local_address = net::IPEndPoint(local_ip_any,
                                                  kDefaultPortMulticast);
  multicast_address_ = net::IPEndPoint(multicast_dns_ip_address,
                                       kDefaultPortMulticast);

  socket_->AllowAddressReuse();

  int status = socket_->Bind(local_address);
  if (status < 0)
    return false;

  socket_->SetMulticastLoopbackMode(false);
  status = socket_->JoinGroup(multicast_dns_ip_address);

  if (status < 0)
    return false;

  DCHECK(socket_->is_connected());

  return true;
}

void DnsSdServer::ProcessMessage(int len, net::IOBufferWithSize* buf) {
  VLOG(1) << "Received new message with length: " << len;

  // Parse the message.
  DnsPacketParser parser(buf->data(), len);

  if (!parser.IsValid())  // Was unable to parse header.
    return;

  // TODO(maksymb): Handle truncated messages.
  if (parser.header().flags & net::dns_protocol::kFlagResponse)  // Not a query.
    return;

  DnsResponseBuilder builder(parser.header().id);

  uint32 current_ttl = GetCurrentTLL();

  DnsQueryRecord query;
  // TODO(maksymb): Check known answers.
  for (int query_idx = 0; query_idx < parser.header().qdcount; ++query_idx) {
    bool success = parser.ReadRecord(&query);
    if (success) {
      ProccessQuery(current_ttl, query, &builder);
    } else {  // if (success)
      VLOG(0) << "Broken package";
      break;
    }
  }

  scoped_refptr<net::IOBufferWithSize> buffer(builder.Build());
  if (buffer.get() == NULL)
    return;  // No answers.

  VLOG(1) << "Current TTL for respond: " << current_ttl;

  bool unicast_respond =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kUnicastRespond);
  socket_->SendTo(buffer.get(), buffer.get()->size(),
                  unicast_respond ? recv_address_ : multicast_address_,
                  base::Bind(&DoNothingAfterSendToSocket));
  VLOG(1) << "Responded to "
      << (unicast_respond ? recv_address_ : multicast_address_).ToString();
}

void DnsSdServer::ProccessQuery(uint32 current_ttl, const DnsQueryRecord& query,
                                DnsResponseBuilder* builder) const {
  std::string log;
  bool responded = false;
  switch (query.qtype) {
    // TODO(maksymb): Add IPv6 support.
    case net::dns_protocol::kTypePTR:
      log = "Processing PTR query";
      if (query.qname == serv_params_.service_type_ ||
          query.qname == serv_params_.secondary_service_type_) {
        builder->AppendPtr(query.qname, current_ttl,
                           serv_params_.service_name_, true);

        if (CommandLine::ForCurrentProcess()->HasSwitch(
                switches::kExtendedResponce)) {
          builder->AppendSrv(serv_params_.service_name_, current_ttl,
                             kSrvPriority, kSrvWeight, serv_params_.http_port_,
                             serv_params_.service_domain_name_, false);
          builder->AppendA(serv_params_.service_domain_name_, current_ttl,
                           serv_params_.http_ipv4_, false);
          builder->AppendAAAA(serv_params_.service_domain_name_, current_ttl,
                              serv_params_.http_ipv6_, false);
          builder->AppendTxt(serv_params_.service_name_, current_ttl, metadata_,
                             false);
        }

        responded = true;
      }

      break;
    case net::dns_protocol::kTypeSRV:
      log = "Processing SRV query";
      if (query.qname == serv_params_.service_name_) {
        builder->AppendSrv(serv_params_.service_name_, current_ttl,
                           kSrvPriority, kSrvWeight, serv_params_.http_port_,
                           serv_params_.service_domain_name_, true);
        responded = true;
      }
      break;
    case net::dns_protocol::kTypeA:
      log = "Processing A query";
      if (query.qname == serv_params_.service_domain_name_) {
        builder->AppendA(serv_params_.service_domain_name_, current_ttl,
                         serv_params_.http_ipv4_, true);
        responded = true;
      }
      break;
    case net::dns_protocol::kTypeAAAA:
      log = "Processing AAAA query";
      if (query.qname == serv_params_.service_domain_name_) {
        builder->AppendAAAA(serv_params_.service_domain_name_, current_ttl,
                            serv_params_.http_ipv6_, true);
        responded = true;
      }
      break;
    case net::dns_protocol::kTypeTXT:
      log = "Processing TXT query";
      if (query.qname == serv_params_.service_name_) {
        builder->AppendTxt(serv_params_.service_name_, current_ttl, metadata_,
                           true);
        responded = true;
      }
      break;
    default:
      base::SStringPrintf(&log, "Unknown query type (%d)", query.qtype);
  }
  log += responded ? ": responded" : ": ignored";
  VLOG(1) << log;
}

void DnsSdServer::DoLoop(int rv) {
  // TODO(maksymb): Check what happened if buffer will be overflowed
  do {
    if (rv > 0)
      ProcessMessage(rv, recv_buf_.get());
    rv = socket_->RecvFrom(recv_buf_.get(), recv_buf_->size(), &recv_address_,
                           base::Bind(&DnsSdServer::DoLoop, AsWeakPtr()));
  } while (rv > 0);

  // TODO(maksymb): Add handler for errors
  DCHECK(rv == net::ERR_IO_PENDING);
}

void DnsSdServer::OnDatagramReceived() {
  DoLoop(0);
}

void DnsSdServer::SendAnnouncement(uint32 ttl) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoAnnouncement)) {
    DnsResponseBuilder builder(ttl);

    builder.AppendPtr(serv_params_.service_type_, ttl,
                     serv_params_.service_name_, true);
    builder.AppendPtr(serv_params_.secondary_service_type_, ttl,
                      serv_params_.service_name_, true);
    builder.AppendSrv(serv_params_.service_name_, ttl, kSrvPriority,
                      kSrvWeight, serv_params_.http_port_,
                      serv_params_.service_domain_name_, true);
    builder.AppendA(serv_params_.service_domain_name_, ttl,
                    serv_params_.http_ipv4_, true);
    builder.AppendAAAA(serv_params_.service_domain_name_, ttl,
                       serv_params_.http_ipv6_, true);
    builder.AppendTxt(serv_params_.service_name_, ttl, metadata_, true);

    scoped_refptr<net::IOBufferWithSize> buffer(builder.Build());

    DCHECK(buffer.get() != NULL);

    socket_->SendTo(buffer.get(), buffer.get()->size(), multicast_address_,
                    base::Bind(&DoNothingAfterSendToSocket));

    VLOG(1) << "Announcement was sent with TTL: " << ttl;
  }

  time_until_live_ = base::Time::Now() +
      base::TimeDelta::FromSeconds(full_ttl_);

  // Schedule next announcement.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DnsSdServer::Update, AsWeakPtr()),
      base::TimeDelta::FromSeconds(static_cast<int64>(
          kTimeToNextAnnouncement*full_ttl_)));
}

uint32 DnsSdServer::GetCurrentTLL() const {
  uint32 current_ttl = (time_until_live_ - base::Time::Now()).InSeconds();
  if (time_until_live_ < base::Time::Now() || current_ttl == 0) {
    // This should not be reachable. But still we don't need to fail.
    current_ttl = 1;  // Service is still alive.
    LOG(ERROR) << "|current_ttl| was equal to zero.";
  }
  return current_ttl;
}
