// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_ONLINE_MONITOR_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_ONLINE_MONITOR_H_

#include "base/run_loop.h"
#include "base/timer.h"
#include "net/base/network_change_notifier.h"

namespace chromeos {

// Waits for the initial network state to become online or a maximum allowed
// wait time has passed.
// TODO(xiyuan): Get rid of this class when switched to a non-nested loop
// solution for app install.
class NetworkOnlineMonitor
    : public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  explicit NetworkOnlineMonitor(int max_wait_milliseconds);
  virtual ~NetworkOnlineMonitor();

  void Wait();

 private:
  // net::NetworkChangeNotifier::ConnectionTypeObserver overrides:
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  int max_wait_millisconeds_;
  base::RunLoop wait_loop_;
  base::OneShotTimer<base::RunLoop> timer_;

  DISALLOW_COPY_AND_ASSIGN(NetworkOnlineMonitor);
};


}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_ONLINE_MONITOR_H_
