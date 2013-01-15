// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mobile/mobile_activator.h"

#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

using content::BrowserThread;
using testing::_;
using testing::Eq;
using testing::Return;

namespace {

const char kTestServicePath[] = "/a/service/path";

const size_t kNumOTASPStates = 3;

chromeos::MobileActivator::PlanActivationState kOTASPStates[kNumOTASPStates] = {
  chromeos::MobileActivator::PLAN_ACTIVATION_TRYING_OTASP,
  chromeos::MobileActivator::PLAN_ACTIVATION_INITIATING_ACTIVATION,
  chromeos::MobileActivator::PLAN_ACTIVATION_OTASP,
};

}  // namespace
namespace chromeos {

class TestMobileActivator : public MobileActivator {
 public:
  TestMobileActivator(MockNetworkLibrary* mock_network_library,
                      CellularNetwork* cellular_network)
      : mock_network_library_(mock_network_library),
        cellular_network_(cellular_network) {
    // Provide reasonable defaults for basic things we're usually not testing.
    ON_CALL(*this, DCheckOnThread(_))
        .WillByDefault(Return());
    ON_CALL(*this, FindMatchingCellularNetwork(_))
        .WillByDefault(Return(cellular_network));
    ON_CALL(*this, GetNetworkLibrary())
        .WillByDefault(Return(mock_network_library_));
  }
  virtual ~TestMobileActivator() {}

  MOCK_METHOD3(ChangeState, void(CellularNetwork*,
                                 MobileActivator::PlanActivationState,
                                 const std::string&));
  MOCK_METHOD1(EvaluateCellularNetwork, void(CellularNetwork*));
  MOCK_METHOD1(FindMatchingCellularNetwork, CellularNetwork*(bool));
  MOCK_METHOD0(StartOTASPTimer, void(void));

  void InvokeChangeState(CellularNetwork* network,
                         MobileActivator::PlanActivationState new_state,
                         const std::string& error_description) {
    MobileActivator::ChangeState(network, new_state, error_description);
  }

 private:
  MOCK_CONST_METHOD0(GetNetworkLibrary, NetworkLibrary*(void));
  MOCK_CONST_METHOD1(DCheckOnThread, void(const BrowserThread::ID id));

  MockNetworkLibrary* mock_network_library_;
  CellularNetwork* cellular_network_;

  DISALLOW_COPY_AND_ASSIGN(TestMobileActivator);
};

class MobileActivatorTest : public testing::Test {
 public:
  MobileActivatorTest()
      : network_library_(),
        cellular_network_(string(kTestServicePath)),
        mobile_activator_(&network_library_, &cellular_network_) {}
  virtual ~MobileActivatorTest() {}

 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  void set_activator_state(const MobileActivator::PlanActivationState state) {
    mobile_activator_.state_ = state;
  }
  void set_network_activation_state(ActivationState state) {
    cellular_network_.activation_state_ = state;
  }
  void set_connection_state(ConnectionState state) {
    cellular_network_.state_ = state;
  }

  MockNetworkLibrary network_library_;
  MockCellularNetwork cellular_network_;
  TestMobileActivator mobile_activator_;
 private:
  DISALLOW_COPY_AND_ASSIGN(MobileActivatorTest);
};

TEST_F(MobileActivatorTest, BasicFlowForNewDevices) {
  // In a new device, we aren't connected to Verizon, we start at START
  // because we haven't paid Verizon (ever), and the modem isn't even partially
  // activated.
  std::string error_description;
  set_activator_state(MobileActivator::PLAN_ACTIVATION_START);
  set_connection_state(STATE_IDLE);
  set_network_activation_state(ACTIVATION_STATE_NOT_ACTIVATED);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_INITIATING_ACTIVATION,
            mobile_activator_.PickNextState(&cellular_network_,
                                            &error_description));
  // Now behave as if ChangeState() has initiated an activation.
  set_activator_state(MobileActivator::PLAN_ACTIVATION_INITIATING_ACTIVATION);
  set_network_activation_state(ACTIVATION_STATE_ACTIVATING);
  // We'll sit in this state while we wait for the OTASP to finish.
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_INITIATING_ACTIVATION,
            mobile_activator_.PickNextState(&cellular_network_,
                                            &error_description));
  set_network_activation_state(ACTIVATION_STATE_PARTIALLY_ACTIVATED);
  // We'll sit in this state until we go online as well.
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_INITIATING_ACTIVATION,
            mobile_activator_.PickNextState(&cellular_network_,
                                            &error_description));
  set_connection_state(STATE_PORTAL);
  // After we go online, we go back to START, which acts as a jumping off
  // point for the two types of initial OTASP.
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_START,
            mobile_activator_.PickNextState(&cellular_network_,
                                            &error_description));
  set_activator_state(MobileActivator::PLAN_ACTIVATION_START);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_TRYING_OTASP,
            mobile_activator_.PickNextState(&cellular_network_,
                                            &error_description));
  // Very similar things happen while we're trying OTASP.
  set_activator_state(MobileActivator::PLAN_ACTIVATION_TRYING_OTASP);
  set_network_activation_state(ACTIVATION_STATE_ACTIVATING);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_TRYING_OTASP,
            mobile_activator_.PickNextState(&cellular_network_,
                                            &error_description));
  set_network_activation_state(ACTIVATION_STATE_PARTIALLY_ACTIVATED);
  set_connection_state(STATE_PORTAL);
  // And when we come back online again and aren't activating, load the portal.
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING,
            mobile_activator_.PickNextState(&cellular_network_,
                                            &error_description));
  // The JS drives us through the payment portal.
  set_activator_state(MobileActivator::PLAN_ACTIVATION_SHOWING_PAYMENT);
  // The JS also calls us to signal that the portal is done.  This triggers us
  // to start our final OTASP via the aptly named StartOTASP().
  EXPECT_CALL(network_library_, SignalCellularPlanPayment());
  EXPECT_CALL(mobile_activator_,
              ChangeState(Eq(&cellular_network_),
                          Eq(MobileActivator::PLAN_ACTIVATION_START_OTASP),
                          _));
  EXPECT_CALL(mobile_activator_,
              EvaluateCellularNetwork(Eq(&cellular_network_)));
  mobile_activator_.HandleSetTransactionStatus(true);
  // Evaluate state will defer to PickNextState to select what to do now that
  // we're in START_ACTIVATION.  PickNextState should decide to start a final
  // OTASP.
  set_activator_state(MobileActivator::PLAN_ACTIVATION_START_OTASP);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_OTASP,
            mobile_activator_.PickNextState(&cellular_network_,
                                            &error_description));
  // Similarly to TRYING_OTASP and INITIATING_OTASP above...
  set_activator_state(MobileActivator::PLAN_ACTIVATION_OTASP);
  set_network_activation_state(ACTIVATION_STATE_ACTIVATING);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_OTASP,
            mobile_activator_.PickNextState(&cellular_network_,
                                            &error_description));
  set_network_activation_state(ACTIVATION_STATE_ACTIVATED);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_DONE,
            mobile_activator_.PickNextState(&cellular_network_,
                                            &error_description));
}

