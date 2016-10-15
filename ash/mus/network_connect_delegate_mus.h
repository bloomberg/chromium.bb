// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_NETWORK_CONNECT_DELEGATE_MUS_H_
#define ASH_MUS_NETWORK_CONNECT_DELEGATE_MUS_H_

#include "base/macros.h"
#include "ui/chromeos/network/network_connect.h"

namespace ash {
namespace mus {

// Routes requests to show network config UI over the mojom::SystemTrayClient
// interface.
// TODO(mash): Replace ui::NetworkConnect::Delegate with a client interface on
// a mojo NetworkConfig service. http://crbug.com/644355
class NetworkConnectDelegateMus : public ui::NetworkConnect::Delegate {
 public:
  NetworkConnectDelegateMus();
  ~NetworkConnectDelegateMus() override;

  // ui::NetworkConnect::Delegate:
  void ShowNetworkConfigure(const std::string& service_path) override;
  void ShowNetworkSettingsForGuid(const std::string& network_id) override;
  bool ShowEnrollNetwork(const std::string& network_id) override;
  void ShowMobileSimDialog() override;
  void ShowMobileSetupDialog(const std::string& service_path) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConnectDelegateMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_NETWORK_CONNECT_DELEGATE_MUS_H_
