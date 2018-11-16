// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/message_loop/message_loop.h"
#include "chromeos/chromeos_pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/network/fast_transition_observer.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace test {

class FastTransitionObserverTest : public ::testing::Test {
 public:
  FastTransitionObserverTest() {
    DBusThreadManager::Initialize();
    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    NetworkHandler::Initialize();
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    local_state_->registry()->RegisterDictionaryPref(
        prefs::kDeviceWiFiFastTransitionEnabled);
    observer_ = std::make_unique<FastTransitionObserver>(local_state_.get());
  }

  ~FastTransitionObserverTest() override {
    network_state_handler_->Shutdown();
    observer_.reset();
    local_state_.reset();
    network_state_handler_.reset();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

  TestingPrefServiceSimple* local_state() { return local_state_.get(); }

  bool GetFastTransitionStatus() {
    return DBusThreadManager::Get()
        ->GetShillManagerClient()
        ->GetTestInterface()
        ->GetFastTransitionStatus();
  }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<FastTransitionObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(FastTransitionObserverTest);
};

TEST_F(FastTransitionObserverTest, FastTransitionChangeCallsShill) {
  // Test that a change in the Fast Transition policy value leads to
  // shill_manager_client being called.
  base::DictionaryValue updated_fast_transition_policy;
  constexpr bool enabled = true;
  updated_fast_transition_policy.SetBoolean("enabled", enabled);

  // Make sure Fast Transition is disabled just before setting preference.
  EXPECT_FALSE(GetFastTransitionStatus());

  // Setting the preference should update the Fast Transition policy.
  local_state()->Set(prefs::kDeviceWiFiFastTransitionEnabled,
                     updated_fast_transition_policy);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetFastTransitionStatus());

  // Clearing the preference should disable Fast Transition
  local_state()->ClearPref(prefs::kDeviceWiFiFastTransitionEnabled);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetFastTransitionStatus());
}

}  // namespace test
}  // namespace chromeos
