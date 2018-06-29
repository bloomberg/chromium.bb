// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_NETWORK_CONNECTION_TRACKER_H_
#define CONTENT_PUBLIC_TEST_MOCK_NETWORK_CONNECTION_TRACKER_H_

#include "content/public/browser/network_connection_tracker.h"

namespace content {

// Allows unit tests to set the network connection type.
// GetConnectionType() can be set to respond synchronously or asynchronously,
// so that it may be tested that tested units are able to correctly handle
// either.
class MockNetworkConnectionTracker : public content::NetworkConnectionTracker {
 public:
  MockNetworkConnectionTracker(bool respond_synchronously,
                               network::mojom::ConnectionType initial_type);
  ~MockNetworkConnectionTracker() override = default;

  bool GetConnectionType(network::mojom::ConnectionType* type,
                         ConnectionTypeCallback callback) override;

  void SetConnectionType(network::mojom::ConnectionType);

 private:
  // Whether GetConnectionType() will respond synchronously.
  bool respond_synchronously_;

  // Keep local copy of the type, for when a synchronous response is requested.
  network::mojom::ConnectionType type_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_NETWORK_CONNECTION_TRACKER_H_
