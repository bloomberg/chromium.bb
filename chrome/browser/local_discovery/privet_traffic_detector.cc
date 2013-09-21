// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_traffic_detector.h"

#include "base/sys_byteorder.h"
#include "chrome/browser/local_discovery/privet_constants.h"
#include "net/base/dns_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_response.h"
#include "net/dns/mdns_client.h"
#include "net/udp/datagram_server_socket.h"
#include "net/udp/udp_server_socket.h"

namespace {
const char kPrivetDefaultDev2iceType[] = "\x07_privet\x04_tcp\x05local";
}

namespace local_discovery {

PrivetTrafficDetector::PrivetTrafficDetector(
    net::AddressFamily address_family,
    const base::Closure& on_traffic_detected)
    : on_traffic_detected_(on_traffic_detected),
      callback_runner_(base::MessageLoop::current()->message_loop_proxy()),
      address_family_(address_family),
      recv_addr_(new net::IPEndPoint),
      io_buffer_(
          new net::IOBufferWithSize(net::dns_protocol::kMaxMulticastSize)),
      weak_ptr_factory_(this) {
}

void PrivetTrafficDetector::Start() {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&PrivetTrafficDetector::StartOnIOThread,
                 weak_ptr_factory_.GetWeakPtr()));
}

PrivetTrafficDetector::~PrivetTrafficDetector() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void PrivetTrafficDetector::StartOnIOThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  ScheduleRestart();
}

void PrivetTrafficDetector::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE)
    ScheduleRestart();
}

void PrivetTrafficDetector::ScheduleRestart() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  socket_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PrivetTrafficDetector::Restart,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(1));
}

void PrivetTrafficDetector::Restart() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (Bind() < net::OK || DoLoop(0) < net::OK) {
    ScheduleRestart();
  }
}

int PrivetTrafficDetector::Bind() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  socket_.reset(new net::UDPServerSocket(NULL, net::NetLog::Source()));
  net::IPEndPoint multicast_addr = net::GetMDnsIPEndPoint(address_family_);
  net::IPAddressNumber address_any(multicast_addr.address().size());
  net::IPEndPoint bind_endpoint(address_any, multicast_addr.port());
  socket_->AllowAddressReuse();
  int rv = socket_->Listen(bind_endpoint);
  if (rv < net::OK)
    return rv;
  socket_->SetMulticastLoopbackMode(false);
  return socket_->JoinGroup(multicast_addr.address());
}

int PrivetTrafficDetector::DoLoop(int rv) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  do {
    if (rv > static_cast<int>(sizeof(net::dns_protocol::Header))) {
      const char* buffer_begin = io_buffer_->data();
      const char* buffer_end = buffer_begin + rv;
      const net::dns_protocol::Header* header =
          reinterpret_cast<const net::dns_protocol::Header*>(buffer_begin);
      // Check if it's response packet.
      if (header->flags & base::HostToNet16(net::dns_protocol::kFlagResponse)) {
        const char* substring_begin = kPrivetDefaultDev2iceType;
        const char* substring_end = substring_begin +
                                    arraysize(kPrivetDefaultDev2iceType);
        // Check for expected substring, any Privet device must include this.
        if (std::search(buffer_begin, buffer_end,
                        substring_begin, substring_end) != buffer_end) {
          socket_.reset();
          callback_runner_->PostTask(FROM_HERE, on_traffic_detected_);
          return net::OK;
        }
      }
    }

    rv = socket_->RecvFrom(
        io_buffer_,
        io_buffer_->size(),
        recv_addr_.get(),
        base::Bind(base::IgnoreResult(&PrivetTrafficDetector::DoLoop),
                   base::Unretained(this)));
  } while (rv > 0);

  if (rv != net::ERR_IO_PENDING)
    return rv;

  return net::OK;
}

}  // namespace local_discovery
