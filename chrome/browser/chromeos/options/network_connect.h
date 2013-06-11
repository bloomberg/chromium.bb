// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONNECT_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONNECT_H_

#include <string>

#include "ui/gfx/native_widget_types.h"  // gfx::NativeWindow

namespace chromeos {
namespace network_connect {

enum ConnectResult {
  NETWORK_NOT_FOUND,
  CONNECT_NOT_STARTED,
  CONNECT_STARTED
};

// Activate the cellular network associated with |service_path| if direct
// activation is supported, otherwise call ShowMobileSetup.
void ActivateCellular(const std::string& service_path);

// Shows the mobile setup dialog which handles:
// * Activation for non direct-activation networks
// * Showing network plan info
void ShowMobileSetup(const std::string& service_path);

// Attempts to connect to the network specified by |service_path|.
// Returns one of the following results:
//  NETWORK_NOT_FOUND if the network does not exist.
//  CONNECT_NOT_STARTED if no connection attempt was started, e.g. because the
//   network is already connected, connecting, or activating.
//  CONNECT_STARTED if a connection attempt was started.
ConnectResult ConnectToNetwork(const std::string& service_path,
                               gfx::NativeWindow parent_window);

// Handle an unconfigured network which might do any of the following:
// * Configure and connect to the network with a matching cert but without
//   pcks11id and tpm pin / slot configured.
// * Show the enrollment dialog for the network.
// * Show the configuration dialog for the network.
// * Show the activation dialog for the network.
// * Show the settings UI for the network.
void HandleUnconfiguredNetwork(const std::string& service_path,
                               gfx::NativeWindow parent_window);

}  // namespace network_connect
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONNECT_H_
