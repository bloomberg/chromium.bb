// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/cryptauth/cryptauth_device_manager.h"

#include <stddef.h>
#include <utility>

#include "base/base64url.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proximity_auth/cryptauth/fake_cryptauth_gcm_manager.h"
#include "components/proximity_auth/cryptauth/mock_cryptauth_client.h"
#include "components/proximity_auth/cryptauth/mock_sync_scheduler.h"
#include "components/proximity_auth/cryptauth/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace proximity_auth {
namespace {

// The initial "Now" time for testing.
const double kInitialTimeNowSeconds = 20000000;

// A later "Now" time for testing.
const double kLaterTimeNowSeconds = kInitialTimeNowSeconds + 30;

// The timestamp of a last successful sync in seconds.
const double kLastSyncTimeSeconds = kInitialTimeNowSeconds - (60 * 60 * 5);

// Unlock key fields originally stored in the user prefs.
const char kStoredPublicKey[] = "AAPL";
const char kStoredDeviceName[] = "iPhone 6";
const char kStoredBluetoothAddress[] = "12:34:56:78:90:AB";

// ExternalDeviceInfo fields for the synced unlock key.
const char kPublicKey1[] = "GOOG";
const char kDeviceName1[] = "Nexus 5";
const char kBluetoothAddress1[] = "aa:bb:cc:ee:dd:ff";

// ExternalDeviceInfo fields for a non-synced unlockable device.
const char kPublicKey2[] = "MSFT";
const char kDeviceName2[] = "Surface Pro 3";

// Validates that |unlock_keys| and the corresponding preferences stored by
// |pref_service| are equal to |expected_unlock_keys|.
void ExpectUnlockKeysAndPrefAreEqual(
    const std::vector<cryptauth::ExternalDeviceInfo>& expected_unlock_keys,
    const std::vector<cryptauth::ExternalDeviceInfo>& unlock_keys,
    const PrefService& pref_service) {
  ASSERT_EQ(expected_unlock_keys.size(), unlock_keys.size());
  for (size_t i = 0; i < unlock_keys.size(); ++i) {
    SCOPED_TRACE(
        base::StringPrintf("Compare protos at index=%d", static_cast<int>(i)));
    const auto& expected_unlock_key = expected_unlock_keys[i];
    const auto& unlock_key = unlock_keys.at(i);
    EXPECT_EQ(expected_unlock_key.public_key(), unlock_key.public_key());
    EXPECT_EQ(expected_unlock_key.friendly_device_name(),
              unlock_key.friendly_device_name());
    EXPECT_EQ(expected_unlock_key.bluetooth_address(),
              unlock_key.bluetooth_address());
    EXPECT_TRUE(expected_unlock_key.unlock_key());
    EXPECT_FALSE(expected_unlock_key.unlockable());
  }

  const base::ListValue* unlock_keys_pref =
      pref_service.GetList(prefs::kCryptAuthDeviceSyncUnlockKeys);
  ASSERT_EQ(expected_unlock_keys.size(), unlock_keys_pref->GetSize());
  for (size_t i = 0; i < unlock_keys_pref->GetSize(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Compare pref dictionary at index=%d",
                                    static_cast<int>(i)));
    const base::DictionaryValue* unlock_key_dictionary;
    EXPECT_TRUE(unlock_keys_pref->GetDictionary(i, &unlock_key_dictionary));
    std::string public_key_b64, device_name_b64, bluetooth_address_b64;
    ASSERT_TRUE(
        unlock_key_dictionary->GetString("public_key", &public_key_b64));
    ASSERT_TRUE(
        unlock_key_dictionary->GetString("device_name", &device_name_b64));
    ASSERT_TRUE(unlock_key_dictionary->GetString("bluetooth_address",
                                                 &bluetooth_address_b64));

    std::string public_key, device_name, bluetooth_address;
    ASSERT_TRUE(base::Base64UrlDecode(
        public_key_b64, base::Base64UrlDecodePolicy::REQUIRE_PADDING,
        &public_key));
    ASSERT_TRUE(base::Base64UrlDecode(
        device_name_b64, base::Base64UrlDecodePolicy::REQUIRE_PADDING,
        &device_name));
    ASSERT_TRUE(base::Base64UrlDecode(
        bluetooth_address_b64, base::Base64UrlDecodePolicy::REQUIRE_PADDING,
        &bluetooth_address));

    const auto& expected_unlock_key = expected_unlock_keys[i];
    EXPECT_EQ(expected_unlock_key.public_key(), public_key);
    EXPECT_EQ(expected_unlock_key.friendly_device_name(), device_name);
    EXPECT_EQ(expected_unlock_key.bluetooth_address(), bluetooth_address);
    EXPECT_TRUE(expected_unlock_key.unlock_key());
    EXPECT_FALSE(expected_unlock_key.unlockable());
  }
}

