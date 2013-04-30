// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_CONNECTIVITY_STATE_HELPER_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_NET_CONNECTIVITY_STATE_HELPER_OBSERVER_H_

namespace chromeos {

// A common observer interface used by the ConnectivityStateHelper object to
// relay observing events received from the (older) NetworkLibrary or the (new)
// NetworkStateHandler implementations.
class ConnectivityStateHelperObserver {
 public:
  // Called when there's a change on the connection manager.
  virtual void NetworkManagerChanged() = 0;

  // Called when the default network changes.
  virtual void DefaultNetworkChanged() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_CONNECTIVITY_STATE_HELPER_OBSERVER_H_
