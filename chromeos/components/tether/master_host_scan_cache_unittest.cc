// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/master_host_scan_cache.h"

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/components/tether/fake_host_scan_cache.h"
#include "chromeos/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/components/tether/timer_factory.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Invoke;

namespace chromeos {

namespace tether {

namespace {

const char kTetherGuid0[] = "kTetherGuid0";
const char kTetherGuid1[] = "kTetherGuid1";
const char kTetherGuid2[] = "kTetherGuid2";
const char kTetherGuid3[] = "kTetherGuid3";

const char kTetherDeviceName0[] = "kDeviceName0";
const char kTetherDeviceName1[] = "kDeviceName1";
const char kTetherDeviceName2[] = "kDeviceName2";
const char kTetherDeviceName3[] = "kDeviceName3";

const char kTetherCarrier0[] = "kTetherCarrier0";
const char kTetherCarrier1[] = "kTetherCarrier1";
const char kTetherCarrier2[] = "kTetherCarrier2";
const char kTetherCarrier3[] = "kTetherCarrier3";

const int kTetherBatteryPercentage0 = 20;
const int kTetherBatteryPercentage1 = 40;
const int kTetherBatteryPercentage2 = 60;
const int kTetherBatteryPercentage3 = 80;

const int kTetherSignalStrength0 = 25;
const int kTetherSignalStrength1 = 50;
const int kTetherSignalStrength2 = 75;
const int kTetherSignalStrength3 = 100;

const bool kTetherSetupRequired0 = true;
const bool kTetherSetupRequired1 = false;
const bool kTetherSetupRequired2 = true;
const bool kTetherSetupRequired3 = false;

// MockTimer which invokes a callback in its destructor.
class ExtendedMockTimer : public base::MockTimer {
 public:
  explicit ExtendedMockTimer(const base::Closure& destructor_callback)
      : base::MockTimer(true /* retain_user_task */, false /* is_repeating */),
        destructor_callback_(destructor_callback) {}

  ~ExtendedMockTimer() override { destructor_callback_.Run(); }

 private:
  base::Closure destructor_callback_;
};

class TestTimerFactory : public TimerFactory {
 public:
  TestTimerFactory() {}
  ~TestTimerFactory() override {}

  std::unordered_map<std::string, ExtendedMockTimer*>&
  tether_network_guid_to_timer_map() {
    return tether_network_guid_to_timer_map_;
  }

  void set_tether_network_guid_for_next_timer(
      const std::string& tether_network_guid_for_next_timer) {
    tether_network_guid_for_next_timer_ = tether_network_guid_for_next_timer;
  }

  // TimerFactory:
  std::unique_ptr<base::Timer> CreateOneShotTimer() override {
    EXPECT_FALSE(tether_network_guid_for_next_timer_.empty());
    ExtendedMockTimer* mock_timer = new ExtendedMockTimer(base::Bind(
        &TestTimerFactory::OnActiveTimerDestructor, base::Unretained(this),
        tether_network_guid_for_next_timer_));
    tether_network_guid_to_timer_map_[tether_network_guid_for_next_timer_] =
        mock_timer;
    return base::WrapUnique(mock_timer);
  }

 private:
  void OnActiveTimerDestructor(const std::string& tether_network_guid) {
    tether_network_guid_to_timer_map_.erase(
        tether_network_guid_to_timer_map_.find(tether_network_guid));
  }

  std::string tether_network_guid_for_next_timer_;
  std::unordered_map<std::string, ExtendedMockTimer*>
      tether_network_guid_to_timer_map_;
};

}  // namespace

// TODO(khorimoto): The test uses a FakeHostScanCache to keep an in-memory
// cache of expected values. This has the potential to be confusing, since this
// is the test for MasterHostScanCache. Clean this up to avoid using
// FakeHostScanCache if possible.
class MasterHostScanCacheTest : public NetworkStateTest {
 protected:
  MasterHostScanCacheTest() {}

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();
    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TECHNOLOGY_ENABLED);

