// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_PROXY_CONFIG_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_NET_PROXY_CONFIG_HANDLER_H_

#include "base/memory/scoped_ptr.h"

class ProxyConfigDictionary;

namespace chromeos {

class NetworkState;

namespace proxy_config {

scoped_ptr<ProxyConfigDictionary> GetProxyConfigForNetwork(
    const NetworkState& network);

void SetProxyConfigForNetwork(const ProxyConfigDictionary& proxy_config,
                              const NetworkState& network);

}  // namespace proxy_config

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_PROXY_CONFIG_HANDLER_H_
