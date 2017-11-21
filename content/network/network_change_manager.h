// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_NETWORK_CHANGE_MANAGER_H_
#define CONTENT_NETWORK_NETWORK_CHANGE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/interfaces/network_change_manager.mojom.h"

namespace content {

// Implementation of mojom::NetworkChangeManager. All accesses to this class are
// done through mojo on the main thread. This registers itself to receive
// broadcasts from net::NetworkChangeNotifier and rebroadcasts the notifications
// to network::mojom::NetworkChangeManagerClients through mojo pipes.
class CONTENT_EXPORT NetworkChangeManager
    : public network::mojom::NetworkChangeManager,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // If |network_change_notifier| is not null, |this| will take ownership of it.
  // Otherwise, the global net::NetworkChangeNotifier will be used.
  explicit NetworkChangeManager(
      std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier);

  ~NetworkChangeManager() override;

  // Binds a NetworkChangeManager request to this object. Mojo messages
  // coming through the associated pipe will be served by this object.
  void AddRequest(network::mojom::NetworkChangeManagerRequest request);

  // mojom::NetworkChangeManager implementation:
  void RequestNotifications(
      network::mojom::NetworkChangeManagerClientPtr client_ptr) override;

  size_t GetNumClientsForTesting() const;

 private:
  // Handles connection errors on notification pipes.
  void NotificationPipeBroken(
      network::mojom::NetworkChangeManagerClient* client);

  // net::NetworkChangeNotifier::NetworkChangeObserver implementation:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  mojo::BindingSet<network::mojom::NetworkChangeManager> bindings_;
  std::vector<network::mojom::NetworkChangeManagerClientPtr> clients_;
  network::mojom::ConnectionType connection_type_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeManager);
};

}  // namespace content

#endif  // CONTENT_NETWORK_NETWORK_CHANGE_MANAGER_H_
