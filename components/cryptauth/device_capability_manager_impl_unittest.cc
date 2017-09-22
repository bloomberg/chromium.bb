// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/device_capability_manager_impl.h"

#include <stddef.h>

#include <chrono>
#include <memory>
#include <thread>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/cryptauth/mock_cryptauth_client.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace cryptauth {

namespace {

const char kSuccessResult[] = "success";
const char kErrorOnToggleEasyUnlockResult[] = "toggleEasyUnlockError";
const char kErrorFindEligibleUnlockDevices[] = "findEligibleUnlockDeviceError";
const char kErrorFindEligibleForPromotion[] = "findEligibleForPromotionError";
const std::string kTestErrorCodeTogglingIneligibleDevice =
    "togglingIneligibleDevice.";

std::vector<cryptauth::ExternalDeviceInfo>
CreateExternalDeviceInfosForRemoteDevices(
    const std::vector<cryptauth::RemoteDevice> remote_devices) {
  std::vector<cryptauth::ExternalDeviceInfo> device_infos;
  for (const auto& remote_device : remote_devices) {
    // Add an ExternalDeviceInfo with the same public key as the RemoteDevice.
    cryptauth::ExternalDeviceInfo info;
    info.set_public_key(remote_device.public_key);
    device_infos.push_back(info);
  }
  return device_infos;
}

}  // namespace

