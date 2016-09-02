// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CHROMIUM_MOCK_NETWORK_CHANGE_NOTIFIER_H_
#define NET_QUIC_CHROMIUM_MOCK_NETWORK_CHANGE_NOTIFIER_H_

#include "net/base/network_change_notifier.h"

namespace net {
namespace test {

class MockNetworkChangeNotifier : public NetworkChangeNotifier {
 public:
  MockNetworkChangeNotifier();
  ~MockNetworkChangeNotifier() override;

  ConnectionType GetCurrentConnectionType() const override;

  void ForceNetworkHandlesSupported();

  bool AreNetworkHandlesCurrentlySupported() const override;

  void SetConnectedNetworksList(const NetworkList& network_list);

  void GetCurrentConnectedNetworks(NetworkList* network_list) const override;

  void NotifyNetworkMadeDefault(NetworkChangeNotifier::NetworkHandle network);

  void NotifyNetworkDisconnected(NetworkChangeNotifier::NetworkHandle network);

 private:
  bool force_network_handles_supported_;
  NetworkChangeNotifier::NetworkList connected_networks_;
};

// Class to replace existing NetworkChangeNotifier singleton with a
// MockNetworkChangeNotifier for a test. To use, simply create a
// ScopedMockNetworkChangeNotifier object in the test.
class ScopedMockNetworkChangeNotifier {
 public:
  ScopedMockNetworkChangeNotifier();
  ~ScopedMockNetworkChangeNotifier();

  MockNetworkChangeNotifier* mock_network_change_notifier();

 private:
  std::unique_ptr<NetworkChangeNotifier::DisableForTest>
      disable_network_change_notifier_for_tests_;
  std::unique_ptr<MockNetworkChangeNotifier> mock_network_change_notifier_;
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_CHROMIUM_MOCK_NETWORK_CHANGE_NOTIFIER_H_
