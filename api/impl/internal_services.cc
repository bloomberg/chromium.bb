// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/internal_services.h"

#include <algorithm>

#include "api/impl/mdns_responder_service.h"
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
    return std::make_unique<mdns::MdnsResponderAdapterImpl>();
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

// Ref-counted singleton instance of InternalServices. This lives only as long
// as there is at least one ScreenListener and/or ScreenPublisher alive.
InternalServices* g_instance = nullptr;
int g_instance_ref_count = 0;

}  // namespace

// static
void InternalServices::RunEventLoopOnce() {
  CHECK(g_instance) << "No listener or publisher is alive.";
  g_instance->mdns_service_.HandleNewEvents(
      platform::OnePlatformLoopIteration(g_instance->internal_service_waiter_));
}

// static
std::unique_ptr<ScreenListener> InternalServices::CreateListener(
    const MdnsScreenListenerConfig& config,
    ScreenListener::Observer* observer) {
  auto* services = ReferenceSingleton();
  auto listener =
      std::make_unique<ScreenListenerImpl>(observer, &services->mdns_service_);
  listener->SetDestructionCallback(&InternalServices::DereferenceSingleton,
                                   services);
  return listener;
}

// static
std::unique_ptr<ScreenPublisher> InternalServices::CreatePublisher(
    const ScreenPublisher::Config& config,
    ScreenPublisher::Observer* observer) {
  auto* services = ReferenceSingleton();
  // TODO(btolsch): Hostname and instance should either come from config or
  // platform+generated.
  services->mdns_service_.SetServiceConfig(
      "turtle-deadbeef", "deadbeef", config.connection_server_port,
      config.network_interface_indices, {"fn=" + config.friendly_name});
  auto publisher =
      std::make_unique<ScreenPublisherImpl>(observer, &services->mdns_service_);
  publisher->SetDestructionCallback(&InternalServices::DereferenceSingleton,
                                    services);
  return publisher;
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
    if (!interface.addresses.empty())
      index_list.push_back(interface.info.index);
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
    if (!SetupMulticastSocket(socket, index))
      continue;

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

InternalServices::InternalServices()
    : mdns_service_(kServiceName,
                    kServiceProtocol,
                    std::make_unique<MdnsResponderAdapterImplFactory>(),
                    std::make_unique<InternalPlatformLinkage>(this)),
      internal_service_waiter_(platform::CreateEventWaiter()) {
  DCHECK(internal_service_waiter_);
}

InternalServices::~InternalServices() {
  DestroyEventWaiter(internal_service_waiter_);
}

void InternalServices::RegisterMdnsSocket(platform::UdpSocketPtr socket) {
  platform::WatchUdpSocketReadable(internal_service_waiter_, socket);
}

void InternalServices::DeregisterMdnsSocket(platform::UdpSocketPtr socket) {
  platform::StopWatchingUdpSocketReadable(internal_service_waiter_, socket);
}

// static
InternalServices* InternalServices::ReferenceSingleton() {
  if (!g_instance) {
    CHECK_EQ(g_instance_ref_count, 0);
    g_instance = new InternalServices();
  }
  ++g_instance_ref_count;
  return g_instance;
}

// static
void InternalServices::DereferenceSingleton(void* instance) {
  CHECK_EQ(static_cast<InternalServices*>(instance), g_instance);
  CHECK_GT(g_instance_ref_count, 0);
  --g_instance_ref_count;
  if (g_instance_ref_count == 0) {
    delete g_instance;
    g_instance = nullptr;
  }
}

}  // namespace openscreen
