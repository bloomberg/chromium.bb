// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_CONNECT_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_CONNECT_H

#include <string>

#include "ash/ash_export.h"
#include "base/strings/string16.h"
#include "ui/gfx/native_widget_types.h"  // gfx::NativeWindow

namespace base {
class DictionaryValue;
}

namespace chromeos {
class NetworkTypePattern;
}

namespace ash {
namespace network_connect {

ASH_EXPORT extern const char kNetworkConnectNotificationId[];
ASH_EXPORT extern const char kNetworkActivateNotificationId[];

ASH_EXPORT extern const char kErrorActivateFailed[];

// Requests a network connection and handles any errors and notifications.
// |parent_window| is used to parent any UI on failure (e.g. for certificate
// enrollment). If NULL, the default window will be used.
ASH_EXPORT void ConnectToNetwork(const std::string& service_path,
                                 gfx::NativeWindow parent_window);

// Enables or disables a network technology. If |technology| refers to cellular
// and the device cannot be enabled due to a SIM lock, this function will
// launch the SIM unlock dialog.
ASH_EXPORT void SetTechnologyEnabled(
    const chromeos::NetworkTypePattern& technology,
    bool enabled_state);

// Requests network activation and handles any errors and notifications.
ASH_EXPORT void ActivateCellular(const std::string& service_path);

// Determines whether or not a network requires a connection to activate or
// setup and either shows a notification or opens the mobile setup dialog.
ASH_EXPORT void ShowMobileSetup(const std::string& service_path);

// Configures a network with a dictionary of Shill properties, then sends a
// connect request. The profile is set according to 'shared' if allowed.
ASH_EXPORT void ConfigureNetworkAndConnect(
    const std::string& service_path,
    const base::DictionaryValue& properties,
    bool shared);

// Requests a new network configuration to be created from a dictionary of
// Shill properties and sends a connect request if the configuration succeeds.
// The profile used is determined by |shared|.
ASH_EXPORT void CreateConfigurationAndConnect(base::DictionaryValue* properties,
                                              bool shared);

// Requests a new network configuration to be created from a dictionary of
// Shill properties. The profile used is determined by |shared|.
ASH_EXPORT void CreateConfiguration(base::DictionaryValue* properties,
                                    bool shared);

// Returns the localized string for shill error string |error|.
ASH_EXPORT base::string16 ErrorString(const std::string& error,
                                      const std::string& service_path);

// Shows the settings for the network specified by |service_path|. If empty,
// or no matching network exists, shows the general internet settings page.
ASH_EXPORT void ShowNetworkSettings(const std::string& service_path);

}  // network_connect
}  // ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_CONNECT_H
