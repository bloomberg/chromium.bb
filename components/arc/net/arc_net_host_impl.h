// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_
#define COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/arc/common/net.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Private implementation of ArcNetHost.
class ArcNetHostImpl : public KeyedService,
                       public InstanceHolder<mojom::NetInstance>::Observer,
                       public chromeos::NetworkStateHandlerObserver,
                       public mojom::NetHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcNetHostImpl* GetForBrowserContext(content::BrowserContext* context);

  // The constructor will register an Observer with ArcBridgeService.
  ArcNetHostImpl(content::BrowserContext* context,
                 ArcBridgeService* arc_bridge_service);
  ~ArcNetHostImpl() override;

  // ARC -> Chrome calls:

  void GetNetworksDeprecated(
      mojom::GetNetworksRequestType type,
      const GetNetworksDeprecatedCallback& callback) override;

  void GetNetworks(mojom::GetNetworksRequestType type,
                   const GetNetworksCallback& callback) override;

  void GetWifiEnabledState(
      const GetWifiEnabledStateCallback& callback) override;

  void SetWifiEnabledState(
      bool is_enabled,
      const SetWifiEnabledStateCallback& callback) override;

  void StartScan() override;

  void CreateNetwork(mojom::WifiConfigurationPtr cfg,
                     const CreateNetworkCallback& callback) override;

  void ForgetNetwork(const std::string& guid,
                     const ForgetNetworkCallback& callback) override;

  void StartConnect(const std::string& guid,
                    const StartConnectCallback& callback) override;

  void StartDisconnect(const std::string& guid,
                       const StartDisconnectCallback& callback) override;

  // Overriden from chromeos::NetworkStateHandlerObserver.
  void ScanCompleted(const chromeos::DeviceState* /*unused*/) override;
  void OnShuttingDown() override;
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;
  void DeviceListChanged() override;
  void GetDefaultNetwork(const GetDefaultNetworkCallback& callback) override;

  // Overridden from ArcBridgeService::InterfaceObserver<mojom::NetInstance>:
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

 private:
  void DefaultNetworkSuccessCallback(const std::string& service_path,
                                     const base::DictionaryValue& dictionary);

  // Due to a race in Chrome, GetNetworkStateFromGuid() might not know about
  // newly created networks, as that function relies on the completion of a
  // separate GetProperties shill call that completes asynchronously.  So this
  // class keeps a local cache of the path->guid mapping as a fallback.
  // This is sufficient to pass CTS but it might not handle multiple
  // successive Create operations (crbug.com/631646).
  bool GetNetworkPathFromGuid(const std::string& guid, std::string* path);

  void CreateNetworkSuccessCallback(
      const mojom::NetHost::CreateNetworkCallback& mojo_callback,
      const std::string& service_path,
      const std::string& guid);

  void CreateNetworkFailureCallback(
      const mojom::NetHost::CreateNetworkCallback& mojo_callback,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // True if the chrome::NetworkStateHandler is currently being observed for
  // state changes.
  bool observing_network_state_ = false;

  std::string cached_service_path_;
  std::string cached_guid_;

  THREAD_CHECKER(thread_checker_);
  mojo::Binding<mojom::NetHost> binding_;
  base::WeakPtrFactory<ArcNetHostImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcNetHostImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_
