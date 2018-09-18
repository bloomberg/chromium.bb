// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/internal_services.h"

#include <algorithm>

#include "api/impl/mdns_responder_service.h"
#include "base/make_unique.h"
#include "discovery/mdns/mdns_responder_adapter_impl.h"
#include "platform/api/error.h"
#include "platform/api/socket.h"

namespace openscreen {
namespace {

constexpr char kServiceName[] = "_openscreen";
constexpr char kServiceProtocol[] = "_udp";
const IPAddress kMulticastAddress{224, 0, 0, 251};
const IPEndpoint kMulticastListeningEndpoint{IPAddress{0, 0, 0, 0}, 5353};

class MdnsResponderAdapterImplFactory final
    : public MdnsResponderAdapterFactory {
 public:
  MdnsResponderAdapterImplFactory() = default;
  ~MdnsResponderAdapterImplFactory() override = default;

  std::unique_ptr<mdns::MdnsResponderAdapter> Create() override {
    return MakeUnique<mdns::MdnsResponderAdapterImpl>();
  }
};

bool SetupMulticastSocket(platform::UdpSocketPtr socket, int32_t ifindex) {
  if (!JoinUdpMulticastGroup(socket, kMulticastAddress, ifindex)) {
    LOG_ERROR << "join multicast group failed: "
              << platform::GetLastErrorString();
    DestroyUdpSocket(socket);
    return false;
  }
  if (!BindUdpSocket(socket, kMulticastListeningEndpoint, ifindex)) {
    LOG_ERROR << "bind failed: " << platform::GetLastErrorString();
    DestroyUdpSocket(socket);
    return false;
  }

  return true;
}

}  // namespace

// static
void InternalServices::RunEventLoopOnce() {
  auto* services = GetInstance();
  services->mdns_service_->HandleNewEvents(
      platform::OnePlatformLoopIteration(services->internal_service_waiter_));
}

// static
std::unique_ptr<ScreenListener> InternalServices::CreateListener(
    const MdnsScreenListenerConfig& config,
    ScreenListener::Observer* observer) {
  auto* services = GetInstance();
  services->EnsureMdnsServiceCreated();
  auto listener =
      MakeUnique<ScreenListenerImpl>(observer, services->mdns_service_.get());
  return listener;
}

// static
std::unique_ptr<ScreenPublisher> InternalServices::CreatePublisher(
    const ScreenPublisher::Config& config,
    ScreenPublisher::Observer* observer) {
  auto* services = GetInstance();
  services->EnsureMdnsServiceCreated();
  // TODO(btolsch): Hostname and instance should either come from config or
  // platform+generated.
  services->mdns_service_->SetServiceConfig(
      "turtle-deadbeef", "deadbeef", config.connection_server_port,
      config.network_interface_indices, {"fn=" + config.friendly_name});
  auto publisher =
      MakeUnique<ScreenPublisherImpl>(observer, services->mdns_service_.get());
  return publisher;
}

// static
InternalServices* InternalServices::GetInstance() {
  static InternalServices services;
  return &services;
}

InternalServices::InternalPlatformLinkage::InternalPlatformLinkage(
    InternalServices* parent)
    : parent_(parent) {}
InternalServices::InternalPlatformLinkage::~InternalPlatformLinkage() = default;

std::vector<MdnsPlatformService::BoundInterface>
InternalServices::InternalPlatformLinkage::RegisterInterfaces(
    const std::vector<int32_t>& interface_index_whitelist) {
  auto addrinfo = platform::GetInterfaceAddresses();
  std::vector<int> index_list;
  for (const auto& interface : addrinfo) {
    if (!interface_index_whitelist.empty() &&
        std::find(interface_index_whitelist.begin(),
                  interface_index_whitelist.end(),
                  interface.info.index) == interface_index_whitelist.end()) {
      continue;
    }
    if (!interface.addresses.empty()) {
      index_list.push_back(interface.info.index);
    }
  }

  // Listen on all interfaces
  std::vector<BoundInterface> result;
  for (int index : index_list) {
    const auto& addr =
        *std::find_if(addrinfo.begin(), addrinfo.end(),
                      [index](const platform::InterfaceAddresses& addr) {
                        return addr.info.index == index;
                      });
    auto* socket = addr.addresses.front().address.IsV4()
                       ? platform::CreateUdpSocketIPv4()
                       : platform::CreateUdpSocketIPv6();
    if (!SetupMulticastSocket(socket, index)) {
      continue;
    }
    // Pick any address for the given interface.
    result.emplace_back(addr.info, addr.addresses.front(), socket);
  }

  for (auto& interface : result) {
    parent_->RegisterMdnsSocket(interface.socket);
  }
  return result;
}

void InternalServices::InternalPlatformLinkage::DeregisterInterfaces(
    const std::vector<BoundInterface>& registered_interfaces) {
  for (const auto& interface : registered_interfaces) {
    parent_->DeregisterMdnsSocket(interface.socket);
    platform::DestroyUdpSocket(interface.socket);
  }
}

InternalServices::InternalServices() = default;
InternalServices::~InternalServices() = default;

void InternalServices::EnsureInternalServiceEventWaiterCreated() {
  if (internal_service_waiter_) {
    return;
  }
  internal_service_waiter_ = platform::CreateEventWaiter();
}

void InternalServices::EnsureMdnsServiceCreated() {
  EnsureInternalServiceEventWaiterCreated();
  if (mdns_service_) {
    return;
  }
  auto mdns_responder_factory = MakeUnique<MdnsResponderAdapterImplFactory>();
  auto platform = MakeUnique<InternalPlatformLinkage>(this);
  mdns_service_ = MakeUnique<MdnsResponderService>(
      kServiceName, kServiceProtocol, std::move(mdns_responder_factory),
      std::move(platform));
}

void InternalServices::RegisterMdnsSocket(platform::UdpSocketPtr socket) {
  platform::WatchUdpSocketReadable(internal_service_waiter_, socket);
}

void InternalServices::DeregisterMdnsSocket(platform::UdpSocketPtr socket) {
  platform::StopWatchingUdpSocketReadable(internal_service_waiter_, socket);
}

}  // namespace openscreen
