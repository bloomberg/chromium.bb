// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_NETWORK_CONTROLLER_CHROMEOS_H_
#define APPS_SHELL_BROWSER_SHELL_NETWORK_CONTROLLER_CHROMEOS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace apps {

// Handles network-related tasks for app_shell on Chrome OS.
class ShellNetworkController : public chromeos::NetworkStateHandlerObserver {
 public:
  // This class must be instantiated after chromeos::DBusThreadManager and
  // destroyed before it.
  ShellNetworkController();
  virtual ~ShellNetworkController();

  // chromeos::NetworkStateHandlerObserver overrides:
  virtual void NetworkListChanged() OVERRIDE;
  virtual void DefaultNetworkChanged(
      const chromeos::NetworkState* state) OVERRIDE;

 private:
  // Controls whether scanning is performed periodically.
  void SetScanningEnabled(bool enabled);

  // Asks shill to scan for networks.
  void RequestScan();

  // If not currently connected or connecting, chooses a wireless network and
  // asks shill to connect to it.
  void ConnectIfUnconnected();

  // Handles a successful or failed connection attempt.
  void HandleConnectionSuccess();
  void HandleConnectionError(
      const std::string& error_name,
      scoped_ptr<base::DictionaryValue> error_data);

  // True when ConnectIfUnconnected() has asked shill to connect but the attempt
  // hasn't succeeded or failed yet. This is tracked to avoid sending duplicate
  // requests before chromeos::NetworkStateHandler has acknowledged the initial
  // connection attempt.
  bool waiting_for_connection_result_;

  // Invokes RequestScan() periodically.
  base::RepeatingTimer<ShellNetworkController> scan_timer_;

  base::WeakPtrFactory<ShellNetworkController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShellNetworkController);
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_NETWORK_CONTROLLER_CHROMEOS_H_