class DeviceCapabilityManagerImplTest
    : public testing::Test,
      public MockCryptAuthClientFactory::Observer {
 public:
  DeviceCapabilityManagerImplTest()
      : all_test_external_device_infos_(
            CreateExternalDeviceInfosForRemoteDevices(
                cryptauth::GenerateTestRemoteDevices(5))),
        test_eligible_external_devices_infos_(
            {all_test_external_device_infos_[0],
             all_test_external_device_infos_[1],
             all_test_external_device_infos_[2]}),
        test_ineligible_external_devices_infos_(
            {all_test_external_device_infos_[3],
             all_test_external_device_infos_[4]}) {}

  void SetUp() override {
    mock_cryptauth_client_factory_ =
        base::MakeUnique<MockCryptAuthClientFactory>(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS);
    mock_cryptauth_client_factory_->AddObserver(this);
    device_capability_manager_ = base::MakeUnique<DeviceCapabilityManagerImpl>(
        mock_cryptauth_client_factory_.get());
  }

  void OnCryptAuthClientCreated(MockCryptAuthClient* client) override {
    ON_CALL(*client, ToggleEasyUnlock(_, _, _))
        .WillByDefault(Invoke(
            this, &DeviceCapabilityManagerImplTest::MockToggleEasyUnlock));
    ON_CALL(*client, FindEligibleUnlockDevices(_, _, _))
        .WillByDefault(Invoke(
            this,
            &DeviceCapabilityManagerImplTest::MockFindEligibleUnlockDevices));
    ON_CALL(*client, FindEligibleForPromotion(_, _, _))
        .WillByDefault(Invoke(
            this,
            &DeviceCapabilityManagerImplTest::MockFindEligibleForPromotion));
  }

  // Mock CryptAuthClient::ToggleEasyUnlock() implementation.
  void MockToggleEasyUnlock(
      const ToggleEasyUnlockRequest& request,
      const CryptAuthClient::ToggleEasyUnlockCallback& callback,
      const CryptAuthClient::ErrorCallback& error_callback) {
    toggle_easy_unlock_callback_ = callback;
    error_callback_ = error_callback;
    error_code_ = kErrorOnToggleEasyUnlockResult;
  }

  // Mock CryptAuthClient::FindEligibleUnlockDevices() implementation.
  void MockFindEligibleUnlockDevices(
      const FindEligibleUnlockDevicesRequest& request,
      const CryptAuthClient::FindEligibleUnlockDevicesCallback& callback,
      const CryptAuthClient::ErrorCallback& error_callback) {
    find_eligible_unlock_devices_callback_ = callback;
    error_callback_ = error_callback;
    error_code_ = kErrorFindEligibleUnlockDevices;
  }

  // Mock CryptAuthClient::FindEligibleForPromotion() implementation.
  void MockFindEligibleForPromotion(
      const FindEligibleForPromotionRequest& request,
      const CryptAuthClient::FindEligibleForPromotionCallback& callback,
      const CryptAuthClient::ErrorCallback& error_callback) {
    find_eligible_for_promotion_callback_ = callback;
    error_callback_ = error_callback;
    error_code_ = kErrorFindEligibleForPromotion;
  }

  FindEligibleUnlockDevicesResponse CreateFindEligibleUnlockDevicesResponse() {
    FindEligibleUnlockDevicesResponse find_eligible_unlock_devices_response;
    for (const auto& device_info : test_eligible_external_devices_infos_) {
      find_eligible_unlock_devices_response.add_eligible_devices()->CopyFrom(
          device_info);
    }
    for (const auto& device_info : test_ineligible_external_devices_infos_) {
      find_eligible_unlock_devices_response.add_ineligible_devices()
          ->mutable_device()
          ->CopyFrom(device_info);
    }
    return find_eligible_unlock_devices_response;
  }

  void VerifyDeviceEligibility() {
    // Ensure that resulting devices are not empty.  Otherwise, following for
    // loop checks will succeed on empty resulting devices.
    EXPECT_TRUE(result_eligible_devices_.size() > 0);
    EXPECT_TRUE(result_ineligible_devices_.size() > 0);
    for (const auto& device_info : result_eligible_devices_) {
      EXPECT_TRUE(
          std::find_if(
              test_eligible_external_devices_infos_.begin(),
              test_eligible_external_devices_infos_.end(),
              [&device_info](const cryptauth::ExternalDeviceInfo& device) {
                return device.public_key() == device_info.public_key();
              }) != test_eligible_external_devices_infos_.end());
    }
    for (const auto& ineligible_device : result_ineligible_devices_) {
      EXPECT_TRUE(
          std::find_if(test_ineligible_external_devices_infos_.begin(),
                       test_ineligible_external_devices_infos_.end(),
                       [&ineligible_device](
                           const cryptauth::ExternalDeviceInfo& device) {
                         return device.public_key() ==
                                ineligible_device.device().public_key();
                       }) != test_ineligible_external_devices_infos_.end());
    }
    result_eligible_devices_.clear();
    result_ineligible_devices_.clear();
  }

  void SetCapabilityEnabled(DeviceCapabilityManagerImpl::Capability capability,
                            const ExternalDeviceInfo& device_info,
                            bool enable) {
    device_capability_manager_->SetCapabilityEnabled(
        device_info.public_key(), capability, enable,
        base::Bind(&DeviceCapabilityManagerImplTest::
                       TestSuccessSetCapabilityKeyUnlockCallback,
                   base::Unretained(this)),
        base::Bind(&DeviceCapabilityManagerImplTest::TestErrorCallback,
                   base::Unretained(this)));
  }

  void FindEligibleDevicesForCapability(
      DeviceCapabilityManagerImpl::Capability capability) {
    device_capability_manager_->FindEligibleDevicesForCapability(
        DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
        base::Bind(&DeviceCapabilityManagerImplTest::
                       TestSuccessFindEligibleUnlockDevicesCallback,
                   base::Unretained(this)),
        base::Bind(&DeviceCapabilityManagerImplTest::TestErrorCallback,
                   base::Unretained(this)));
  }

  void IsPromotableDevice(DeviceCapabilityManagerImpl::Capability capability,
                          const std::string& public_key) {
    device_capability_manager_->IsCapabilityPromotable(
        public_key, capability,
        base::Bind(&DeviceCapabilityManagerImplTest::
                       TestSuccessFindEligibleForPromotionDeviceCallback,
                   base::Unretained(this)),
        base::Bind(&DeviceCapabilityManagerImplTest::TestErrorCallback,
                   base::Unretained(this)));
  }

  void TestSuccessSetCapabilityKeyUnlockCallback() { result_ = kSuccessResult; }

  void TestSuccessFindEligibleUnlockDevicesCallback(
      const std::vector<ExternalDeviceInfo>& eligible_devices,
      const std::vector<IneligibleDevice>& ineligible_devices) {
    result_ = kSuccessResult;
    result_eligible_devices_ = eligible_devices;
    result_ineligible_devices_ = ineligible_devices;
  }

  void TestSuccessFindEligibleForPromotionDeviceCallback(bool eligible) {
    result_ = kSuccessResult;
    result_eligible_for_promotion_ = eligible;
  }

  void TestErrorCallback(const std::string& error_message) {
    result_ = error_message;
  }

  void InvokeSetCapabilityKeyUnlockCallback() {
    CryptAuthClient::ToggleEasyUnlockCallback success_callback =
        toggle_easy_unlock_callback_;
    ASSERT_TRUE(!success_callback.is_null());
    toggle_easy_unlock_callback_.Reset();
    success_callback.Run(ToggleEasyUnlockResponse());
  }

  void InvokeFindEligibleUnlockDevicesCallback(
      const FindEligibleUnlockDevicesResponse& retrieved_devices_response) {
    CryptAuthClient::FindEligibleUnlockDevicesCallback success_callback =
        find_eligible_unlock_devices_callback_;
    ASSERT_TRUE(!success_callback.is_null());
    find_eligible_unlock_devices_callback_.Reset();
    success_callback.Run(retrieved_devices_response);
  }

  void InvokeFindEligibleForPromotionCallback(bool eligible_for_promotion) {
    FindEligibleForPromotionResponse response;
    response.set_may_show_promo(eligible_for_promotion);
    CryptAuthClient::FindEligibleForPromotionCallback success_callback =
        find_eligible_for_promotion_callback_;
    ASSERT_TRUE(!success_callback.is_null());
    find_eligible_for_promotion_callback_.Reset();
    success_callback.Run(response);
  }

  void InvokeErrorCallback() {
    CryptAuthClient::ErrorCallback error_callback = error_callback_;
    ASSERT_TRUE(!error_callback.is_null());
    error_callback_.Reset();
    error_callback.Run(error_code_);
  }

  std::string GetResultAndReset() {
    std::string result;
    result.swap(result_);
    return result;
  }

  bool GetEligibleForPromotionAndReset() {
    bool result_eligible_for_promotion = result_eligible_for_promotion_;
    result_eligible_for_promotion_ = false;
    return result_eligible_for_promotion;
  }

  const std::vector<cryptauth::ExternalDeviceInfo>
      all_test_external_device_infos_;
  const std::vector<ExternalDeviceInfo> test_eligible_external_devices_infos_;
  const std::vector<ExternalDeviceInfo> test_ineligible_external_devices_infos_;

  std::unique_ptr<MockCryptAuthClientFactory> mock_cryptauth_client_factory_;
  std::unique_ptr<cryptauth::DeviceCapabilityManagerImpl>
      device_capability_manager_;
  CryptAuthClient::ToggleEasyUnlockCallback toggle_easy_unlock_callback_;
  CryptAuthClient::FindEligibleUnlockDevicesCallback
      find_eligible_unlock_devices_callback_;
  CryptAuthClient::FindEligibleForPromotionCallback
      find_eligible_for_promotion_callback_;
  CryptAuthClient::ErrorCallback error_callback_;

  // Used by all tests that involve CryptauthClient network calls internally.
  // Network call statuses are mocked out in place by |result_| and
  // |error_code_| to keep track of the order in which DevicaCapabilityManager
  // functions are called.
  std::string result_;
  std::string error_code_;
  // For FindEligibleUnlockDevice tests.
  std::vector<ExternalDeviceInfo> result_eligible_devices_;
  std::vector<IneligibleDevice> result_ineligible_devices_;
  // For FindEligibleForPromotion tests.
  bool result_eligible_for_promotion_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCapabilityManagerImplTest);
};

