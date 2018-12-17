// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/network_connection_change_simulator.h"

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

NetworkConnectionChangeSimulator::NetworkConnectionChangeSimulator() = default;
NetworkConnectionChangeSimulator::~NetworkConnectionChangeSimulator() = default;

void NetworkConnectionChangeSimulator::SetConnectionType(
    network::mojom::ConnectionType type) {
  network::NetworkConnectionTracker* network_connection_tracker =
      content::GetNetworkConnectionTracker();
  network::mojom::ConnectionType connection_type =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  run_loop_ = std::make_unique<base::RunLoop>();
  network_connection_tracker->AddNetworkConnectionObserver(this);
  SimulateNetworkChange(type);
  // Make sure the underlying network connection type becomes |type|.
  // The while loop is necessary because in some machine such as "Builder
  // linux64 trunk", the |connection_type| can be CONNECTION_ETHERNET before
  // it changes to |type|. So here it needs to wait until the
  // |connection_type| becomes |type|.
  while (
      !network_connection_tracker->GetConnectionType(
          &connection_type,
          base::BindOnce(&NetworkConnectionChangeSimulator::OnConnectionChanged,
                         base::Unretained(this))) ||
      connection_type != type) {
    SimulateNetworkChange(type);
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }
  network_connection_tracker->RemoveNetworkConnectionObserver(this);
}

// static
void NetworkConnectionChangeSimulator::SimulateNetworkChange(
    network::mojom::ConnectionType type) {
  if (base::FeatureList::IsEnabled(network::features::kNetworkService) &&
      !IsNetworkServiceRunningInProcess()) {
    network::mojom::NetworkServiceTestPtr network_service_test;
    ServiceManagerConnection::GetForProcess()->GetConnector()->BindInterface(
        mojom::kNetworkServiceName, &network_service_test);
    base::RunLoop run_loop;
    network_service_test->SimulateNetworkChange(type, run_loop.QuitClosure());
    run_loop.Run();
    return;
  }
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::ConnectionType(type));
}

void NetworkConnectionChangeSimulator::OnConnectionChanged(
    network::mojom::ConnectionType connection_type) {
  DCHECK(run_loop_);
  run_loop_->Quit();
}

}  // namespace content
