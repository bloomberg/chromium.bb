// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/internal_services.h"

#include <algorithm>

#include "api/impl/mdns_responder_service.h"
#include "discovery/mdns/mdns_responder_adapter_impl.h"
#include "platform/api/error.h"
#include "platform/api/logging.h"
#include "platform/api/socket.h"

namespace openscreen {
namespace {

constexpr char kServiceName[] = "_openscreen";
constexpr char kServiceProtocol[] = "_udp";
const IPAddress kMulticastAddress{224, 0, 0, 251};
const IPAddress kMulticastIPv6Address{
    // ff02::fb
    0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfb,
};
const uint16_t kMulticastListeningPort = 5353;

class MdnsResponderAdapterImplFactory final
    : public MdnsResponderAdapterFactory {
 public:
  MdnsResponderAdapterImplFactory() = default;
  ~MdnsResponderAdapterImplFactory() override = default;

  std::unique_ptr<mdns::MdnsResponderAdapter> Create() override {
    return std::make_unique<mdns::MdnsResponderAdapterImpl>();
  }
};

bool SetUpMulticastSocket(platform::UdpSocketPtr socket,
                          platform::InterfaceIndex ifindex) {
  do {
    const IPAddress broadcast_address =
        IsIPv6Socket(socket) ? kMulticastIPv6Address : kMulticastAddress;
    if (!JoinUdpMulticastGroup(socket, broadcast_address, ifindex)) {
      OSP_LOG_ERROR << "join multicast group failed for interface " << ifindex
                    << ": " << platform::GetLastErrorString();
      break;
    }

    if (!BindUdpSocket(socket, {{}, kMulticastListeningPort}, ifindex)) {
      OSP_LOG_ERROR << "bind failed for interface " << ifindex << ": "
                    << platform::GetLastErrorString();
      break;
    }

    return true;
  } while (false);

  if (platform::GetLastError() == EADDRINUSE) {
    OSP_LOG_ERROR
        << "Is there a mDNS service, such as Bonjour, already running?";
  }
  return false;
}

// Ref-counted singleton instance of InternalServices. This lives only as long
// as there is at least one ScreenListener and/or ScreenPublisher alive.
InternalServices* g_instance = nullptr;
int g_instance_ref_count = 0;

}  // namespace

// static
void InternalServices::RunEventLoopOnce() {
  OSP_CHECK(g_instance) << "No listener or publisher is alive.";
  g_instance->mdns_service_.HandleNewEvents(
      platform::OnePlatformLoopIteration(g_instance->mdns_waiter_));
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
  services->mdns_service_.SetServiceConfig(
      config.hostname, config.service_instance_name,
      config.connection_server_port, config.network_interface_indices,
      {{"fn", config.friendly_name}});
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
    const std::vector<platform::InterfaceIndex>& whitelist) {
  auto addrinfo = platform::GetInterfaceAddresses();
  const bool do_filter_using_whitelist = !whitelist.empty();
  std::vector<platform::InterfaceIndex> index_list;
  for (const auto& interface : addrinfo) {
    OSP_VLOG(1) << "Found interface: " << interface;
    if (do_filter_using_whitelist &&
        std::find(whitelist.begin(), whitelist.end(), interface.info.index) ==
            whitelist.end()) {
      OSP_VLOG(1) << "Ignoring interface not in whitelist: " << interface.info;
      continue;
    }
    if (!interface.addresses.empty())
      index_list.push_back(interface.info.index);
  }

  // Listen on all interfaces
  std::vector<BoundInterface> result;
  for (platform::InterfaceIndex index : index_list) {
    const auto& addr =
        *std::find_if(addrinfo.begin(), addrinfo.end(),
                      [index](const platform::InterfaceAddresses& addr) {
                        return addr.info.index == index;
                      });
    auto* socket = addr.addresses.front().address.IsV4()
                       ? platform::CreateUdpSocketIPv4()
                       : platform::CreateUdpSocketIPv6();
    if (!SetUpMulticastSocket(socket, index)) {
      DestroyUdpSocket(socket);
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

InternalServices::InternalServices()
    : mdns_service_(kServiceName,
                    kServiceProtocol,
                    std::make_unique<MdnsResponderAdapterImplFactory>(),
                    std::make_unique<InternalPlatformLinkage>(this)),
      mdns_waiter_(platform::CreateEventWaiter()) {
  OSP_DCHECK(mdns_waiter_);
}

InternalServices::~InternalServices() {
  DestroyEventWaiter(mdns_waiter_);
}

void InternalServices::RegisterMdnsSocket(platform::UdpSocketPtr socket) {
  platform::WatchUdpSocketReadable(mdns_waiter_, socket);
}

void InternalServices::DeregisterMdnsSocket(platform::UdpSocketPtr socket) {
  platform::StopWatchingUdpSocketReadable(mdns_waiter_, socket);
}

// static
InternalServices* InternalServices::ReferenceSingleton() {
  if (!g_instance) {
    OSP_CHECK_EQ(g_instance_ref_count, 0);
    g_instance = new InternalServices();
  }
  ++g_instance_ref_count;
  return g_instance;
}

// static
void InternalServices::DereferenceSingleton(void* instance) {
  OSP_CHECK_EQ(static_cast<InternalServices*>(instance), g_instance);
  OSP_CHECK_GT(g_instance_ref_count, 0);
  --g_instance_ref_count;
  if (g_instance_ref_count == 0) {
    delete g_instance;
    g_instance = nullptr;
  }
}

}  // namespace openscreen
