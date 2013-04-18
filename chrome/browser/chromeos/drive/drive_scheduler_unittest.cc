// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_scheduler.h"

#include <set>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/prefs/pref_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace drive {

namespace {

class MockNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  MOCK_CONST_METHOD0(GetCurrentConnectionType,
                     net::NetworkChangeNotifier::ConnectionType());
};

void CopyResourceIdFromGetResourceEntryCallback(
    std::vector<std::string>* id_list_out,
    const std::string& requested_id,
    google_apis::GDataErrorCode error_in,
    scoped_ptr<google_apis::ResourceEntry> resource_entry_in) {
  id_list_out->push_back(requested_id);
}

}  // namespace

class DriveSchedulerTest : public testing::Test {
 public:
  DriveSchedulerTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        profile_(new TestingProfile) {
  }

  virtual void SetUp() OVERRIDE {
    mock_network_change_notifier_.reset(new MockNetworkChangeNotifier);

    fake_drive_service_.reset(new google_apis::FakeDriveService());
    fake_drive_service_->LoadResourceListForWapi(
        "chromeos/gdata/root_feed.json");
    fake_drive_service_->LoadAccountMetadataForWapi(
        "chromeos/gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi(
        "chromeos/drive/applist.json");

    scheduler_.reset(new DriveScheduler(profile_.get(),
                                        fake_drive_service_.get()));
    scheduler_->SetDisableThrottling(true);
  }

  virtual void TearDown() OVERRIDE {
    // The scheduler should be deleted before NetworkLibrary, as it
    // registers itself as observer of NetworkLibrary.
    scheduler_.reset();
    google_apis::test_util::RunBlockingPoolTask();
    fake_drive_service_.reset();
    mock_network_change_notifier_.reset();
  }

  // Sets up MockNetworkChangeNotifier as if it's connected to a network with
  // the specified connection type.
  void ChangeConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    EXPECT_CALL(*mock_network_change_notifier_, GetCurrentConnectionType())
        .WillRepeatedly(Return(type));
    // Notify the sync client that the network is changed. This is done via
    // NetworkChangeNotifier in production, but here, we simulate the behavior
    // by directly calling OnConnectionTypeChanged().
    scheduler_->OnConnectionTypeChanged(type);
  }

  // Sets up MockNetworkChangeNotifier as if it's connected to wifi network.
  void ConnectToWifi() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  }

  // Sets up MockNetworkChangeNotifier as if it's connected to cellular network.
  void ConnectToCellular() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_2G);
  }

  // Sets up MockNetworkChangeNotifier as if it's connected to wimax network.
  void ConnectToWimax() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_4G);
  }

  // Sets up MockNetworkChangeNotifier as if it's disconnected.
  void ConnectToNone() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<DriveScheduler> scheduler_;
  scoped_ptr<MockNetworkChangeNotifier> mock_network_change_notifier_;
  scoped_ptr<google_apis::FakeDriveService> fake_drive_service_;
};

TEST_F(DriveSchedulerTest, GetAboutResource) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AboutResource> about_resource;
  scheduler_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(
          &error, &about_resource));
  google_apis::test_util::RunBlockingPoolTask();
  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(about_resource);
}

TEST_F(DriveSchedulerTest, GetAppList) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AppList> app_list;

  scheduler_->GetAppList(
      google_apis::test_util::CreateCopyResultCallback(&error, &app_list));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(app_list);
}

TEST_F(DriveSchedulerTest, GetAccountMetadata) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AccountMetadata> account_metadata;

  scheduler_->GetAccountMetadata(
      google_apis::test_util::CreateCopyResultCallback(
          &error, &account_metadata));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(account_metadata);
}

TEST_F(DriveSchedulerTest, GetAllResourceList) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;

  scheduler_->GetAllResourceList(
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
}

