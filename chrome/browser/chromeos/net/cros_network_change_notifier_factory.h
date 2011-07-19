// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_CROS_NETWORK_CHANGE_NOTIFIER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_NET_CROS_NETWORK_CHANGE_NOTIFIER_FACTORY_H_
#pragma once

#include "base/compiler_specific.h"
#include "net/base/network_change_notifier_factory.h"

namespace chromeos {

class NetworkChangeNotifier;

// CrosNetworkChangeNotifierFactory creates ChromeOS-specific specialization of
// NetworkChangeNotifier.
class CrosNetworkChangeNotifierFactory
    : public net::NetworkChangeNotifierFactory {
 public:
  CrosNetworkChangeNotifierFactory() {}

  // Overrides of net::NetworkChangeNotifierFactory.
  virtual net::NetworkChangeNotifier* CreateInstance() OVERRIDE;
};

}  // namespace net

#endif  // CHROME_BROWSER_CHROMEOS_NET_CROS_NETWORK_CHANGE_NOTIFIER_FACTORY_H_