TEST_F(DeviceCapabilityManagerImplTest, TestOrderUponMultipleRequests) {
  SetCapabilityEnabled(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
      test_eligible_external_devices_infos_[0], true /* enable */);
  FindEligibleDevicesForCapability(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY);
  IsPromotableDevice(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
      test_eligible_external_devices_infos_[0].public_key());
  SetCapabilityEnabled(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
      test_eligible_external_devices_infos_[1], true /* enable */);
  FindEligibleDevicesForCapability(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY);
  IsPromotableDevice(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
      test_eligible_external_devices_infos_[1].public_key());

  InvokeSetCapabilityKeyUnlockCallback();
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  InvokeFindEligibleUnlockDevicesCallback(
      CreateFindEligibleUnlockDevicesResponse());
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  VerifyDeviceEligibility();

  InvokeFindEligibleForPromotionCallback(true /* eligible */);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_TRUE(GetEligibleForPromotionAndReset());

  InvokeSetCapabilityKeyUnlockCallback();
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  InvokeFindEligibleUnlockDevicesCallback(
      CreateFindEligibleUnlockDevicesResponse());
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  VerifyDeviceEligibility();

  InvokeFindEligibleForPromotionCallback(true /* eligible */);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_TRUE(GetEligibleForPromotionAndReset());
}