    test_timer_factory_ = new TestTimerFactory();
    fake_active_host_ = base::MakeUnique<FakeActiveHost>();
    mock_tether_host_response_recorder_ =
        base::MakeUnique<NiceMock<MockTetherHostResponseRecorder>>();
    device_id_tether_network_guid_map_ =
        base::MakeUnique<DeviceIdTetherNetworkGuidMap>();

    ON_CALL(*mock_tether_host_response_recorder_,
            GetPreviouslyConnectedHostIds())
        .WillByDefault(Invoke(
            this, &MasterHostScanCacheTest::GetPreviouslyConnectedHostIds));

    host_scan_cache_ = base::MakeUnique<MasterHostScanCache>(
        network_state_handler(), fake_active_host_.get(),
        mock_tether_host_response_recorder_.get(),
        device_id_tether_network_guid_map_.get());
    host_scan_cache_->SetTimerFactoryForTest(
        base::WrapUnique(test_timer_factory_));

    // To track what is expected to be contained in the cache, maintain a
    // FakeHostScanCache in memory and update it alongside |host_scan_cache_|.
    // Use a std::vector to track which device IDs correspond to devices whose
    // Tether networks' HasConnectedToHost fields are expected to be set.
    expected_cache_ = base::MakeUnique<FakeHostScanCache>();
    has_connected_to_host_device_ids_.clear();
  }

  void TearDown() override {
    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  void FireTimer(const std::string& tether_network_guid) {
    ExtendedMockTimer* timer =
        test_timer_factory_
            ->tether_network_guid_to_timer_map()[tether_network_guid];
    ASSERT_TRUE(timer);
    timer->Fire();

    // If the device whose correlated timer has fired is not the active host, it
    // is expected to be removed from the cache.
    EXPECT_EQ(fake_active_host_->GetTetherNetworkGuid(),
              expected_cache_->active_host_tether_network_guid());
    if (fake_active_host_->GetTetherNetworkGuid() != tether_network_guid) {
      expected_cache_->RemoveHostScanResult(tether_network_guid);
    }
  }

  std::vector<std::string> GetPreviouslyConnectedHostIds() const {
    return has_connected_to_host_device_ids_;
  }

  void SetActiveHost(const std::string& tether_network_guid) {
    fake_active_host_->SetActiveHostConnected(
        device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
            tether_network_guid),
        tether_network_guid, "wifiNetworkGuid");
    expected_cache_->set_active_host_tether_network_guid(tether_network_guid);
  }

