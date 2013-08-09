// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONNECT_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONNECT_H_

#include <string>
#include <vector>

#include "chromeos/network/onc/onc_constants.h"
#include "ui/gfx/native_widget_types.h"  // gfx::NativeWindow

namespace base {
class DictionaryValue;
}

namespace chromeos {

class NetworkState;

namespace network_connect {

// Shows the mobile setup dialog which handles:
// * Activation for non direct-activation networks
// * Showing network plan info
void ShowMobileSetup(const std::string& service_path);

// Shows the network settings subpage for |service_path| (or the main
// network settings page if empty).
void ShowNetworkSettings(const std::string& service_path);

// Handle an unconfigured network:
// * Show the Configure dialog for wifi/wimax/VPN
// * Show the Activation, MobileSetup dialog, or settings page for cellular
void HandleUnconfiguredNetwork(const std::string& service_path,
                               gfx::NativeWindow parent_window);

// If the network UIData has a matching enrollment URL, triggers the enrollment
// dialog and returns true.
bool EnrollNetwork(const std::string& service_path,
                   gfx::NativeWindow parent_window);

// Looks up the policy for |network| for the current active user and sets
// |onc_source| accordingly.
const base::DictionaryValue* FindPolicyForActiveUser(
    const NetworkState* network,
    onc::ONCSource* onc_source);

}  // namespace network_connect
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_NETWORK_CONNECT_H_