TEST_F(DeviceCapabilityManagerImplTest, TestMultipleSetUnlocksRequests) {
  SetCapabilityEnabled(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
      test_eligible_external_devices_infos_[0], true /* enable */);
  SetCapabilityEnabled(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
      test_eligible_external_devices_infos_[1], true /* enable */);
  SetCapabilityEnabled(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
      test_eligible_external_devices_infos_[2], true /* enable */);

  InvokeErrorCallback();
  EXPECT_EQ(kErrorOnToggleEasyUnlockResult, GetResultAndReset());

  InvokeSetCapabilityKeyUnlockCallback();
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  InvokeSetCapabilityKeyUnlockCallback();
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(DeviceCapabilityManagerImplTest,
       TestMultipleFindEligibleForUnlockDevicesRequests) {
  FindEligibleDevicesForCapability(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY);
  FindEligibleDevicesForCapability(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY);
  FindEligibleDevicesForCapability(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY);

  InvokeFindEligibleUnlockDevicesCallback(
      CreateFindEligibleUnlockDevicesResponse());
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  VerifyDeviceEligibility();

  InvokeErrorCallback();
  EXPECT_EQ(kErrorFindEligibleUnlockDevices, GetResultAndReset());

  InvokeFindEligibleUnlockDevicesCallback(
      CreateFindEligibleUnlockDevicesResponse());
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  VerifyDeviceEligibility();
}

TEST_F(DeviceCapabilityManagerImplTest, TestMultipleIsPromotableRequests) {
  IsPromotableDevice(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
      test_eligible_external_devices_infos_[0].public_key());
  IsPromotableDevice(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
      test_eligible_external_devices_infos_[1].public_key());
  IsPromotableDevice(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
      test_eligible_external_devices_infos_[2].public_key());

  InvokeFindEligibleForPromotionCallback(true /*eligible*/);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_TRUE(GetEligibleForPromotionAndReset());

  InvokeFindEligibleForPromotionCallback(true /*eligible*/);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_TRUE(GetEligibleForPromotionAndReset());

  InvokeErrorCallback();
  EXPECT_EQ(kErrorFindEligibleForPromotion, GetResultAndReset());
}

TEST_F(DeviceCapabilityManagerImplTest, TestOrderViaMultipleErrors) {
  SetCapabilityEnabled(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
      test_eligible_external_devices_infos_[0], true /* enable */);
  FindEligibleDevicesForCapability(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY);
  IsPromotableDevice(
      DeviceCapabilityManagerImpl::Capability::CAPABILITY_UNLOCK_KEY,
      test_eligible_external_devices_infos_[0].public_key());

  InvokeErrorCallback();
  EXPECT_EQ(kErrorOnToggleEasyUnlockResult, GetResultAndReset());

  InvokeErrorCallback();
  EXPECT_EQ(kErrorFindEligibleUnlockDevices, GetResultAndReset());

  InvokeErrorCallback();
  EXPECT_EQ(kErrorFindEligibleForPromotion, GetResultAndReset());
}

}  // namespace cryptauth