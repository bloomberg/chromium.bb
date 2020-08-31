// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_P2P_PORT_ALLOCATOR_H_
#define CHROME_SERVICES_SHARING_WEBRTC_P2P_PORT_ALLOCATOR_H_

#include <memory>

#include "third_party/webrtc/p2p/client/basic_port_allocator.h"

namespace sharing {

// TODO(crbug.com/1044522): reuse code from blink instead.
class P2PPortAllocator : public cricket::BasicPortAllocator {
 public:
  struct Config {
    // Enable non-proxied UDP-based transport when set to true. When set to
    // false, it effectively disables all UDP traffic until UDP-supporting proxy
    // RETURN is available in the future.
    bool enable_nonproxied_udp = true;

    // Request binding to individual NICs. Whether multiple routes is allowed is
    // subject to the permission check on mic/camera. When specified as false or
    // the permission request is denied, it still uses the default local address
    // to generate a single local candidate. TODO(guoweis): Rename this to
    // |request_multiple_routes|.
    bool enable_multiple_routes = true;

    // Enable exposing the default local address when set to true. This is
    // only in effect when the |enable_multiple_routes| is false or the
    // permission check of mic/camera is denied.
    bool enable_default_local_candidate = true;
  };

  P2PPortAllocator(std::unique_ptr<rtc::NetworkManager> network_manager,
                   rtc::PacketSocketFactory* socket_factory,
                   const Config& config);
  P2PPortAllocator(const P2PPortAllocator&) = delete;
  P2PPortAllocator& operator=(const P2PPortAllocator&) = delete;
  ~P2PPortAllocator() override;

  // Will also initialize the network manager passed into the constructor.
  void Initialize() override;

 private:
  std::unique_ptr<rtc::NetworkManager> network_manager_;
  Config config_;
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_P2P_PORT_ALLOCATOR_H_
