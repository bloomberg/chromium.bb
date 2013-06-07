// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/dns_sd_server.h"

#include <string.h>

#include "base/basictypes.h"
#include "net/base/big_endian.h"
#include "net/base/net_util.h"
#include "net/dns/dns_protocol.h"

namespace {

const char* kDefaultIpAddressMulticast = "224.0.0.251";
const uint16 kDefaultPortMulticast = 5353;

// TODO(maksymb): Add possibility to set constants via command line arguments
const uint32 kDefaultTTL = 60*60;  // in seconds

}  // namespace

DnsSdServer::DnsSdServer() : is_online_(false) {
  // Do nothing
}

DnsSdServer::~DnsSdServer() {
  Shutdown();
}

bool DnsSdServer::Start() {
  if (is_online_)
    return true;

  if (!CreateSocket())
    return false;

  LOG(INFO) << "DNS server started";

  SendAnnouncement(kDefaultTTL);

  is_online_ = true;
  return true;
}

void DnsSdServer::Update() {
  if (!is_online_)
    return;

  SendAnnouncement(kDefaultTTL);
}

void DnsSdServer::Shutdown() {
  if (!is_online_)
    return;

  SendAnnouncement(0);  // ttl is 0
  socket_->Close();
  is_online_ = false;
  LOG(INFO) << "DNS server stopped";
}

void DnsSdServer::ProcessMessages() {
  NOTIMPLEMENTED();  // implement this
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
                                   net::RandIntCallback(),
                                   NULL,
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

bool DnsSdServer::CheckPendingQueries() {
  NOTIMPLEMENTED();  // implement this
  return false;
}

void DoNothing(int /*var*/) {
  // Do nothing
}

void DnsSdServer::SendAnnouncement(uint32 ttl) {
  // Create a message with allocated space for header.
  // DNS header is temporary empty.
  scoped_ptr<std::vector<uint8> > message(
    new std::vector<uint8>(sizeof(net::dns_protocol::Header), 0));  // all is 0

  // TODO(maksymb): Create and implement DnsResponse class

  // Preparing for sending
  scoped_refptr<net::IOBufferWithSize> buffer =
    new net::IOBufferWithSize(static_cast<int>(message.get()->size()));
  memcpy(buffer.get()->data(), message.get()->data(), message.get()->size());

  // Create empty callback (we don't need it at all) and send packet
  net::CompletionCallback callback = base::Bind(DoNothing);
  socket_->SendTo(buffer.get(),
                  buffer.get()->size(),
                  multicast_address_,
                  callback);

  LOG(INFO) << "Announcement was sent with TTL: " << ttl;
}

