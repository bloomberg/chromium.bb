// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/network_change_notifier_factory_cast.h"

#include "base/lazy_instance.h"
#include "chromecast/net/network_change_notifier_cast.h"

namespace chromecast {

namespace {

base::LazyInstance<NetworkChangeNotifierCast> g_network_change_notifier_cast =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

net::NetworkChangeNotifier* NetworkChangeNotifierFactoryCast::CreateInstance() {
  return g_network_change_notifier_cast.Pointer();
}

NetworkChangeNotifierFactoryCast::~NetworkChangeNotifierFactoryCast() {
}

// static
NetworkChangeNotifierCast* NetworkChangeNotifierFactoryCast::GetInstance() {
  return g_network_change_notifier_cast.Pointer();
}

}  // namespace chromecast
