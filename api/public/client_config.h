// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_CLIENT_CONFIG_H_
#define API_PUBLIC_CLIENT_CONFIG_H_

#include <cstdint>
#include <vector>

#include "platform/api/network_interface.h"

namespace openscreen {

struct ClientConfig {
  ClientConfig();
  ~ClientConfig();

  // The indexes of network interfaces that should be used by the Open Screen
  // Library.  The indexes derive from the values of
  // openscreen::platform::InterfaceInfo::index.
  std::vector<platform::InterfaceIndex> interface_indexes;
};

}  // namespace openscreen

#endif  // API_PUBLIC_CLIENT_CONFIG_H_