// Harness for testing CryptAuthDeviceManager.
class TestCryptAuthDeviceManager : public CryptAuthDeviceManager {
 public:
  TestCryptAuthDeviceManager(scoped_ptr<base::Clock> clock,
                             scoped_ptr<CryptAuthClientFactory> client_factory,
                             CryptAuthGCMManager* gcm_manager,
                             PrefService* pref_service)
      : CryptAuthDeviceManager(std::move(clock),
                               std::move(client_factory),
                               gcm_manager,
                               pref_service),
        scoped_sync_scheduler_(new NiceMock<MockSyncScheduler>()),
        weak_sync_scheduler_factory_(scoped_sync_scheduler_.get()) {}

  ~TestCryptAuthDeviceManager() override {}

  scoped_ptr<SyncScheduler> CreateSyncScheduler() override {
    EXPECT_TRUE(scoped_sync_scheduler_);
    return std::move(scoped_sync_scheduler_);
  }

  base::WeakPtr<MockSyncScheduler> GetSyncScheduler() {
    return weak_sync_scheduler_factory_.GetWeakPtr();
  }

 private:
  // Ownership is passed to |CryptAuthDeviceManager| super class when
  // |CreateSyncScheduler()| is called.
  scoped_ptr<MockSyncScheduler> scoped_sync_scheduler_;

  // Stores the pointer of |scoped_sync_scheduler_| after ownership is passed to
  // the super class.
  // This should be safe because the life-time this SyncScheduler will always be
  // within the life of the TestCryptAuthDeviceManager object.
  base::WeakPtrFactory<MockSyncScheduler> weak_sync_scheduler_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestCryptAuthDeviceManager);
};

}  // namespace

