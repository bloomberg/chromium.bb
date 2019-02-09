// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/device_info/device_info_sync_bridge.h"
#include "components/sync/device_info/device_info_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

// Mock DeviceInfoTracker class for setting active devices
class TestDeviceInfoTracker : public syncer::DeviceInfoTracker {
 public:
  TestDeviceInfoTracker() = default;
  ~TestDeviceInfoTracker() override = default;

  void SetActiveDevices(int devices) { active_devices_ = devices; }

  // DeviceInfoTracker implementation
  bool IsSyncing() const override { return false; }
  std::unique_ptr<syncer::DeviceInfo> GetDeviceInfo(
      const std::string& client_id) const override {
    return std::unique_ptr<syncer::DeviceInfo>();
  }
  std::vector<std::unique_ptr<syncer::DeviceInfo>> GetAllDeviceInfo()
      const override {
    return std::vector<std::unique_ptr<syncer::DeviceInfo>>();
  }
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  int CountActiveDevices() const override { return active_devices_; }

 protected:
  int active_devices_;
};

// Mock DeviceInfoSyncService to host mocked DeviceInfoTracker
class TestDeviceInfoSyncService : public syncer::DeviceInfoSyncService {
 public:
  TestDeviceInfoSyncService() = default;
  ~TestDeviceInfoSyncService() override = default;

  TestDeviceInfoTracker* GetMockDeviceInfoTracker() { return &tracker_; }
  void SetTrackerActiveDevices(int devices) {
    tracker_.SetActiveDevices(devices);
  }

  // DeviceInfoSyncService implementation
  syncer::LocalDeviceInfoProvider* GetLocalDeviceInfoProvider() override {
    return nullptr;
  }
  syncer::DeviceInfoTracker* GetDeviceInfoTracker() override {
    return &tracker_;
  }
  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override {
    return nullptr;
  }
  void InitLocalCacheGuid(const std::string& cache_guid,
                          const std::string& session_name) override {}
  void ClearLocalCacheGuid() override {}

 protected:
  TestDeviceInfoTracker tracker_;
};

std::unique_ptr<KeyedService> BuildMockDeviceInfoSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestDeviceInfoSyncService>();
}

class SendTabToSelfUtilTest : public BrowserWithTestWindowTest {
 public:
  SendTabToSelfUtilTest() = default;
  ~SendTabToSelfUtilTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    mock_profile_sync_service_ =
        static_cast<browser_sync::ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
                profile(), base::BindRepeating(&BuildMockProfileSyncService)));
    mock_device_sync_service_ = static_cast<TestDeviceInfoSyncService*>(
        DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockDeviceInfoSyncService)));

    incognito_profile_ = profile()->GetOffTheRecordProfile();
    url_ = GURL("https://www.google.com");
  }

  // Set up all test conditions to let ShouldOfferFeature() return true
  void SetUpAllTrueEnv() {
    scoped_feature_list_.InitAndEnableFeature(switches::kSyncSendTabToSelf);
    syncer::ModelTypeSet enabled_modeltype(syncer::SEND_TAB_TO_SELF);
    EXPECT_CALL(*mock_profile_sync_service_->GetUserSettingsMock(),
                GetChosenDataTypes())
        .WillRepeatedly(testing::Return(enabled_modeltype));

    mock_device_sync_service_->SetTrackerActiveDevices(2);

    AddTab(browser(), url_);
    NavigateAndCommitActiveTab(url_);
  }

  // Set up a environment in which the feature flag is disabled
  void SetUpFeatureDisabledEnv() {
    scoped_feature_list_.InitAndDisableFeature(switches::kSyncSendTabToSelf);
    syncer::ModelTypeSet enabled_modeltype(syncer::SEND_TAB_TO_SELF);
    EXPECT_CALL(*mock_profile_sync_service_->GetUserSettingsMock(),
                GetChosenDataTypes())
        .WillRepeatedly(testing::Return(enabled_modeltype));

    mock_device_sync_service_->SetTrackerActiveDevices(2);

    AddTab(browser(), url_);
    NavigateAndCommitActiveTab(url_);
  }

 protected:
  browser_sync::ProfileSyncServiceMock* mock_profile_sync_service_;
  TestDeviceInfoSyncService* mock_device_sync_service_;

  base::test::ScopedFeatureList scoped_feature_list_;
  Profile* incognito_profile_;
  GURL url_;
};