  void SetHasConnectedToHost(const std::string& tether_network_guid) {
    has_connected_to_host_device_ids_.push_back(
        device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
            tether_network_guid));
    mock_tether_host_response_recorder_
        ->NotifyObserversPreviouslyConnectedHostIdsChanged();
  }

  // Sets host scan results in the cache for the device at index |index|. Index
  // can be from 0 to 3 and corresponds to the index of the constants declared
  // at the top of this test file.
  void SetCacheScanResultForDeviceIndex(int32_t index) {
    // There are 4 sets of test constants.
    ASSERT_TRUE(index >= 0 && index <= 3);

    HostScanCacheEntry::Builder builder;

    switch (index) {
      case 0:
        builder.SetTetherNetworkGuid(kTetherGuid0)
            .SetDeviceName(kTetherDeviceName0)
            .SetCarrier(kTetherCarrier0)
            .SetBatteryPercentage(kTetherBatteryPercentage0)
            .SetSignalStrength(kTetherSignalStrength0)
            .SetSetupRequired(kTetherSetupRequired0);
        break;
      case 1:
        builder.SetTetherNetworkGuid(kTetherGuid1)
            .SetDeviceName(kTetherDeviceName1)
            .SetCarrier(kTetherCarrier1)
            .SetBatteryPercentage(kTetherBatteryPercentage1)
            .SetSignalStrength(kTetherSignalStrength1)
            .SetSetupRequired(kTetherSetupRequired1);
        break;
      case 2:
        builder.SetTetherNetworkGuid(kTetherGuid2)
            .SetDeviceName(kTetherDeviceName2)
            .SetCarrier(kTetherCarrier2)
            .SetBatteryPercentage(kTetherBatteryPercentage2)
            .SetSignalStrength(kTetherSignalStrength2)
            .SetSetupRequired(kTetherSetupRequired2);
        break;
      case 3:
        builder.SetTetherNetworkGuid(kTetherGuid3)
            .SetDeviceName(kTetherDeviceName3)
            .SetCarrier(kTetherCarrier3)
            .SetBatteryPercentage(kTetherBatteryPercentage3)
            .SetSignalStrength(kTetherSignalStrength3)
            .SetSetupRequired(kTetherSetupRequired3);
        break;
      default:
        NOTREACHED();
        break;
    }

    SetHostScanResult(*builder.Build());
  }

  void SetHostScanResult(const HostScanCacheEntry& entry) {
    test_timer_factory_->set_tether_network_guid_for_next_timer(
        entry.tether_network_guid);
    host_scan_cache_->SetHostScanResult(entry);
    expected_cache_->SetHostScanResult(entry);
  }

  void RemoveHostScanResult(const std::string& tether_network_guid) {
    host_scan_cache_->RemoveHostScanResult(tether_network_guid);
    expected_cache_->RemoveHostScanResult(tether_network_guid);
  }

  void ClearCacheExceptForActiveHost() {
    host_scan_cache_->ClearCacheExceptForActiveHost();
    expected_cache_->ClearCacheExceptForActiveHost();
  }

  bool HasConnectedToHost(const std::string& tether_network_guid) {
    auto it =
        std::find(has_connected_to_host_device_ids_.begin(),
                  has_connected_to_host_device_ids_.end(), tether_network_guid);
    return it != has_connected_to_host_device_ids_.end();
  }

  // Verifies that the information present in |expected_cache_| and
  // |has_connected_to_host_device_ids_| mirrors what |host_scan_cache_| has set
  // in NetworkStateHandler.
  void VerifyCacheMatchesNetworkStack() {
    for (auto& it : expected_cache_->cache()) {
      const std::string tether_network_guid = it.first;
      const HostScanCacheEntry& entry = it.second;

      // Ensure that each entry in |expected_cache_| matches the
      // corresponding entry in NetworkStateHandler.
      const NetworkState* tether_network_state =
          network_state_handler()->GetNetworkStateFromGuid(tether_network_guid);
      ASSERT_TRUE(tether_network_state);
      EXPECT_EQ(entry.device_name, tether_network_state->name());
      EXPECT_EQ(entry.carrier, tether_network_state->carrier());
      EXPECT_EQ(entry.battery_percentage,
                tether_network_state->battery_percentage());
      EXPECT_EQ(entry.signal_strength, tether_network_state->signal_strength());
      EXPECT_EQ(entry.setup_required,
                host_scan_cache_->DoesHostRequireSetup(tether_network_guid));
      EXPECT_EQ(HasConnectedToHost(tether_network_guid),
                tether_network_state->tether_has_connected_to_host());

      // Ensure that each entry has an actively-running Timer.
      auto timer_map_it =
          test_timer_factory_->tether_network_guid_to_timer_map().begin();
      EXPECT_NE(timer_map_it,
                test_timer_factory_->tether_network_guid_to_timer_map().end());
      EXPECT_TRUE(timer_map_it->second->IsRunning());
    }
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  TestTimerFactory* test_timer_factory_;
  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<NiceMock<MockTetherHostResponseRecorder>>
      mock_tether_host_response_recorder_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;

  std::vector<std::string> has_connected_to_host_device_ids_;
  std::unique_ptr<FakeHostScanCache> expected_cache_;

  std::unique_ptr<MasterHostScanCache> host_scan_cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MasterHostScanCacheTest);
};