class ProximityAuthCryptAuthDeviceManagerTest
    : public testing::Test,
      public CryptAuthDeviceManager::Observer,
      public MockCryptAuthClientFactory::Observer {
 protected:
  ProximityAuthCryptAuthDeviceManagerTest()
      : clock_(new base::SimpleTestClock()),
        client_factory_(new MockCryptAuthClientFactory(
            MockCryptAuthClientFactory::MockType::MAKE_STRICT_MOCKS)),
        gcm_manager_("existing gcm registration id") {
    client_factory_->AddObserver(this);
  }

  ~ProximityAuthCryptAuthDeviceManagerTest() {
    client_factory_->RemoveObserver(this);
  }

  // testing::Test:
  void SetUp() override {
    clock_->SetNow(base::Time::FromDoubleT(kInitialTimeNowSeconds));

    CryptAuthDeviceManager::RegisterPrefs(pref_service_.registry());
    pref_service_.SetUserPref(
        prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure,
        new base::FundamentalValue(false));
    pref_service_.SetUserPref(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds,
                              new base::FundamentalValue(kLastSyncTimeSeconds));
    pref_service_.SetUserPref(
        prefs::kCryptAuthDeviceSyncReason,
        new base::FundamentalValue(cryptauth::INVOCATION_REASON_UNKNOWN));

    scoped_ptr<base::DictionaryValue> unlock_key_dictionary(
        new base::DictionaryValue());

    std::string public_key_b64, device_name_b64, bluetooth_address_b64;
    base::Base64UrlEncode(kStoredPublicKey,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &public_key_b64);
    base::Base64UrlEncode(kStoredDeviceName,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &device_name_b64);
    base::Base64UrlEncode(kStoredBluetoothAddress,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &bluetooth_address_b64);

    unlock_key_dictionary->SetString("public_key", public_key_b64);
    unlock_key_dictionary->SetString("device_name", device_name_b64);
    unlock_key_dictionary->SetString("bluetooth_address",
                                     bluetooth_address_b64);
    {
      ListPrefUpdate update(&pref_service_,
                            prefs::kCryptAuthDeviceSyncUnlockKeys);
      update.Get()->Append(std::move(unlock_key_dictionary));
    }

    device_manager_.reset(new TestCryptAuthDeviceManager(
        make_scoped_ptr(clock_), make_scoped_ptr(client_factory_),
        &gcm_manager_, &pref_service_));
    device_manager_->AddObserver(this);

    cryptauth::ExternalDeviceInfo unlock_key;
    unlock_key.set_public_key(kPublicKey1);
    unlock_key.set_friendly_device_name(kDeviceName1);
    unlock_key.set_bluetooth_address(kBluetoothAddress1);
    unlock_key.set_unlock_key(true);
    unlock_key.set_unlockable(false);

    cryptauth::ExternalDeviceInfo unlockable_device;
    unlockable_device.set_public_key(kPublicKey2);
    unlockable_device.set_friendly_device_name(kDeviceName2);
    unlockable_device.set_unlock_key(false);
    unlockable_device.set_unlockable(true);

    get_my_devices_response_.add_devices()->CopyFrom(unlock_key);
    get_my_devices_response_.add_devices()->CopyFrom(unlockable_device);

    ON_CALL(*sync_scheduler(), GetStrategy())
        .WillByDefault(Return(SyncScheduler::Strategy::PERIODIC_REFRESH));
  }

  void TearDown() override { device_manager_->RemoveObserver(this); }

  // CryptAuthDeviceManager::Observer:
  void OnSyncStarted() override { OnSyncStartedProxy(); }

  void OnSyncFinished(CryptAuthDeviceManager::SyncResult sync_result,
                      CryptAuthDeviceManager::DeviceChangeResult
                          device_change_result) override {
    OnSyncFinishedProxy(sync_result, device_change_result);
  }

  MOCK_METHOD0(OnSyncStartedProxy, void());
  MOCK_METHOD2(OnSyncFinishedProxy,
               void(CryptAuthDeviceManager::SyncResult,
                    CryptAuthDeviceManager::DeviceChangeResult));

  // Simulates firing the SyncScheduler to trigger a device sync attempt.
  void FireSchedulerForSync(
      cryptauth::InvocationReason expected_invocation_reason) {
    SyncScheduler::Delegate* delegate =
        static_cast<SyncScheduler::Delegate*>(device_manager_.get());

    scoped_ptr<SyncScheduler::SyncRequest> sync_request = make_scoped_ptr(
        new SyncScheduler::SyncRequest(device_manager_->GetSyncScheduler()));
    EXPECT_CALL(*this, OnSyncStartedProxy());
    delegate->OnSyncRequested(std::move(sync_request));

    EXPECT_EQ(expected_invocation_reason,
              get_my_devices_request_.invocation_reason());

    // The allow_stale_read flag is set if the sync was not forced.
    bool allow_stale_read =
        pref_service_.GetInteger(prefs::kCryptAuthDeviceSyncReason) !=
        cryptauth::INVOCATION_REASON_UNKNOWN;
    EXPECT_EQ(allow_stale_read, get_my_devices_request_.allow_stale_read());
  }

  // MockCryptAuthClientFactory::Observer:
  void OnCryptAuthClientCreated(MockCryptAuthClient* client) override {
    EXPECT_CALL(*client, GetMyDevices(_, _, _))
        .WillOnce(DoAll(SaveArg<0>(&get_my_devices_request_),
                        SaveArg<1>(&success_callback_),
                        SaveArg<2>(&error_callback_)));
  }

  MockSyncScheduler* sync_scheduler() {
    return device_manager_->GetSyncScheduler().get();
  }

  // Owned by |device_manager_|.
  base::SimpleTestClock* clock_;

  // Owned by |device_manager_|.
  MockCryptAuthClientFactory* client_factory_;

  TestingPrefServiceSimple pref_service_;

  FakeCryptAuthGCMManager gcm_manager_;

  scoped_ptr<TestCryptAuthDeviceManager> device_manager_;

  cryptauth::GetMyDevicesResponse get_my_devices_response_;

  cryptauth::GetMyDevicesRequest get_my_devices_request_;

  CryptAuthClient::GetMyDevicesCallback success_callback_;

  CryptAuthClient::ErrorCallback error_callback_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthCryptAuthDeviceManagerTest);
};

