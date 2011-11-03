// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/net/cros_network_change_notifier_factory.h"

#include "chrome/browser/chromeos/net/network_change_notifier_chromeos.h"

namespace chromeos {

namespace {

NetworkChangeNotifierChromeos* g_network_change_notifier = NULL;

}  // namespace

net::NetworkChangeNotifier* CrosNetworkChangeNotifierFactory::CreateInstance() {
  DCHECK(!g_network_change_notifier);
  g_network_change_notifier = new NetworkChangeNotifierChromeos();
  return g_network_change_notifier;
}

// static
NetworkChangeNotifierChromeos* CrosNetworkChangeNotifierFactory::GetInstance() {
  return g_network_change_notifier;
}

}  // namespace net