TEST_F(SendTabToSelfUtilTest, IsFlagEnabled_True) {
  scoped_feature_list_.InitAndEnableFeature(switches::kSyncSendTabToSelf);

  EXPECT_TRUE(IsFlagEnabled());
}

TEST_F(SendTabToSelfUtilTest, IsFlagEnabled_False) {
  scoped_feature_list_.InitAndDisableFeature(switches::kSyncSendTabToSelf);
  EXPECT_FALSE(IsFlagEnabled());
}

TEST_F(SendTabToSelfUtilTest, IsUserSyncTypeEnabled_True) {
  syncer::ModelTypeSet enabled_modeltype(syncer::SEND_TAB_TO_SELF);
  EXPECT_CALL(*mock_profile_sync_service_->GetUserSettingsMock(),
              GetChosenDataTypes())
      .WillRepeatedly(testing::Return(enabled_modeltype));

  EXPECT_TRUE(IsUserSyncTypeEnabled(profile()));

  EXPECT_CALL(*mock_profile_sync_service_->GetUserSettingsMock(),
              GetChosenDataTypes())
      .WillRepeatedly(testing::Return(syncer::ModelTypeSet::All()));

  EXPECT_TRUE(IsUserSyncTypeEnabled(profile()));
}

TEST_F(SendTabToSelfUtilTest, IsUserSyncTypeEnabled_False) {
  syncer::ModelTypeSet disabled_modeltype;
  EXPECT_CALL(*mock_profile_sync_service_->GetUserSettingsMock(),
              GetChosenDataTypes())
      .WillRepeatedly(testing::Return(disabled_modeltype));

  EXPECT_FALSE(IsUserSyncTypeEnabled(profile()));
}

TEST_F(SendTabToSelfUtilTest, IsSyncingOnMultipleDevices_True) {
  mock_device_sync_service_->SetTrackerActiveDevices(2);

  EXPECT_TRUE(IsSyncingOnMultipleDevices(profile()));
}

TEST_F(SendTabToSelfUtilTest, IsSyncingOnMultipleDevices_False) {
  mock_device_sync_service_->SetTrackerActiveDevices(0);

  EXPECT_FALSE(IsSyncingOnMultipleDevices(profile()));
}

TEST_F(SendTabToSelfUtilTest, ContentRequirementsMet) {
  EXPECT_TRUE(IsContentRequirementsMet(url_, profile()));
}

TEST_F(SendTabToSelfUtilTest, NotHTTPOrHTTPS) {
  url_ = GURL("192.168.0.0");
  EXPECT_FALSE(IsContentRequirementsMet(url_, profile()));
}

TEST_F(SendTabToSelfUtilTest, NativePage) {
  url_ = GURL("chrome://flags");
  EXPECT_FALSE(IsContentRequirementsMet(url_, profile()));
}

TEST_F(SendTabToSelfUtilTest, IncognitoMode) {
  EXPECT_FALSE(IsContentRequirementsMet(url_, incognito_profile_));
}

TEST_F(SendTabToSelfUtilTest, ShouldOfferFeature_True) {
  SetUpAllTrueEnv();

  EXPECT_TRUE(ShouldOfferFeature(browser()));
}

TEST_F(SendTabToSelfUtilTest, ShouldOfferFeature_IsFlagEnabled_False) {
  SetUpFeatureDisabledEnv();
  EXPECT_FALSE(ShouldOfferFeature(browser()));
}

TEST_F(SendTabToSelfUtilTest, ShouldOfferFeature_IsUserSyncTypeEnabled_False) {
  SetUpAllTrueEnv();
  syncer::ModelTypeSet disabled_modeltype;
  EXPECT_CALL(*mock_profile_sync_service_->GetUserSettingsMock(),
              GetChosenDataTypes())
      .WillRepeatedly(testing::Return(disabled_modeltype));

  EXPECT_FALSE(ShouldOfferFeature(browser()));
}

TEST_F(SendTabToSelfUtilTest,
       ShouldOfferFeature_IsSyncingOnMultipleDevices_False) {
  SetUpAllTrueEnv();
  mock_device_sync_service_->SetTrackerActiveDevices(0);

  EXPECT_FALSE(ShouldOfferFeature(browser()));
}

TEST_F(SendTabToSelfUtilTest,
       ShouldOfferFeature_IsContentRequirementsMet_False) {
  SetUpAllTrueEnv();
  url_ = GURL("192.168.0.0");
  NavigateAndCommitActiveTab(url_);

  EXPECT_FALSE(ShouldOfferFeature(browser()));
}

}  // namespace

}  // namespace send_tab_to_self