TEST_F(ProximityAuthCryptAuthDeviceManagerTest, RegisterPrefs) {
  TestingPrefServiceSimple pref_service;
  CryptAuthDeviceManager::RegisterPrefs(pref_service.registry());
  EXPECT_TRUE(pref_service.FindPreference(
      prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds));
  EXPECT_TRUE(pref_service.FindPreference(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure));
  EXPECT_TRUE(pref_service.FindPreference(prefs::kCryptAuthDeviceSyncReason));
  EXPECT_TRUE(
      pref_service.FindPreference(prefs::kCryptAuthDeviceSyncUnlockKeys));
}

TEST_F(ProximityAuthCryptAuthDeviceManagerTest, GetSyncState) {
  device_manager_->Start();

  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::PERIODIC_REFRESH));
  EXPECT_FALSE(device_manager_->IsRecoveringFromFailure());

  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::AGGRESSIVE_RECOVERY));
  EXPECT_TRUE(device_manager_->IsRecoveringFromFailure());

  base::TimeDelta time_to_next_sync = base::TimeDelta::FromMinutes(60);
  ON_CALL(*sync_scheduler(), GetTimeToNextSync())
      .WillByDefault(Return(time_to_next_sync));
  EXPECT_EQ(time_to_next_sync, device_manager_->GetTimeToNextAttempt());

  ON_CALL(*sync_scheduler(), GetSyncState())
      .WillByDefault(Return(SyncScheduler::SyncState::SYNC_IN_PROGRESS));
  EXPECT_TRUE(device_manager_->IsSyncInProgress());

  ON_CALL(*sync_scheduler(), GetSyncState())
      .WillByDefault(Return(SyncScheduler::SyncState::WAITING_FOR_REFRESH));
  EXPECT_FALSE(device_manager_->IsSyncInProgress());
}

TEST_F(ProximityAuthCryptAuthDeviceManagerTest, InitWithDefaultPrefs) {
  scoped_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  clock->SetNow(base::Time::FromDoubleT(kInitialTimeNowSeconds));
  base::TimeDelta elapsed_time = clock->Now() - base::Time::FromDoubleT(0);

  TestingPrefServiceSimple pref_service;
  CryptAuthDeviceManager::RegisterPrefs(pref_service.registry());

  TestCryptAuthDeviceManager device_manager(
      std::move(clock),
      make_scoped_ptr(new MockCryptAuthClientFactory(
          MockCryptAuthClientFactory::MockType::MAKE_STRICT_MOCKS)),
      &gcm_manager_, &pref_service);

  EXPECT_CALL(
      *(device_manager.GetSyncScheduler()),
      Start(elapsed_time, SyncScheduler::Strategy::AGGRESSIVE_RECOVERY));
  device_manager.Start();
  EXPECT_TRUE(device_manager.GetLastSyncTime().is_null());
  EXPECT_EQ(0u, device_manager.unlock_keys().size());
}

TEST_F(ProximityAuthCryptAuthDeviceManagerTest, InitWithExistingPrefs) {
  EXPECT_CALL(
      *sync_scheduler(),
      Start(clock_->Now() - base::Time::FromDoubleT(kLastSyncTimeSeconds),
            SyncScheduler::Strategy::PERIODIC_REFRESH));

  device_manager_->Start();
  EXPECT_EQ(base::Time::FromDoubleT(kLastSyncTimeSeconds),
            device_manager_->GetLastSyncTime());

  auto unlock_keys = device_manager_->unlock_keys();
  ASSERT_EQ(1u, unlock_keys.size());
  EXPECT_EQ(kStoredPublicKey, unlock_keys[0].public_key());
  EXPECT_EQ(kStoredDeviceName, unlock_keys[0].friendly_device_name());
  EXPECT_EQ(kStoredBluetoothAddress, unlock_keys[0].bluetooth_address());
  EXPECT_TRUE(unlock_keys[0].unlock_key());
}

TEST_F(ProximityAuthCryptAuthDeviceManagerTest, SyncSucceedsForFirstTime) {
  pref_service_.ClearPref(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds);
  device_manager_->Start();

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_INITIALIZATION);
  ASSERT_FALSE(success_callback_.is_null());

  clock_->SetNow(base::Time::FromDoubleT(kLaterTimeNowSeconds));
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));

  success_callback_.Run(get_my_devices_response_);
  EXPECT_EQ(clock_->Now(), device_manager_->GetLastSyncTime());

  ExpectUnlockKeysAndPrefAreEqual(std::vector<cryptauth::ExternalDeviceInfo>(
                                      1, get_my_devices_response_.devices(0)),
                                  device_manager_->unlock_keys(),
                                  pref_service_);
}

