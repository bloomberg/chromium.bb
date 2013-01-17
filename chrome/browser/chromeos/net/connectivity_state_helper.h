// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_CONNECTIVITY_STATE_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_NET_CONNECTIVITY_STATE_HELPER_H_

#include "chrome/browser/chromeos/cros/network_library.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

// This class provides an interface for consumers to query the connectivity
// state on Chrome OS. Subclasses must implement the query methods using an
// appropriate source (e.g. NetworkStateHandler).
class ConnectivityStateHelper {
 public:
  virtual ~ConnectivityStateHelper() {}

  // Initializes the state helper singleton to use the default (network state
  // handler) implementation or the network library implementation based
  // on the value of command line flag.
  static void Initialize();
  static void Shutdown();
  static ConnectivityStateHelper* Get();

  // Returns true if there's a network of |type| in connected state.
  virtual bool IsConnectedType(const std::string& type) = 0;

  // Returns true if there's a network of |type| in connecting state.
  virtual bool IsConnectingType(const std::string& type) = 0;

  // Get the name for the primary network of type |type| which is not
  // in non-idle state (i.e. connecting or connected state).
  virtual std::string NetworkNameForType(const std::string& type) = 0;

 protected:
  ConnectivityStateHelper() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_CONNECTIVITY_STATE_HELPER_H_
