// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETBIOS_HOST_LOCATOR_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETBIOS_HOST_LOCATOR_H_

#include <list>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/smb_client/discovery/host_locator.h"
#include "chrome/browser/chromeos/smb_client/discovery/netbios_client_interface.h"
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
  using NetBiosClientFactory =
      base::RepeatingCallback<std::unique_ptr<NetBiosClientInterface>()>;

  NetBiosHostLocator(GetInterfacesFunction get_interfaces,
                     NetBiosClientFactory client_factory);

  ~NetBiosHostLocator() override;

  // HostLocator override.
  void FindHosts(FindHostsCallback callback) override;

 private:
  // Returns a list of network interfaces on the device.
  net::NetworkInterfaceList GetNetworkInterfaceList();

  // Finds hosts on |interface| by constructing a NetBiosClient, performing a
  // NetBios Name Request for the interface.
  void FindHostsOnInterface(const net::NetworkInterface& interface);

  // Creates a NetBiosClient using the |client_factory_|.
  std::unique_ptr<NetBiosClientInterface> CreateClient() const;

  // Executes a name request transaction for |broadcast_address| using the most
  // recently added NetBiosClient in |netbios_clients_|.
  void ExecuteNameRequest(const net::IPAddress& broadcast_address);

  // Callback handler for packets received by the |netbios_clients_|.
  void PacketReceived(const std::vector<uint8_t>& packet,
                      uint16_t transaction_id,
                      const net::IPEndPoint& sender_ip);

  bool running_ = false;
  GetInterfacesFunction get_interfaces_;
  NetBiosClientFactory client_factory_;
  FindHostsCallback callback_;
  HostMap results_;
  uint16_t transaction_id_ = 0;
  // |netbios_clients_| is a container for storing NetBios clients that are
  // currently performing a NetBios Name Request so that they do not go out of
  // scope. One NetBiosClient exists for each network interface on the device.
  std::list<std::unique_ptr<NetBiosClientInterface>> netbios_clients_;

  DISALLOW_COPY_AND_ASSIGN(NetBiosHostLocator);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETBIOS_HOST_LOCATOR_H_