TEST_F(ProximityAuthCryptAuthDeviceManagerTest, ForceSync) {
  device_manager_->Start();

  EXPECT_CALL(*sync_scheduler(), ForceSync());
  device_manager_->ForceSyncNow(cryptauth::INVOCATION_REASON_MANUAL);

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_MANUAL);

  clock_->SetNow(base::Time::FromDoubleT(kLaterTimeNowSeconds));
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);
  EXPECT_EQ(clock_->Now(), device_manager_->GetLastSyncTime());

  ExpectUnlockKeysAndPrefAreEqual(std::vector<cryptauth::ExternalDeviceInfo>(
                                      1, get_my_devices_response_.devices(0)),
                                  device_manager_->unlock_keys(),
                                  pref_service_);
}

TEST_F(ProximityAuthCryptAuthDeviceManagerTest, ForceSyncFailsThenSucceeds) {
  device_manager_->Start();
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure));
  base::Time old_sync_time = device_manager_->GetLastSyncTime();

  // The first force sync fails.
  EXPECT_CALL(*sync_scheduler(), ForceSync());
  device_manager_->ForceSyncNow(cryptauth::INVOCATION_REASON_MANUAL);
  FireSchedulerForSync(cryptauth::INVOCATION_REASON_MANUAL);
  clock_->SetNow(base::Time::FromDoubleT(kLaterTimeNowSeconds));
  EXPECT_CALL(*this,
              OnSyncFinishedProxy(
                  CryptAuthDeviceManager::SyncResult::FAILURE,
                  CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED));
  error_callback_.Run("404");
  EXPECT_EQ(old_sync_time, device_manager_->GetLastSyncTime());
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure));
  EXPECT_EQ(static_cast<int>(cryptauth::INVOCATION_REASON_MANUAL),
            pref_service_.GetInteger(prefs::kCryptAuthDeviceSyncReason));

  // The second recovery sync succeeds.
  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::AGGRESSIVE_RECOVERY));
  FireSchedulerForSync(cryptauth::INVOCATION_REASON_MANUAL);
  clock_->SetNow(base::Time::FromDoubleT(kLaterTimeNowSeconds + 30));
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);
  EXPECT_EQ(clock_->Now(), device_manager_->GetLastSyncTime());

  ExpectUnlockKeysAndPrefAreEqual(std::vector<cryptauth::ExternalDeviceInfo>(
                                      1, get_my_devices_response_.devices(0)),
                                  device_manager_->unlock_keys(),
                                  pref_service_);

  EXPECT_FLOAT_EQ(
      clock_->Now().ToDoubleT(),
      pref_service_.GetDouble(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds));
  EXPECT_EQ(static_cast<int>(cryptauth::INVOCATION_REASON_UNKNOWN),
            pref_service_.GetInteger(prefs::kCryptAuthDeviceSyncReason));
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure));
}

TEST_F(ProximityAuthCryptAuthDeviceManagerTest, PeriodicSyncFailsThenSucceeds) {
  device_manager_->Start();
  base::Time old_sync_time = device_manager_->GetLastSyncTime();

  // The first periodic sync fails.
  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  clock_->SetNow(base::Time::FromDoubleT(kLaterTimeNowSeconds));
  EXPECT_CALL(*this,
              OnSyncFinishedProxy(
                  CryptAuthDeviceManager::SyncResult::FAILURE,
                  CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED));
  error_callback_.Run("401");
  EXPECT_EQ(old_sync_time, device_manager_->GetLastSyncTime());
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure));

  // The second recovery sync succeeds.
  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::AGGRESSIVE_RECOVERY));
  FireSchedulerForSync(cryptauth::INVOCATION_REASON_FAILURE_RECOVERY);
  clock_->SetNow(base::Time::FromDoubleT(kLaterTimeNowSeconds + 30));
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);
  EXPECT_EQ(clock_->Now(), device_manager_->GetLastSyncTime());

  ExpectUnlockKeysAndPrefAreEqual(std::vector<cryptauth::ExternalDeviceInfo>(
                                      1, get_my_devices_response_.devices(0)),
                                  device_manager_->unlock_keys(),
                                  pref_service_);

  EXPECT_FLOAT_EQ(
      clock_->Now().ToDoubleT(),
      pref_service_.GetDouble(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds));
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure));
}

