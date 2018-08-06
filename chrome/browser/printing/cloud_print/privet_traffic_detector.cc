// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_traffic_detector.h"

#include <stddef.h>

#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_byteorder.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/mdns_client.h"

namespace {

const int kMaxRestartAttempts = 10;
const char kPrivetDeviceTypeDnsString[] = "\x07_privet";

void GetNetworkListInBackground(
    const base::Callback<void(const net::NetworkInterfaceList&)> callback) {
  base::AssertBlockingAllowed();
  net::NetworkInterfaceList networks;
  if (!GetNetworkList(&networks, net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES))
    return;

  net::NetworkInterfaceList ip4_networks;
  for (size_t i = 0; i < networks.size(); ++i) {
    net::AddressFamily address_family =
        net::GetAddressFamily(networks[i].address);
    if (address_family == net::ADDRESS_FAMILY_IPV4 &&
        networks[i].prefix_length >= 24) {
      ip4_networks.push_back(networks[i]);
    }
  }

  net::IPAddress localhost_prefix(127, 0, 0, 0);
  ip4_networks.push_back(
      net::NetworkInterface("lo",
                            "lo",
                            0,
                            net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
                            localhost_prefix,
                            8,
                            net::IP_ADDRESS_ATTRIBUTE_NONE));
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                   base::BindOnce(callback, ip4_networks));
}

}  // namespace

namespace cloud_print {

PrivetTrafficDetector::PrivetTrafficDetector(
    net::AddressFamily address_family,
    content::BrowserContext* profile,
    const base::Closure& on_traffic_detected)
    : on_traffic_detected_(on_traffic_detected),
      callback_runner_(base::ThreadTaskRunnerHandle::Get()),
      address_family_(address_family),
      restart_attempts_(kMaxRestartAttempts),
      receiver_binding_(this),
      profile_(profile),
      weak_ptr_factory_(this) {}

PrivetTrafficDetector::~PrivetTrafficDetector() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void PrivetTrafficDetector::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  g_browser_process->network_connection_tracker()->AddNetworkConnectionObserver(
      this);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&PrivetTrafficDetector::ScheduleRestart,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrivetTrafficDetector::Stop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  g_browser_process->network_connection_tracker()
      ->RemoveNetworkConnectionObserver(this);
}

void PrivetTrafficDetector::HandleConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  restart_attempts_ = kMaxRestartAttempts;
  if (type != network::mojom::ConnectionType::CONNECTION_NONE) {
    ScheduleRestart();
  }
}

void PrivetTrafficDetector::ScheduleRestart() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  ResetConnection();
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&GetNetworkListInBackground,
                     base::Bind(&PrivetTrafficDetector::Restart,
                                weak_ptr_factory_.GetWeakPtr())),
      base::TimeDelta::FromSeconds(3));
}

void PrivetTrafficDetector::Restart(const net::NetworkInterfaceList& networks) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  networks_ = networks;
  Bind();
}

void PrivetTrafficDetector::Bind() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!start_time_.is_null()) {
    base::TimeDelta time_delta = base::Time::Now() - start_time_;
    UMA_HISTOGRAM_LONG_TIMES("LocalDiscovery.DetectorRestartTime", time_delta);
  }
  start_time_ = base::Time::Now();

  network::mojom::UDPSocketReceiverPtr receiver_ptr;
  network::mojom::UDPSocketReceiverRequest receiver_request =
      mojo::MakeRequest(&receiver_ptr);
  receiver_binding_.Bind(std::move(receiver_request));
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&PrivetTrafficDetector::CreateUDPSocketOnUIThread, this,
                     mojo::MakeRequest(&socket_), std::move(receiver_ptr)));

  net::IPEndPoint multicast_addr = net::GetMDnsIPEndPoint(address_family_);
  net::IPEndPoint bind_endpoint(
      net::IPAddress::AllZeros(multicast_addr.address().size()),
      multicast_addr.port());

  network::mojom::UDPSocketOptionsPtr socket_options =
      network::mojom::UDPSocketOptions::New();
  socket_options->allow_address_reuse = true;
  socket_options->multicast_loopback_mode = false;

  socket_->Bind(bind_endpoint, std::move(socket_options),
                base::BindOnce(&PrivetTrafficDetector::OnBindComplete, this,
                               multicast_addr));
}

