// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NET_MOCK_NETWORK_CHANGE_OBSERVER_H_
#define CHROME_BROWSER_SYNC_NET_MOCK_NETWORK_CHANGE_OBSERVER_H_

#include "base/basictypes.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"

// This class is a mock net::NetworkChangeNotifier::Observer used in
// unit tests.

namespace browser_sync {

class MockNetworkChangeObserver
    : public net::NetworkChangeNotifier::Observer {
 public:
  MockNetworkChangeObserver() {}

  virtual ~MockNetworkChangeObserver() {}

  MOCK_METHOD0(OnIPAddressChanged, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNetworkChangeObserver);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NET_MOCK_NETWORK_CHANGE_OBSERVER_H_
