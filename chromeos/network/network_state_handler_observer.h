// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_OBSERVER_H_
#define CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_OBSERVER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

class NetworkState;

// Observer class for all network state changes, including changes to
// active (connecting or connected) services.
class CHROMEOS_EXPORT NetworkStateHandlerObserver {
 public:
  typedef std::vector<const NetworkState*> NetworkStateList;

  NetworkStateHandlerObserver();
  virtual ~NetworkStateHandlerObserver();

  // Called when one or more network manager properties changes.
  virtual void NetworkManagerChanged();

  // The list of networks changed.
  virtual void NetworkListChanged(const NetworkStateList& networks);

  // The list of devices changed. Typically we don't care about the list
  // of devices, so they are not passed in the method.
  virtual void DeviceListChanged();

  // The active network changed. |network| will be NULL if there is no longer
  // an active network.
  virtual void ActiveNetworkChanged(const NetworkState* network);

  // The state of the active network changed.
  virtual void ActiveNetworkStateChanged(const NetworkState* network);

  // One or more network service properties changed. Note: for the active
  // network, this will be called in *addition* to ActiveNetworkStateChanged()
  // if the state property changed.
  virtual void NetworkServiceChanged(const NetworkState* network);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStateHandlerObserver);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_STATE_HANDLER_OBSERVER_H_
