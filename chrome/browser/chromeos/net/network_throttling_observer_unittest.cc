// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/net/network_throttling_observer.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/mock_shill_manager_client.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::NiceMock;

namespace chromeos {

namespace test {

class NetworkThrottlingObserverTest : public ::testing::Test {
 public:
  NetworkThrottlingObserverTest() {
    std::unique_ptr<DBusThreadManagerSetter> dbus_setter =
        DBusThreadManager::GetSetterForTesting();
    // Owned by DBusThreadManager, which takes care of cleaning it
    mock_manager_client_ = new NiceMock<MockShillManagerClient>();
    dbus_setter->SetShillManagerClient(
        std::unique_ptr<ShillManagerClient>(mock_manager_client_));
    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    NetworkHandler::Initialize();
    local_state_ = base::MakeUnique<TestingPrefServiceSimple>();
    local_state_->registry()->RegisterDictionaryPref(
        prefs::kNetworkThrottlingEnabled);
    observer_ = base::MakeUnique<NetworkThrottlingObserver>(local_state_.get());
  }

  ~NetworkThrottlingObserverTest() override {
    network_state_handler_->Shutdown();
    observer_.reset();
    local_state_.reset();
    network_state_handler_.reset();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<NetworkThrottlingObserver> observer_;
  MockShillManagerClient* mock_manager_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkThrottlingObserverTest);
};

TEST_F(NetworkThrottlingObserverTest, ThrottlingChangeCallsShill) {
  // Test that a change in the throttling policy value leads to
  // shill_manager_client being called.
  base::DictionaryValue updated_throttling_policy;
  bool enabled = true;
  uint32_t upload_rate = 1200;
  uint32_t download_rate = 2000;
  updated_throttling_policy.SetBoolean("enabled", enabled);
  updated_throttling_policy.SetInteger("upload_rate_kbits", upload_rate);
  updated_throttling_policy.SetInteger("download_rate_kbits", download_rate);
  EXPECT_CALL(
      *mock_manager_client_,
      SetNetworkThrottlingStatus(enabled, upload_rate, download_rate, _, _))
      .Times(1);
  local_state_->Set(prefs::kNetworkThrottlingEnabled,
                    updated_throttling_policy);
  Mock::VerifyAndClearExpectations(mock_manager_client_);

  // Clearing the preference should disable throttling
  EXPECT_CALL(*mock_manager_client_,
              SetNetworkThrottlingStatus(false, 0, 0, _, _))
      .Times(1);
  local_state_->ClearPref(prefs::kNetworkThrottlingEnabled);
}

}  // namespace test
}  // namespace chromeos
