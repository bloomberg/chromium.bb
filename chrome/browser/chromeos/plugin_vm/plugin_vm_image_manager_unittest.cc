// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_download_client.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/background_service/test/test_download_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

namespace {

const char kProfileName[] = "p1";
const char kUrl[] = "http://example.com";
const char kHash[] =
    "842841a4c75a55ad050d686f4ea5f77e83ae059877fe9b6946aa63d3d057ed32";

}  // namespace

class MockObserver : public PluginVmImageManager::Observer {
 public:
  MOCK_METHOD0(OnDownloadStarted, void());
  MOCK_METHOD1(OnProgressUpdated,
               void(base::Optional<double> fraction_complete));
  MOCK_METHOD1(OnDownloadCompleted, void(base::FilePath file_path));
  MOCK_METHOD0(OnDownloadCancelled, void());
  MOCK_METHOD0(OnDownloadFailed, void());
};

class PluginVmImageManagerTest : public testing::Test {
 public:
  PluginVmImageManagerTest()
      : download_service_(new download::test::TestDownloadService()) {}

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  PluginVmImageManager* manager_;
  download::test::TestDownloadService* download_service_;
  std::unique_ptr<MockObserver> download_observer_;

  void SetUp() override {
    ASSERT_TRUE(profiles_dir_.CreateUniqueTempDir());
    CreateProfile();
    SetPluginVmImagePref(kUrl, kHash);

    manager_ = PluginVmImageManagerFactory::GetForProfile(profile_.get());
    download_service_->SetIsReady(true);
    download_service_->set_client(
        new PluginVmImageDownloadClient(profile_.get()));
    manager_->SetDownloadServiceForTesting(download_service_);
    download_observer_ = std::make_unique<MockObserver>();
    manager_->SetObserver(download_observer_.get());
  }

  void TearDown() override {
    profile_.reset();
    download_observer_.reset();
  }

  void SetPluginVmImagePref(std::string url, std::string hash) {
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                plugin_vm::prefs::kPluginVmImage);
    base::DictionaryValue* plugin_vm_image = update.Get();
    plugin_vm_image->SetKey("url", base::Value(url));
    plugin_vm_image->SetKey("hash", base::Value(hash));
  }

 private:
  base::ScopedTempDir profiles_dir_;

  void CreateProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(kProfileName);
    profile_builder.SetPath(profiles_dir_.GetPath().AppendASCII(kProfileName));
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(pref_service->registry());
    profile_builder.SetPrefService(std::move(pref_service));
    profile_ = profile_builder.Build();
  }

  DISALLOW_COPY_AND_ASSIGN(PluginVmImageManagerTest);
};

TEST_F(PluginVmImageManagerTest, DownloadPluginVmImageParamsTest) {
  EXPECT_CALL(*download_observer_, OnDownloadCompleted(testing::_));

  manager_->StartDownload();

  std::string guid = manager_->GetCurrentDownloadGuidForTesting();
  base::Optional<download::DownloadParams> params =
      download_service_->GetDownload(guid);
  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(guid, params->guid);
  EXPECT_EQ(download::DownloadClient::PLUGIN_VM_IMAGE, params->client);
  EXPECT_EQ(GURL(kUrl), params->request_params.url);

  base::RunLoop().RunUntilIdle();
}

TEST_F(PluginVmImageManagerTest, OnlyOneDownloadAtATimeIsAllowedTest) {
  EXPECT_CALL(*download_observer_, OnDownloadCompleted(testing::_)).Times(1);
  EXPECT_CALL(*download_observer_, OnDownloadFailed()).Times(1);

  manager_->StartDownload();

  EXPECT_TRUE(manager_->IsDownloading());

  manager_->StartDownload();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PluginVmImageManagerTest, CanStartSecondDownloadWhenSucceededTest) {
  EXPECT_CALL(*download_observer_, OnDownloadCompleted(testing::_)).Times(2);

  manager_->StartDownload();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(manager_->IsDownloading());

  manager_->StartDownload();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PluginVmImageManagerTest, CanStartSecondDownloadWhenFailedTest) {
  EXPECT_CALL(*download_observer_, OnDownloadFailed()).Times(1);
  EXPECT_CALL(*download_observer_, OnDownloadCompleted(testing::_)).Times(1);

  manager_->StartDownload();
  std::string guid = manager_->GetCurrentDownloadGuidForTesting();
  download_service_->SetFailedDownload(guid, false);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(manager_->IsDownloading());

  manager_->StartDownload();
  base::RunLoop().RunUntilIdle();
}

TEST_F(PluginVmImageManagerTest, CancelledDownloadTest) {
  EXPECT_CALL(*download_observer_, OnDownloadCompleted(testing::_)).Times(0);

  manager_->StartDownload();
  manager_->CancelDownload();
  base::RunLoop().RunUntilIdle();
}

}  // namespace plugin_vm
