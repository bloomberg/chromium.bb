// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_CONNECT_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_CONNECT_DELEGATE_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chromeos/network/network_connect.h"

namespace chromeos {

class NetworkStateNotifier;

class NetworkConnectDelegateChromeOS : public NetworkConnect::Delegate {
 public:
  NetworkConnectDelegateChromeOS();
  ~NetworkConnectDelegateChromeOS() override;

  void ShowNetworkConfigure(const std::string& network_id) override;
  void ShowNetworkSettings(const std::string& network_id) override;
  bool ShowEnrollNetwork(const std::string& network_id) override;
  void ShowMobileSimDialog() override;
  void ShowMobileSetupDialog(const std::string& service_path) override;
  void ShowNetworkConnectError(const std::string& error_name,
                               const std::string& network_id) override;
  void ShowMobileActivationError(const std::string& network_id) override;

 private:
  std::unique_ptr<NetworkStateNotifier> network_state_notifier_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectDelegateChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_CONNECT_DELEGATE_CHROMEOS_H_
