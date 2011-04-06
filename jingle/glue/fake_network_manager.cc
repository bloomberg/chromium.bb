// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/fake_network_manager.h"

#include "base/logging.h"
#include "net/base/ip_endpoint.h"
#include "jingle/glue/utils.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"

namespace jingle_glue {

FakeNetworkManager::FakeNetworkManager(const net::IPAddressNumber& address)
    : address_(address) {
}

FakeNetworkManager::~FakeNetworkManager() {
}

bool FakeNetworkManager::EnumNetworks(
    bool include_ignored, std::vector<talk_base::Network*>* networks) {
  net::IPEndPoint endpoint(address_, 0);
  talk_base::SocketAddress address;
  CHECK(IPEndPointToSocketAddress(endpoint, &address));
  networks->push_back(new talk_base::Network(
      "fake", "Fake Network", address.ip(), 0));
  return true;
}

}  // namespace jingle_glue
