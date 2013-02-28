// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_online_monitor.h"

namespace chromeos {

NetworkOnlineMonitor::NetworkOnlineMonitor(int max_wait_milliseconds)
    : max_wait_millisconeds_(max_wait_milliseconds) {
}

NetworkOnlineMonitor::~NetworkOnlineMonitor() {}

void NetworkOnlineMonitor::Wait() {
  // Set a maximum allowed wait time.
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(max_wait_millisconeds_),
               &wait_loop_, &base::RunLoop::Quit);

  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  wait_loop_.Run();
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void NetworkOnlineMonitor::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  const bool online = type != net::NetworkChangeNotifier::CONNECTION_NONE;
  if (online)
    wait_loop_.Quit();
}

}  // namespace chromeos
