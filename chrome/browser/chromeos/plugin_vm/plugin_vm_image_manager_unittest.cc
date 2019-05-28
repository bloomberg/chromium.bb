// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_download_client.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "components/account_id/account_id.h"
#include "components/download/public/background_service/test/test_download_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

namespace {

using ::testing::_;

const char kProfileName[] = "p1";
const char kUrl[] = "http://example.com";
const char kPluginVmImageFile[] = "plugin_vm_image_file_1.zip";
const char kContent[] = "This is zipped content.";
const char kHash[] =
    "842841a4c75a55ad050d686f4ea5f77e83ae059877fe9b6946aa63d3d057ed32";
const char kHashUppercase[] =
    "842841A4C75A55AD050D686F4EA5F77E83AE059877FE9B6946AA63D3D057ED32";
const char kHash2[] =
    "02f06421ae27144aacdc598aebcd345a5e2e634405e8578300173628fe1574bd";
// File size set in test_download_service.
const int kDownloadedPluginVmImageSizeInMb = 123456789u / (1024 * 1024);

}  // namespace

class MockObserver : public PluginVmImageManager::Observer {
 public:
  MOCK_METHOD0(OnDownloadStarted, void());
  MOCK_METHOD3(OnDownloadProgressUpdated,
               void(uint64_t bytes_downloaded,
                    int64_t content_length,
                    int64_t download_bytes_per_sec));
  MOCK_METHOD0(OnDownloadCompleted, void());
  MOCK_METHOD0(OnDownloadCancelled, void());
  MOCK_METHOD0(OnDownloadFailed, void());
  MOCK_METHOD2(OnImportProgressUpdated,
               void(uint64_t percent_completed,
                    int64_t import_percent_per_second));
  MOCK_METHOD0(OnImported, void());
  MOCK_METHOD0(OnImportCancelled, void());
  MOCK_METHOD0(OnImportFailed, void());
};

class PluginVmImageManagerTest : public testing::Test {
 public:
  PluginVmImageManagerTest()
      : download_service_(new download::test::TestDownloadService()) {
    chromeos::DBusThreadManager::Initialize();
    fake_concierge_client_ = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
  }

  ~PluginVmImageManagerTest() override {
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<PluginVmTestHelper> plugin_vm_test_helper_;
  std::unique_ptr<TestingProfile> profile_;
  PluginVmImageManager* manager_;
  download::test::TestDownloadService* download_service_;
  std::unique_ptr<MockObserver> observer_;
  base::FilePath fake_downloaded_plugin_vm_image_archive_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  // Owned by chromeos::DBusThreadManager
  chromeos::FakeConciergeClient* fake_concierge_client_;

  void SetUp() override {
    ASSERT_TRUE(profiles_dir_.CreateUniqueTempDir());
    CreateProfile();
    plugin_vm_test_helper_ =
        std::make_unique<PluginVmTestHelper>(profile_.get());
    plugin_vm_test_helper_->AllowPluginVm();
    // Sets new PluginVmImage pref for this test.
    SetPluginVmImagePref(kUrl, kHash);

    manager_ = PluginVmImageManagerFactory::GetForProfile(profile_.get());
    download_service_->SetIsReady(true);
    download_service_->SetHash256(kHash);
    download_service_->set_client(
        new PluginVmImageDownloadClient(profile_.get()));
    manager_->SetDownloadServiceForTesting(download_service_);
    observer_ = std::make_unique<MockObserver>();
    manager_->SetObserver(observer_.get());

    fake_downloaded_plugin_vm_image_archive_ = CreateZipFile();

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    profile_.reset();
    observer_.reset();
  }

  void SetPluginVmImagePref(std::string url, std::string hash) {
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                plugin_vm::prefs::kPluginVmImage);
    base::DictionaryValue* plugin_vm_image = update.Get();
    plugin_vm_image->SetKey("url", base::Value(url));
    plugin_vm_image->SetKey("hash", base::Value(hash));
  }

  void ProcessImageUntilImporting() {
    manager_->StartDownload();
    test_browser_thread_bundle_.RunUntilIdle();
  }

  void ProcessImageUntilConfigured() {
    ProcessImageUntilImporting();

    // Faking downloaded file for testing.
    manager_->SetDownloadedPluginVmImageArchiveForTesting(
        fake_downloaded_plugin_vm_image_archive_);
    manager_->StartImport();
    test_browser_thread_bundle_.RunUntilIdle();
  }

  base::FilePath CreateZipFile() {
    base::FilePath zip_file_path =
        profile_->GetPath().AppendASCII(kPluginVmImageFile);
    base::WriteFile(zip_file_path, kContent, strlen(kContent));
    return zip_file_path;
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
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnImportProgressUpdated(50.0, _));
  EXPECT_CALL(*observer_, OnImported());

  manager_->StartDownload();

  std::string guid = manager_->GetCurrentDownloadGuidForTesting();
  base::Optional<download::DownloadParams> params =
      download_service_->GetDownload(guid);
  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(guid, params->guid);
  EXPECT_EQ(download::DownloadClient::PLUGIN_VM_IMAGE, params->client);
  EXPECT_EQ(GURL(kUrl), params->request_params.url);

  // Finishing image processing.
  test_browser_thread_bundle_.RunUntilIdle();
  // Faking downloaded file for testing.
  manager_->SetDownloadedPluginVmImageArchiveForTesting(
      fake_downloaded_plugin_vm_image_archive_);
  manager_->StartImport();
  test_browser_thread_bundle_.RunUntilIdle();
}

