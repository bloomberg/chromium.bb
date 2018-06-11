// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETBIOS_HOST_LOCATOR_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETBIOS_HOST_LOCATOR_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/smb_client/discovery/host_locator.h"
#include "net/base/network_interfaces.h"

namespace chromeos {
namespace smb_client {

// Calculates the broadcast address of a network interface.
net::IPAddress CalculateBroadcastAddress(
    const net::NetworkInterface& interface);

// Returns true if a network interface should be used for NetBios discovery.
bool ShouldUseInterface(const net::NetworkInterface& interface);

// HostLocator implementation that uses NetBIOS to locate hosts.
class NetBiosHostLocator : public HostLocator,
                           public base::SupportsWeakPtr<NetBiosHostLocator> {
 public:
  using GetInterfacesFunction =
      base::RepeatingCallback<net::NetworkInterfaceList()>;

  explicit NetBiosHostLocator(GetInterfacesFunction get_interfaces);
  ~NetBiosHostLocator() override;

  // HostLocator override.
  void FindHosts(FindHostsCallback callback) override;

 private:
  // Returns a list of network interfaces on the device.
  net::NetworkInterfaceList GetNetworkInterfaceList();

  GetInterfacesFunction get_interfaces_;
  FindHostsCallback callback_;
  HostMap results_;

  DISALLOW_COPY_AND_ASSIGN(NetBiosHostLocator);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETBIOS_HOST_LOCATOR_H_
