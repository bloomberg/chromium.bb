// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_INTERNAL_SERVICES_H_
#define API_IMPL_INTERNAL_SERVICES_H_

#include <memory>
#include <vector>

#include "api/impl/mdns_platform_service.h"
#include "api/impl/mdns_screen_listener_factory.h"
#include "api/impl/mdns_screen_publisher_factory.h"
#include "api/impl/screen_listener_impl.h"
#include "api/impl/screen_publisher_impl.h"
#include "base/macros.h"
#include "platform/api/event_waiter.h"
#include "platform/api/network_interface.h"
#include "platform/api/socket.h"
#include "platform/base/event_loop.h"

namespace openscreen {

class MdnsResponderService;

// Factory for ScreenListener and ScreenPublisher instances; owns internal
// objects needed to instantiate them such as MdnsResponderService and runs an
// event loop.
// TODO(btolsch): This may be renamed and/or split up once QUIC code lands and
// this use case is more concrete.
class InternalServices {
 public:
  static void RunEventLoopOnce();

  static std::unique_ptr<ScreenListener> CreateListener(
      const MdnsScreenListenerConfig& config,
      ScreenListener::Observer* observer);
  static std::unique_ptr<ScreenPublisher> CreatePublisher(
      const ScreenPublisher::Config& config,
      ScreenPublisher::Observer* observer);

 private:
  static InternalServices* GetInstance();

  class InternalPlatformLinkage final : public MdnsPlatformService {
   public:
    explicit InternalPlatformLinkage(InternalServices* parent);
    ~InternalPlatformLinkage() override;

    std::vector<BoundInterface> RegisterInterfaces(
        const std::vector<int32_t>& interface_index_whitelist) override;
    void DeregisterInterfaces(
        const std::vector<BoundInterface>& registered_interfaces) override;

   private:
    InternalServices* const parent_;
  };

  InternalServices();
  ~InternalServices();

  void EnsureInternalServiceEventWaiterCreated();
  void EnsureMdnsServiceCreated();

  void RegisterMdnsSocket(platform::UdpSocketPtr socket);
  void DeregisterMdnsSocket(platform::UdpSocketPtr socket);

  std::unique_ptr<MdnsResponderService> mdns_service_;
  // TODO(btolsch): To support e.g. both QUIC and mDNS listening for separate
  // sockets, we need to either:
  //  - give them their own individual waiter objects
  //  - remember who registered for what in a wrapper here
  //  - something else...
  // Currently, RegisterMdnsSocket is our hook to do 1 or 2.
  platform::EventWaiterPtr internal_service_waiter_;

  DISALLOW_COPY_AND_ASSIGN(InternalServices);
};

}  // namespace openscreen

#endif  // API_IMPL_INTERNAL_SERVICES_H_
