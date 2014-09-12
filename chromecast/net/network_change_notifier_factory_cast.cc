// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/network_change_notifier_factory_cast.h"

#include "chromecast/net/network_change_notifier_cast.h"

namespace chromecast {

net::NetworkChangeNotifier* NetworkChangeNotifierFactoryCast::CreateInstance() {
  // Caller assumes ownership.
  return new NetworkChangeNotifierCast();
}

NetworkChangeNotifierFactoryCast::~NetworkChangeNotifierFactoryCast() {
}

}  // namespace chromecast
