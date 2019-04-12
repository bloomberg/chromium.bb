// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/device_info/device_info_sync_bridge.h"
#include "components/sync/device_info/device_info_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/test_sync_service.h"
#include "content/public/browser/navigation_entry.h"
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

  void ForcePulseForTest() override {}

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

 protected:
  TestDeviceInfoTracker tracker_;
};

std::unique_ptr<KeyedService> BuildMockDeviceInfoSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestDeviceInfoSyncService>();
}

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

class SendTabToSelfUtilTest : public BrowserWithTestWindowTest {
 public:
  SendTabToSelfUtilTest() = default;
  ~SendTabToSelfUtilTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    test_sync_service_ = static_cast<syncer::TestSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildTestSyncService)));

    mock_device_sync_service_ = static_cast<TestDeviceInfoSyncService*>(
        DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockDeviceInfoSyncService)));

    incognito_profile_ = profile()->GetOffTheRecordProfile();
    url_ = GURL("https://www.google.com");
    title_ = base::UTF8ToUTF16(base::StringPiece("Google"));
  }

  // Set up all test conditions to let ShouldOfferFeature() return true
  void SetUpAllTrueEnv() {
    scoped_feature_list_.InitWithFeatures(
        {switches::kSyncSendTabToSelf, kSendTabToSelfShowSendingUI}, {});
    syncer::ModelTypeSet enabled_modeltype(syncer::SEND_TAB_TO_SELF);
    test_sync_service_->SetPreferredDataTypes(enabled_modeltype);

    mock_device_sync_service_->SetTrackerActiveDevices(2);

    AddTab(browser(), url_);
    NavigateAndCommitActiveTabWithTitle(browser(), url_, title_);
  }

  // Set up a environment in which the feature flag is disabled
  void SetUpFeatureDisabledEnv() {
    scoped_feature_list_.InitWithFeatures(
        {}, {switches::kSyncSendTabToSelf, kSendTabToSelfShowSendingUI});
    syncer::ModelTypeSet enabled_modeltype(syncer::SEND_TAB_TO_SELF);
    test_sync_service_->SetPreferredDataTypes(enabled_modeltype);

    mock_device_sync_service_->SetTrackerActiveDevices(2);

    AddTab(browser(), url_);
    NavigateAndCommitActiveTabWithTitle(browser(), url_, title_);
  }

 protected:
  syncer::TestSyncService* test_sync_service_;
  TestDeviceInfoSyncService* mock_device_sync_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  Profile* incognito_profile_;
  GURL url_;
  base::string16 title_;
};

TEST_F(SendTabToSelfUtilTest, AreFlagsEnabled_True) {
  scoped_feature_list_.InitWithFeatures(
      {switches::kSyncSendTabToSelf, kSendTabToSelfShowSendingUI}, {});

  EXPECT_TRUE(IsSendingEnabled());
  EXPECT_TRUE(IsReceivingEnabled());
}

TEST_F(SendTabToSelfUtilTest, AreFlagsEnabled_False) {
  scoped_feature_list_.InitWithFeatures(
      {}, {switches::kSyncSendTabToSelf, kSendTabToSelfShowSendingUI});

  EXPECT_FALSE(IsSendingEnabled());
  EXPECT_FALSE(IsReceivingEnabled());
}

TEST_F(SendTabToSelfUtilTest, IsReceivingEnabled_True) {
  scoped_feature_list_.InitWithFeatures({switches::kSyncSendTabToSelf},
                                        {kSendTabToSelfShowSendingUI});

  EXPECT_FALSE(IsSendingEnabled());
  EXPECT_TRUE(IsReceivingEnabled());
}

TEST_F(SendTabToSelfUtilTest, IsOnlySendingEnabled_False) {
  scoped_feature_list_.InitWithFeatures({kSendTabToSelfShowSendingUI},
                                        {switches::kSyncSendTabToSelf});

  EXPECT_FALSE(IsSendingEnabled());
  EXPECT_FALSE(IsReceivingEnabled());
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

}  // namespace

}  // namespace send_tab_to_self
