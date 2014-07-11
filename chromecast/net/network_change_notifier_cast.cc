// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/network_change_notifier_cast.h"

namespace chromecast {

NetworkChangeNotifierCast::NetworkChangeNotifierCast() {
}

NetworkChangeNotifierCast::~NetworkChangeNotifierCast() {
}

net::NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierCast::GetCurrentConnectionType() const {
  return net::NetworkChangeNotifier::CONNECTION_NONE;
}

}  // namespace chromecast