TEST_F(MobileActivatorTest, OTASPScheduling) {
  const std::string error;
  for (size_t i = 0; i < kNumOTASPStates; ++i) {
    // When activation works, we start a timer to watch for success.
    EXPECT_CALL(cellular_network_, StartActivation())
        .Times(1)
        .WillOnce(Return(true));
    EXPECT_CALL(mobile_activator_, StartOTASPTimer())
        .Times(1);
    set_activator_state(MobileActivator::PLAN_ACTIVATION_START);
    mobile_activator_.InvokeChangeState(&cellular_network_,
                                        kOTASPStates[i],
                                        error);
    // When activation fails, it's an error, unless we're trying for the final
    // OTASP, in which case we try again via DELAY_OTASP.
    EXPECT_CALL(cellular_network_, StartActivation())
        .Times(1)
        .WillOnce(Return(false));
    if (kOTASPStates[i] == MobileActivator::PLAN_ACTIVATION_OTASP) {
      EXPECT_CALL(mobile_activator_, ChangeState(
          Eq(&cellular_network_),
          Eq(MobileActivator::PLAN_ACTIVATION_DELAY_OTASP),
          _));
    } else {
      EXPECT_CALL(mobile_activator_, ChangeState(
          Eq(&cellular_network_),
          Eq(MobileActivator::PLAN_ACTIVATION_ERROR),
          _));
    }
    set_activator_state(MobileActivator::PLAN_ACTIVATION_START);
    mobile_activator_.InvokeChangeState(&cellular_network_,
                                        kOTASPStates[i],
                                        error);
  }
}

TEST_F(MobileActivatorTest, ReconnectOnDisconnectFromPaymentPortal) {
  // Most states either don't care if we're offline or expect to be offline at
  // some point.  For instance the OTASP states expect to go offline during
  // activation and eventually come back.  There are a few transitions states
  // like START_OTASP and DELAY_OTASP which don't really depend on the state of
  // the modem (offline or online) to work correctly.  A few places however,
  // like when we're displaying the portal care quite a bit about going
  // offline.  Lets test for those cases.
  std::string error_description;
  set_connection_state(STATE_FAILURE);
  set_network_activation_state(ACTIVATION_STATE_PARTIALLY_ACTIVATED);
  set_activator_state(MobileActivator::PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_RECONNECTING,
            mobile_activator_.PickNextState(&cellular_network_,
                                            &error_description));
  set_activator_state(MobileActivator::PLAN_ACTIVATION_SHOWING_PAYMENT);
  EXPECT_EQ(MobileActivator::PLAN_ACTIVATION_RECONNECTING,
            mobile_activator_.PickNextState(&cellular_network_,
                                            &error_description));
}

TEST_F(MobileActivatorTest, StartAtStart) {
  EXPECT_CALL(network_library_,
              AddNetworkManagerObserver(Eq(&mobile_activator_)));
  EXPECT_CALL(network_library_, HasRecentCellularPlanPayment()).
      WillOnce(Return(false));
  EXPECT_CALL(mobile_activator_,
              EvaluateCellularNetwork(Eq(&cellular_network_)));
  mobile_activator_.StartActivation();
  EXPECT_EQ(mobile_activator_.state(), MobileActivator::PLAN_ACTIVATION_START);
}

}  // namespace chromeos
