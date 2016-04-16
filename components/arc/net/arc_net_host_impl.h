// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_
#define COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/net.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcBridgeService;

// Private implementation of ArcNetHost.
class ArcNetHostImpl : public ArcService,
                       public ArcBridgeService::Observer,
                       public chromeos::NetworkStateHandlerObserver,
                       public mojom::NetHost {
 public:
  // The constructor will register an Observer with ArcBridgeService.
  explicit ArcNetHostImpl(ArcBridgeService* arc_bridge_service);
  ~ArcNetHostImpl() override;

  // Called when a GetNetworks call is sent from ARC.
  void GetNetworksDeprecated(
      bool configured_only,
      bool visible_only,
      const GetNetworksDeprecatedCallback& callback) override;

  // Called when a GetNetworks call is sent from ARC.
  void GetNetworks(mojom::GetNetworksRequestType type,
                   const GetNetworksCallback& callback) override;

  // Called when a GetWifiEnabledState call is sent from ARC.
  void GetWifiEnabledState(
      const GetWifiEnabledStateCallback& callback) override;

  // Called when a SetWifiEnabledState call is sent from ARC.
  void SetWifiEnabledState(
      bool is_enabled,
      const SetWifiEnabledStateCallback& callback) override;

  // Called when a StartScan call is sent from ARC.
  void StartScan() override;

  // Overriden from chromeos::NetworkStateHandlerObserver.
  void ScanCompleted(const chromeos::DeviceState* /*unused*/) override;

  // Overriden from chromeos::NetworkStateHandlerObserver.
  void OnShuttingDown() override;

  // Overridden from ArcBridgeService::Observer:
  void OnNetInstanceReady() override;

 private:
  base::ThreadChecker thread_checker_;
  mojo::Binding<arc::mojom::NetHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcNetHostImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_