TEST_F(MasterHostScanCacheTest, TestSetScanResultsAndLetThemExpire) {
  SetCacheScanResultForDeviceIndex(0);
  EXPECT_EQ(1u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  SetCacheScanResultForDeviceIndex(1);
  EXPECT_EQ(2u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  SetCacheScanResultForDeviceIndex(2);
  EXPECT_EQ(3u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  SetCacheScanResultForDeviceIndex(3);
  EXPECT_EQ(4u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  FireTimer(kTetherGuid0);
  EXPECT_EQ(3u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  FireTimer(kTetherGuid1);
  EXPECT_EQ(2u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  FireTimer(kTetherGuid2);
  EXPECT_EQ(1u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  FireTimer(kTetherGuid3);
  EXPECT_TRUE(expected_cache_->empty());
  VerifyCacheMatchesNetworkStack();
}

TEST_F(MasterHostScanCacheTest, TestSetScanResultThenUpdateAndRemove) {
  SetCacheScanResultForDeviceIndex(0);
  EXPECT_EQ(1u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  // Change the fields for tether network with GUID |kTetherGuid0| to the
  // fields corresponding to |kTetherGuid1|.
  SetHostScanResult(*HostScanCacheEntry::Builder()
                         .SetTetherNetworkGuid(kTetherGuid0)
                         .SetDeviceName(kTetherDeviceName0)
                         .SetCarrier(kTetherCarrier1)
                         .SetBatteryPercentage(kTetherBatteryPercentage1)
                         .SetSignalStrength(kTetherSignalStrength1)
                         .SetSetupRequired(kTetherSetupRequired1)
                         .Build());
  EXPECT_EQ(1u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  // Now, remove that result.
  RemoveHostScanResult(kTetherGuid0);
  EXPECT_TRUE(expected_cache_->empty());
  VerifyCacheMatchesNetworkStack();
}

TEST_F(MasterHostScanCacheTest, TestSetScanResult_SetActiveHost_ThenClear) {
  SetCacheScanResultForDeviceIndex(0);
  EXPECT_EQ(1u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  SetCacheScanResultForDeviceIndex(1);
  EXPECT_EQ(2u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  SetCacheScanResultForDeviceIndex(2);
  EXPECT_EQ(3u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  // Now, set the active host to be the device 0.
  SetActiveHost(kTetherGuid0);

  // Clear the cache except for the active host.
  ClearCacheExceptForActiveHost();
  EXPECT_EQ(1u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  // Attempt to remove the active host. This operation should fail since
  // removing the active host from the cache is not allowed.
  RemoveHostScanResult(kTetherGuid0);
  EXPECT_EQ(1u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  // Fire the timer for the active host. Likewise, this should not result in the
  // cache entry being removed.
  FireTimer(kTetherGuid0);
  EXPECT_EQ(1u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  // Now, unset the active host.
  SetActiveHost("");

  // Removing the device should now succeed.
  RemoveHostScanResult(kTetherGuid0);
  EXPECT_TRUE(expected_cache_->empty());
  VerifyCacheMatchesNetworkStack();
}

TEST_F(MasterHostScanCacheTest, TestHasConnectedToHost) {
  // Before the test starts, set device 0 as having already connected.
  SetHasConnectedToHost(kTetherGuid0);

  SetCacheScanResultForDeviceIndex(0);
  EXPECT_EQ(1u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  SetCacheScanResultForDeviceIndex(1);
  EXPECT_EQ(2u, expected_cache_->size());
  VerifyCacheMatchesNetworkStack();

  // Simulate a connection to device 1.
  SetActiveHost(kTetherGuid1);
  SetHasConnectedToHost(kTetherGuid1);
  VerifyCacheMatchesNetworkStack();
}

}  // namespace tether

}  // namespace chromeos
