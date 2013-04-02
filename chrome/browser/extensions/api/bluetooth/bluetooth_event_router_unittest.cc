// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"
#include "chrome/test/base/testing_profile.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ExtensionBluetoothEventRouterTest : public testing::Test {
 public:
  ExtensionBluetoothEventRouterTest()
      : mock_adapter_(new testing::StrictMock<device::MockBluetoothAdapter>()),
        router_(&test_profile_) {
    router_.SetAdapterForTest(mock_adapter_);
  }

 protected:
  testing::StrictMock<device::MockBluetoothAdapter>* mock_adapter_;
  TestingProfile test_profile_;
  ExtensionBluetoothEventRouter router_;
};

TEST_F(ExtensionBluetoothEventRouterTest, BluetoothEventListener) {
  router_.OnListenerAdded();
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
  router_.OnListenerRemoved();
}

TEST_F(ExtensionBluetoothEventRouterTest, MultipleBluetoothEventListeners) {
  router_.OnListenerAdded();
  router_.OnListenerAdded();
  router_.OnListenerAdded();
  router_.OnListenerRemoved();
  router_.OnListenerRemoved();
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
  router_.OnListenerRemoved();
}

}  // namespace extensions