void PrivetTrafficDetector::CreateUDPSocketOnUIThread(
    network::mojom::UDPSocketRequest request,
    network::mojom::UDPSocketReceiverPtr receiver_ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  network::mojom::NetworkContext* network_context =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetNetworkContext();
  network_context->CreateUDPSocket(std::move(request), std::move(receiver_ptr));
}

void PrivetTrafficDetector::OnBindComplete(
    net::IPEndPoint multicast_addr,
    int rv,
    const base::Optional<net::IPEndPoint>& ip_endpoint) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (rv != net::OK) {
    if ((restart_attempts_--) > 0)
      ScheduleRestart();
  } else {
    socket_->JoinGroup(
        multicast_addr.address(),
        base::BindOnce(&PrivetTrafficDetector::OnJoinGroupComplete, this));
  }
}

bool PrivetTrafficDetector::IsSourceAcceptable() const {
  for (size_t i = 0; i < networks_.size(); ++i) {
    if (net::IPAddressMatchesPrefix(recv_addr_.address(), networks_[i].address,
                                    networks_[i].prefix_length)) {
      return true;
    }
  }
  return false;
}

bool PrivetTrafficDetector::IsPrivetPacket(
    base::span<const uint8_t> data) const {
  if (data.size() <= sizeof(net::dns_protocol::Header) ||
      !IsSourceAcceptable()) {
    return false;
  }

  const char* buffer_begin = reinterpret_cast<const char*>(data.data());
  const char* buffer_end = buffer_begin + data.size();
  const net::dns_protocol::Header* header =
      reinterpret_cast<const net::dns_protocol::Header*>(buffer_begin);
  // Check if response packet.
  if (!(header->flags & base::HostToNet16(net::dns_protocol::kFlagResponse)))
    return false;
  const char* substring_begin = kPrivetDeviceTypeDnsString;
  const char* substring_end = substring_begin +
                              arraysize(kPrivetDeviceTypeDnsString) - 1;
  // Check for expected substring, any Privet device must include this.
  return std::search(buffer_begin, buffer_end, substring_begin,
                     substring_end) != buffer_end;
}

void PrivetTrafficDetector::OnJoinGroupComplete(int rv) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (rv != net::OK) {
    if ((restart_attempts_--) > 0)
      ScheduleRestart();
  } else {
    // Reset on success.
    restart_attempts_ = kMaxRestartAttempts;
    socket_->ReceiveMoreWithBufferSize(1, net::dns_protocol::kMaxMulticastSize);
  }
}

void PrivetTrafficDetector::ResetConnection() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  socket_.reset();
  receiver_binding_.Close();
}

void PrivetTrafficDetector::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&PrivetTrafficDetector::HandleConnectionChanged,
                     weak_ptr_factory_.GetWeakPtr(), type));
}

void PrivetTrafficDetector::OnReceived(
    int32_t result,
    const base::Optional<net::IPEndPoint>& src_addr,
    base::Optional<base::span<const uint8_t>> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (result != net::OK)
    return;

  // |data| and |src_addr| are guaranteed to be non-null when |result| is
  // net::OK
  recv_addr_ = src_addr.value();
  if (IsPrivetPacket(data.value())) {
    ResetConnection();
    callback_runner_->PostTask(FROM_HERE, on_traffic_detected_);
    base::TimeDelta time_delta = base::Time::Now() - start_time_;
    UMA_HISTOGRAM_LONG_TIMES("LocalDiscovery.DetectorTriggerTime", time_delta);
  } else {
    socket_->ReceiveMoreWithBufferSize(1, net::dns_protocol::kMaxMulticastSize);
  }
}

}  // namespace cloud_print