TEST_F(DriveSchedulerTest, GetResourceListInDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;

  scheduler_->GetResourceListInDirectory(
      fake_drive_service_->GetRootResourceId(),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
}

TEST_F(DriveSchedulerTest, Search) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;

  scheduler_->Search(
      "File",  // search query
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
}

TEST_F(DriveSchedulerTest, GetChangeList) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  // Create a new directory.
  // The loaded (initial) changestamp is 654321. Thus, by this operation,
  // it should become 654322.
  {
    scoped_ptr<google_apis::ResourceEntry> resource_entry;
    fake_drive_service_->AddNewDirectory(
        fake_drive_service_->GetRootResourceId(),
        "new directory",
        google_apis::test_util::CreateCopyResultCallback(
            &error, &resource_entry));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
  }

  error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;
  scheduler_->GetChangeList(
      654321 + 1,  // start_changestamp
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
}

TEST_F(DriveSchedulerTest, ContinueGetResourceList) {
  ConnectToWifi();
  fake_drive_service_->set_default_max_results(2);

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;

  scheduler_->GetAllResourceList(
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  const google_apis::Link* next_link =
      resource_list->GetLinkByType(google_apis::Link::LINK_NEXT);
  ASSERT_TRUE(next_link);
  // Keep the next url before releasing the |resource_list|.
  GURL next_url(next_link->href());

  error = google_apis::GDATA_OTHER_ERROR;
  resource_list.reset();

  scheduler_->ContinueGetResourceList(
      next_url,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_list));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
}

TEST_F(DriveSchedulerTest, GetResourceEntry) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;

  scheduler_->GetResourceEntry(
      "file:2_file_resource_id",  // resource ID
      DriveClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(entry);
}

TEST_F(DriveSchedulerTest, DeleteResource) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->DeleteResource(
      "file:2_file_resource_id",
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(DriveSchedulerTest, CopyHostedDocument) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;

  scheduler_->CopyHostedDocument(
      "document:5_document_resource_id",  // resource ID
      "New Document",  // new name
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(entry);
}

TEST_F(DriveSchedulerTest, RenameResource) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->RenameResource(
      "file:2_file_resource_id",
      "New Name",
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(DriveSchedulerTest, AddResourceToDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->AddResourceToDirectory(
      "folder:1_folder_resource_id",
      "file:2_file_resource_id",
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(DriveSchedulerTest, RemoveResourceFromDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->RemoveResourceFromDirectory(
      "folder:1_folder_resource_id",
      "file:subdirectory_file_1_id",  // resource ID
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(DriveSchedulerTest, AddNewDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;

  scheduler_->AddNewDirectory(
      fake_drive_service_->GetRootResourceId(),  // Root directory.
      "New Directory",
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_CREATED, error);
  ASSERT_TRUE(entry);
}

TEST_F(DriveSchedulerTest, GetResourceEntryPriority) {
  // Disconnect from the network to prevent jobs from starting.
  ConnectToNone();

  std::string resource_1("file:1_file_resource_id");
  std::string resource_2("file:2_file_resource_id");
  std::string resource_3("file:3_file_resource_id");
  std::string resource_4("file:4_file_resource_id");
  std::vector<std::string> resource_ids;

  scheduler_->GetResourceEntry(
      resource_1,  // resource ID
      DriveClientContext(USER_INITIATED),
      base::Bind(&CopyResourceIdFromGetResourceEntryCallback,
                 &resource_ids,
                 resource_1));
  scheduler_->GetResourceEntry(
      resource_2,  // resource ID
      DriveClientContext(PREFETCH),
      base::Bind(&CopyResourceIdFromGetResourceEntryCallback,
                 &resource_ids,
                 resource_2));
  scheduler_->GetResourceEntry(
      resource_3,  // resource ID
      DriveClientContext(BACKGROUND),
      base::Bind(&CopyResourceIdFromGetResourceEntryCallback,
                 &resource_ids,
                 resource_3));
  scheduler_->GetResourceEntry(
      resource_4,  // resource ID
      DriveClientContext(USER_INITIATED),
      base::Bind(&CopyResourceIdFromGetResourceEntryCallback,
                 &resource_ids,
                 resource_4));

  // Reconnect to the network to start all jobs.
  ConnectToWifi();
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(resource_ids.size(), 4ul);
  ASSERT_EQ(resource_ids[0], resource_1);
  ASSERT_EQ(resource_ids[1], resource_4);
  ASSERT_EQ(resource_ids[2], resource_3);
  ASSERT_EQ(resource_ids[3], resource_2);
}

TEST_F(DriveSchedulerTest, GetResourceEntryNoConnection) {
  ConnectToNone();

  std::string resource("file:1_file_resource_id");
  std::vector<std::string> resource_ids;

  scheduler_->GetResourceEntry(
      resource,  // resource ID
      DriveClientContext(BACKGROUND),
      base::Bind(&CopyResourceIdFromGetResourceEntryCallback,
                 &resource_ids,
                 resource));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(resource_ids.size(), 0ul);

  // Reconnect to the net.
  ConnectToWifi();

  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(resource_ids.size(), 1ul);
  ASSERT_EQ(resource_ids[0], resource);
}

TEST_F(DriveSchedulerTest, DownloadFileCellularDisabled) {
  ConnectToCellular();

  // Disable fetching over cellular network.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDriveOverCellular, true);

  // Try to get a file in the background
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const GURL kContentUrl("https://file_content_url/");
  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  google_apis::GDataErrorCode download_error = google_apis::GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  scheduler_->DownloadFile(
      base::FilePath::FromUTF8Unsafe("/drive/whatever.txt"),  // virtual path
      kOutputFilePath,
      kContentUrl,
      DriveClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(
          &download_error, &output_file_path),
      google_apis::GetContentCallback());
  // Metadata should still work
  google_apis::GDataErrorCode metadata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AccountMetadata> account_metadata;

  // Try to get the metadata
  scheduler_->GetAccountMetadata(
      google_apis::test_util::CreateCopyResultCallback(
          &metadata_error, &account_metadata));
  google_apis::test_util::RunBlockingPoolTask();

  // Check the metadata
  ASSERT_EQ(google_apis::HTTP_SUCCESS, metadata_error);
  ASSERT_TRUE(account_metadata);

  // Check the download
  EXPECT_EQ(google_apis::GDATA_OTHER_ERROR, download_error);

  // Switch to a Wifi connection
  ConnectToWifi();

  google_apis::test_util::RunBlockingPoolTask();

  // Check the download again
  EXPECT_EQ(google_apis::HTTP_SUCCESS, download_error);
  std::string content;
  EXPECT_EQ(output_file_path, kOutputFilePath);
  ASSERT_TRUE(file_util::ReadFileToString(output_file_path, &content));
  // The content is "x"s of the file size specified in root_feed.json.
  EXPECT_EQ("xxxxxxxxxx", content);
}

TEST_F(DriveSchedulerTest, DownloadFileWimaxDisabled) {
  ConnectToWimax();

  // Disable fetching over cellular network.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDriveOverCellular, true);

  // Try to get a file in the background
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const GURL kContentUrl("https://file_content_url/");
  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  google_apis::GDataErrorCode download_error = google_apis::GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  scheduler_->DownloadFile(
      base::FilePath::FromUTF8Unsafe("/drive/whatever.txt"),  // virtual path
      kOutputFilePath,
      kContentUrl,
      DriveClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(
          &download_error, &output_file_path),
      google_apis::GetContentCallback());
  // Metadata should still work
  google_apis::GDataErrorCode metadata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AccountMetadata> account_metadata;

  // Try to get the metadata
  scheduler_->GetAccountMetadata(
      google_apis::test_util::CreateCopyResultCallback(
          &metadata_error, &account_metadata));
  google_apis::test_util::RunBlockingPoolTask();

  // Check the metadata
  ASSERT_EQ(google_apis::HTTP_SUCCESS, metadata_error);
  ASSERT_TRUE(account_metadata);

  // Check the download
  EXPECT_EQ(google_apis::GDATA_OTHER_ERROR, download_error);

  // Switch to a Wifi connection
  ConnectToWifi();

  google_apis::test_util::RunBlockingPoolTask();

  // Check the download again
  EXPECT_EQ(google_apis::HTTP_SUCCESS, download_error);
  std::string content;
  EXPECT_EQ(output_file_path, kOutputFilePath);
  ASSERT_TRUE(file_util::ReadFileToString(output_file_path, &content));
  // The content is "x"s of the file size specified in root_feed.json.
  EXPECT_EQ("xxxxxxxxxx", content);
}

TEST_F(DriveSchedulerTest, DownloadFileCellularEnabled) {
  ConnectToCellular();

  // Enable fetching over cellular network.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDriveOverCellular, false);

  // Try to get a file in the background
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const GURL kContentUrl("https://file_content_url/");
  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  google_apis::GDataErrorCode download_error = google_apis::GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  scheduler_->DownloadFile(
      base::FilePath::FromUTF8Unsafe("/drive/whatever.txt"),  // virtual path
      kOutputFilePath,
      kContentUrl,
      DriveClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(
          &download_error, &output_file_path),
      google_apis::GetContentCallback());
  // Metadata should still work
  google_apis::GDataErrorCode metadata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AccountMetadata> account_metadata;

  // Try to get the metadata
  scheduler_->GetAccountMetadata(
      google_apis::test_util::CreateCopyResultCallback(
          &metadata_error, &account_metadata));
  google_apis::test_util::RunBlockingPoolTask();

  // Check the metadata
  ASSERT_EQ(google_apis::HTTP_SUCCESS, metadata_error);
  ASSERT_TRUE(account_metadata);

  // Check the download
  EXPECT_EQ(google_apis::HTTP_SUCCESS, download_error);
  std::string content;
  EXPECT_EQ(output_file_path, kOutputFilePath);
  ASSERT_TRUE(file_util::ReadFileToString(output_file_path, &content));
  // The content is "x"s of the file size specified in root_feed.json.
  EXPECT_EQ("xxxxxxxxxx", content);
}

TEST_F(DriveSchedulerTest, DownloadFileWimaxEnabled) {
  ConnectToWimax();

  // Enable fetching over cellular network.
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDriveOverCellular, false);

  // Try to get a file in the background
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const GURL kContentUrl("https://file_content_url/");
  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  google_apis::GDataErrorCode download_error = google_apis::GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  scheduler_->DownloadFile(
      base::FilePath::FromUTF8Unsafe("/drive/whatever.txt"),  // virtual path
      kOutputFilePath,
      kContentUrl,
      DriveClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(
          &download_error, &output_file_path),
      google_apis::GetContentCallback());
  // Metadata should still work
  google_apis::GDataErrorCode metadata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AccountMetadata> account_metadata;

  // Try to get the metadata
  scheduler_->GetAccountMetadata(
      google_apis::test_util::CreateCopyResultCallback(
          &metadata_error, &account_metadata));
  google_apis::test_util::RunBlockingPoolTask();

  // Check the metadata
  ASSERT_EQ(google_apis::HTTP_SUCCESS, metadata_error);
  ASSERT_TRUE(account_metadata);

  // Check the download
  EXPECT_EQ(google_apis::HTTP_SUCCESS, download_error);
  std::string content;
  EXPECT_EQ(output_file_path, kOutputFilePath);
  ASSERT_TRUE(file_util::ReadFileToString(output_file_path, &content));
  // The content is "x"s of the file size specified in root_feed.json.
  EXPECT_EQ("xxxxxxxxxx", content);
}

TEST_F(DriveSchedulerTest, JobInfo) {
  // Disable background upload/download.
  ConnectToWimax();
  profile_->GetPrefs()->SetBoolean(prefs::kDisableDriveOverCellular, true);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;
  scoped_ptr<google_apis::AccountMetadata> account_metadata;
  base::FilePath path;

  std::set<JobType> expected_types;

  // Add many jobs.
  expected_types.insert(TYPE_ADD_NEW_DIRECTORY);
  scheduler_->AddNewDirectory(
      fake_drive_service_->GetRootResourceId(),
      "New Directory",
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  expected_types.insert(TYPE_GET_ACCOUNT_METADATA);
  scheduler_->GetAccountMetadata(
      google_apis::test_util::CreateCopyResultCallback(
          &error, &account_metadata));
  expected_types.insert(TYPE_RENAME_RESOURCE);
  scheduler_->RenameResource(
      "file:2_file_resource_id",
      "New Name",
      google_apis::test_util::CreateCopyResultCallback(&error));
  expected_types.insert(TYPE_DOWNLOAD_FILE);
  scheduler_->DownloadFile(
      base::FilePath::FromUTF8Unsafe("/drive/whatever.txt"),  // virtual path
      temp_dir.path().AppendASCII("whatever.txt"),
      GURL("https://file_content_url/"),
      DriveClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(&error, &path),
      google_apis::GetContentCallback());

  // The number of jobs queued so far.
  EXPECT_EQ(4U, scheduler_->GetJobInfoList().size());

  // Add more jobs.
  expected_types.insert(TYPE_ADD_RESOURCE_TO_DIRECTORY);
  scheduler_->AddResourceToDirectory(
      "folder:1_folder_resource_id",
      "file:2_file_resource_id",
      google_apis::test_util::CreateCopyResultCallback(&error));
  expected_types.insert(TYPE_COPY_HOSTED_DOCUMENT);
  scheduler_->CopyHostedDocument(
      "document:5_document_resource_id",
      "New Document",
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));

  // 6 jobs in total were queued.
  std::vector<JobInfo> jobs = scheduler_->GetJobInfoList();
  EXPECT_EQ(6U, jobs.size());
  std::set<JobType> actual_types;
  for (size_t i = 0; i < jobs.size(); ++i)
    actual_types.insert(jobs[i].job_type);
  EXPECT_EQ(expected_types, actual_types);

  // Run the jobs.
  google_apis::test_util::RunBlockingPoolTask();

  // All jobs except the BACKGROUND job should have finished.
  jobs = scheduler_->GetJobInfoList();
  ASSERT_EQ(1U, jobs.size());
  EXPECT_EQ(TYPE_DOWNLOAD_FILE, jobs[0].job_type);

  // Run the background downloading job as well.
  ConnectToWifi();
  google_apis::test_util::RunBlockingPoolTask();

  // All jobs should have finished.
  EXPECT_EQ(0U, scheduler_->GetJobInfoList().size());
}

}  // namespace drive