TEST_F(ProximityAuthCryptAuthDeviceManagerTest, SyncSameDevice) {
  device_manager_->Start();
  auto original_unlock_keys = device_manager_->unlock_keys();

  // Sync new devices.
  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  ASSERT_FALSE(success_callback_.is_null());
  EXPECT_CALL(*this,
              OnSyncFinishedProxy(
                  CryptAuthDeviceManager::SyncResult::SUCCESS,
                  CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED));

  // Sync the same device.
  cryptauth::ExternalDeviceInfo synced_unlock_key;
  synced_unlock_key.set_public_key(kStoredPublicKey);
  synced_unlock_key.set_friendly_device_name(kStoredDeviceName);
  synced_unlock_key.set_bluetooth_address(kStoredBluetoothAddress);
  synced_unlock_key.set_unlock_key(true);
  synced_unlock_key.set_unlockable(false);
  cryptauth::GetMyDevicesResponse get_my_devices_response;
  get_my_devices_response.add_devices()->CopyFrom(synced_unlock_key);
  success_callback_.Run(get_my_devices_response);

  // Check that unlock keys are still the same after sync.
  ExpectUnlockKeysAndPrefAreEqual(
      original_unlock_keys, device_manager_->unlock_keys(), pref_service_);
}

TEST_F(ProximityAuthCryptAuthDeviceManagerTest, SyncEmptyDeviceList) {
  cryptauth::GetMyDevicesResponse empty_response;

  device_manager_->Start();
  EXPECT_EQ(1u, device_manager_->unlock_keys().size());

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  ASSERT_FALSE(success_callback_.is_null());
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(empty_response);

  ExpectUnlockKeysAndPrefAreEqual(std::vector<cryptauth::ExternalDeviceInfo>(),
                                  device_manager_->unlock_keys(),
                                  pref_service_);
}

TEST_F(ProximityAuthCryptAuthDeviceManagerTest, SyncTwoUnlockKeys) {
  cryptauth::GetMyDevicesResponse response(get_my_devices_response_);
  cryptauth::ExternalDeviceInfo unlock_key2;
  unlock_key2.set_public_key("new public key");
  unlock_key2.set_friendly_device_name("new device name");
  unlock_key2.set_bluetooth_address("aa:bb:cc:dd:ee:ff");
  unlock_key2.set_unlock_key(true);
  response.add_devices()->CopyFrom(unlock_key2);

  std::vector<cryptauth::ExternalDeviceInfo> expected_unlock_keys;
  expected_unlock_keys.push_back(get_my_devices_response_.devices(0));
  expected_unlock_keys.push_back(unlock_key2);

  device_manager_->Start();
  EXPECT_EQ(1u, device_manager_->unlock_keys().size());
  EXPECT_EQ(1u, pref_service_.GetList(prefs::kCryptAuthDeviceSyncUnlockKeys)
                    ->GetSize());

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_PERIODIC);
  ASSERT_FALSE(success_callback_.is_null());
  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(response);

  ExpectUnlockKeysAndPrefAreEqual(
      expected_unlock_keys, device_manager_->unlock_keys(), pref_service_);
}

TEST_F(ProximityAuthCryptAuthDeviceManagerTest, SyncOnGCMPushMessage) {
  device_manager_->Start();

  EXPECT_CALL(*sync_scheduler(), ForceSync());
  gcm_manager_.PushResyncMessage();

  FireSchedulerForSync(cryptauth::INVOCATION_REASON_SERVER_INITIATED);

  EXPECT_CALL(*this, OnSyncFinishedProxy(
                         CryptAuthDeviceManager::SyncResult::SUCCESS,
                         CryptAuthDeviceManager::DeviceChangeResult::CHANGED));
  success_callback_.Run(get_my_devices_response_);

  ExpectUnlockKeysAndPrefAreEqual(std::vector<cryptauth::ExternalDeviceInfo>(
                                      1, get_my_devices_response_.devices(0)),
                                  device_manager_->unlock_keys(),
                                  pref_service_);
}

}  // namespace proximity_auth
