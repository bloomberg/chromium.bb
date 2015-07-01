// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/proximity_auth_ble_system.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "components/proximity_auth/connection_finder.h"
#include "components/proximity_auth/proximity_auth_client.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace proximity_auth {
namespace {
class MockConnectionFinder : public ConnectionFinder {
 public:
  MockConnectionFinder() {}
  ~MockConnectionFinder() {}

  MOCK_METHOD1(Find, void(const ConnectionCallback&));
};

}  // namespace

class ProximityAuthBleSystemTestable : public ProximityAuthBleSystem {
 public:
  class MockScreenlockBridgeAdapter
      : public ProximityAuthBleSystem::ScreenlockBridgeAdapter {
   public:
    MockScreenlockBridgeAdapter() {}
    ~MockScreenlockBridgeAdapter() {}

    MOCK_METHOD1(AddObserver, void(ScreenlockBridge::Observer*));
    MOCK_METHOD1(RemoveObserver, void(ScreenlockBridge::Observer*));
    MOCK_METHOD1(Unlock, void(ProximityAuthClient*));
  };

  ProximityAuthBleSystemTestable(ScreenlockBridgeAdapter* screenlock_bridge)
      : ProximityAuthBleSystem(make_scoped_ptr(screenlock_bridge), nullptr) {}

  ConnectionFinder* CreateConnectionFinder() override {
    return new NiceMock<MockConnectionFinder>();
  }
};

class ProximityAuthBleSystemTest : public testing::Test {
 protected:
  ProximityAuthBleSystemTest()
      : screenlock_bridge_(new NiceMock<
            ProximityAuthBleSystemTestable::MockScreenlockBridgeAdapter>) {}

  void SetExpectations() {
    EXPECT_CALL(*screenlock_bridge_, AddObserver(_));
    EXPECT_CALL(*screenlock_bridge_, RemoveObserver(_));
  }

  ProximityAuthBleSystemTestable::MockScreenlockBridgeAdapter*
      screenlock_bridge_;
};

TEST_F(ProximityAuthBleSystemTest, ConstructAndDestroyShouldNotCrash) {
  SetExpectations();

  ProximityAuthBleSystemTestable proximity_auth_system(screenlock_bridge_);
}

TEST_F(ProximityAuthBleSystemTest,
       OnScreenDidLock_OnLockScreenEvent_OnScreenDidUnlock) {
  SetExpectations();

  ProximityAuthBleSystemTestable proximity_auth_system(screenlock_bridge_);
  proximity_auth_system.OnScreenDidLock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);
  proximity_auth_system.OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);
}

TEST_F(ProximityAuthBleSystemTest,
       OnScreenDidLock_OnSigninScreenEvent_OnScreenDidUnlock) {
  SetExpectations();

  ProximityAuthBleSystemTestable proximity_auth_system(screenlock_bridge_);
  proximity_auth_system.OnScreenDidLock(
      ScreenlockBridge::LockHandler::SIGNIN_SCREEN);
  proximity_auth_system.OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::SIGNIN_SCREEN);
}

TEST_F(ProximityAuthBleSystemTest,
       OnScreenDidLock_OnOtherScreenEvent_OnScreenDidUnlock) {
  SetExpectations();

  ProximityAuthBleSystemTestable proximity_auth_system(screenlock_bridge_);
  proximity_auth_system.OnScreenDidLock(
      ScreenlockBridge::LockHandler::OTHER_SCREEN);
  proximity_auth_system.OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::OTHER_SCREEN);
}

}  // namespace proximity_auth