TEST_F(PluginVmImageManagerTest, OnlyOneImageIsProcessedTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnImportProgressUpdated(50.0, _));
  EXPECT_CALL(*observer_, OnImported());

  manager_->StartDownload();

  EXPECT_TRUE(manager_->IsProcessingImage());

  test_browser_thread_bundle_.RunUntilIdle();
  // Faking downloaded file for testing.
  manager_->SetDownloadedPluginVmImageArchiveForTesting(
      fake_downloaded_plugin_vm_image_archive_);

  EXPECT_TRUE(manager_->IsProcessingImage());

  manager_->StartImport();

  EXPECT_TRUE(manager_->IsProcessingImage());

  test_browser_thread_bundle_.RunUntilIdle();

  EXPECT_FALSE(manager_->IsProcessingImage());

  histogram_tester_->ExpectUniqueSample(kPluginVmImageDownloadedSizeHistogram,
                                        kDownloadedPluginVmImageSizeInMb, 1);
}

TEST_F(PluginVmImageManagerTest, CanProceedWithANewImageWhenSucceededTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  EXPECT_CALL(*observer_, OnDownloadCompleted()).Times(2);
  EXPECT_CALL(*observer_, OnImportProgressUpdated(50.0, _)).Times(2);
  EXPECT_CALL(*observer_, OnImported()).Times(2);

  ProcessImageUntilConfigured();

  EXPECT_FALSE(manager_->IsProcessingImage());

  // As it is deleted after successful importing.
  fake_downloaded_plugin_vm_image_archive_ = CreateZipFile();
  ProcessImageUntilConfigured();

  histogram_tester_->ExpectUniqueSample(kPluginVmImageDownloadedSizeHistogram,
                                        kDownloadedPluginVmImageSizeInMb, 2);
}

TEST_F(PluginVmImageManagerTest, CanProceedWithANewImageWhenFailedTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  EXPECT_CALL(*observer_, OnDownloadFailed());
  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnImportProgressUpdated(50.0, _));
  EXPECT_CALL(*observer_, OnImported());

  manager_->StartDownload();
  std::string guid = manager_->GetCurrentDownloadGuidForTesting();
  download_service_->SetFailedDownload(guid, false);
  test_browser_thread_bundle_.RunUntilIdle();

  EXPECT_FALSE(manager_->IsProcessingImage());

  ProcessImageUntilConfigured();

  histogram_tester_->ExpectUniqueSample(kPluginVmImageDownloadedSizeHistogram,
                                        kDownloadedPluginVmImageSizeInMb, 1);
}

TEST_F(PluginVmImageManagerTest, CancelledDownloadTest) {
  EXPECT_CALL(*observer_, OnDownloadCompleted()).Times(0);
  EXPECT_CALL(*observer_, OnDownloadCancelled());

  manager_->StartDownload();
  manager_->CancelDownload();
  test_browser_thread_bundle_.RunUntilIdle();
  // Finishing image processing as it should really happen.
  manager_->OnDownloadCancelled();

  histogram_tester_->ExpectTotalCount(kPluginVmImageDownloadedSizeHistogram, 0);
}

TEST_F(PluginVmImageManagerTest, ImportNonExistingImageTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnImportFailed());

  ProcessImageUntilImporting();
  // Should fail as fake downloaded file isn't set.
  manager_->StartImport();
  test_browser_thread_bundle_.RunUntilIdle();

  histogram_tester_->ExpectUniqueSample(kPluginVmImageDownloadedSizeHistogram,
                                        kDownloadedPluginVmImageSizeInMb, 1);
}

TEST_F(PluginVmImageManagerTest, CancelledImportTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);
  SetupConciergeForCancelDiskImageOperation(fake_concierge_client_,
                                            true /* success */);

  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnImportCancelled());

  ProcessImageUntilImporting();

  // Faking downloaded file for testing.
  manager_->SetDownloadedPluginVmImageArchiveForTesting(
      fake_downloaded_plugin_vm_image_archive_);
  manager_->StartImport();
  manager_->CancelImport();
  test_browser_thread_bundle_.RunUntilIdle();
}

TEST_F(PluginVmImageManagerTest, EmptyPluginVmImageUrlTest) {
  SetPluginVmImagePref("", kHash);
  EXPECT_CALL(*observer_, OnDownloadFailed());
  ProcessImageUntilImporting();

  histogram_tester_->ExpectTotalCount(kPluginVmImageDownloadedSizeHistogram, 0);
}

TEST_F(PluginVmImageManagerTest, VerifyDownloadTest) {
  EXPECT_FALSE(manager_->VerifyDownload(kHash2));
  EXPECT_TRUE(manager_->VerifyDownload(kHashUppercase));
  EXPECT_TRUE(manager_->VerifyDownload(kHash));
  EXPECT_FALSE(manager_->VerifyDownload(std::string()));
}

TEST_F(PluginVmImageManagerTest, CannotStartDownloadIfPluginVmGetsDisabled) {
  profile_->ScopedCrosSettingsTestHelper()->SetBoolean(
      chromeos::kPluginVmAllowed, false);
  EXPECT_CALL(*observer_, OnDownloadFailed());
  ProcessImageUntilImporting();
}

}  // namespace plugin_vm
