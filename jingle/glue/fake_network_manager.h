// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_GLUE_FAKE_NETWORK_MANAGER_H_
#define JINGLE_GLUE_FAKE_NETWORK_MANAGER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "net/base/net_util.h"
#include "third_party/libjingle/source/talk/base/network.h"

namespace jingle_glue {

// FakeNetworkManager always returns one interface with the IP address
// specified in the constructor.
class FakeNetworkManager : public talk_base::NetworkManager {
 public:
  FakeNetworkManager(const net::IPAddressNumber& address);
  virtual ~FakeNetworkManager();

 protected:
  // Override from talk_base::NetworkManager.
  virtual bool EnumNetworks(
      bool include_ignored,
      std::vector<talk_base::Network*>* networks) OVERRIDE;

  net::IPAddressNumber address_;
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_FAKE_NETWORK_MANAGER_H_
