// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/job_scheduler.h"

#include <set>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

namespace {

class FakeNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  FakeNetworkChangeNotifier() : type_(CONNECTION_NONE) {}

  void set_connection_type(ConnectionType type) { type_ = type; }

  virtual ConnectionType GetCurrentConnectionType() const OVERRIDE {
    return type_;
  }

 private:
  net::NetworkChangeNotifier::ConnectionType type_;
};

void CopyResourceIdFromGetResourceEntryCallback(
    std::vector<std::string>* id_list_out,
    const std::string& requested_id,
    google_apis::GDataErrorCode error_in,
    scoped_ptr<google_apis::ResourceEntry> resource_entry_in) {
  id_list_out->push_back(requested_id);
}

class JobListLogger : public JobListObserver {
 public:
  enum EventType {
    ADDED,
    UPDATED,
    DONE,
  };

  struct EventLog {
    EventType type;
    JobInfo info;

    EventLog(EventType type, const JobInfo& info) : type(type), info(info) {
    }
  };

  // Checks whether the specified type of event has occurred.
  bool Has(EventType type, JobType job_type) {
    for (size_t i = 0; i < events.size(); ++i) {
      if (events[i].type == type && events[i].info.job_type == job_type)
        return true;
    }
    return false;
  }

  // Gets the progress event information of the specified type.
  void GetProgressInfo(JobType job_type, std::vector<int64>* progress) {
    for (size_t i = 0; i < events.size(); ++i) {
      if (events[i].type == UPDATED && events[i].info.job_type == job_type)
        progress->push_back(events[i].info.num_completed_bytes);
    }
  }

  // JobListObserver overrides.
  virtual void OnJobAdded(const JobInfo& info) OVERRIDE {
    events.push_back(EventLog(ADDED, info));
  }

  virtual void OnJobUpdated(const JobInfo& info) OVERRIDE {
    events.push_back(EventLog(UPDATED, info));
  }

  virtual void OnJobDone(const JobInfo& info, FileError error) OVERRIDE {
    events.push_back(EventLog(DONE, info));
  }

 private:
  std::vector<EventLog> events;
};

}  // namespace

class JobSchedulerTest : public testing::Test {
 public:
  JobSchedulerTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        profile_(new TestingProfile) {
  }

  virtual void SetUp() OVERRIDE {
    fake_network_change_notifier_.reset(new FakeNetworkChangeNotifier);

    fake_drive_service_.reset(new google_apis::FakeDriveService());
    fake_drive_service_->LoadResourceListForWapi(
        "chromeos/gdata/root_feed.json");
    fake_drive_service_->LoadAccountMetadataForWapi(
        "chromeos/gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi(
        "chromeos/drive/applist.json");

    scheduler_.reset(new JobScheduler(profile_.get(),
                                      fake_drive_service_.get()));
    scheduler_->SetDisableThrottling(true);
  }

  virtual void TearDown() OVERRIDE {
    // The scheduler should be deleted before NetworkLibrary, as it
    // registers itself as observer of NetworkLibrary.
    scheduler_.reset();
    google_apis::test_util::RunBlockingPoolTask();
    fake_drive_service_.reset();
    fake_network_change_notifier_.reset();
  }

 protected:
  // Sets up FakeNetworkChangeNotifier as if it's connected to a network with
  // the specified connection type.
  void ChangeConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    fake_network_change_notifier_->set_connection_type(type);
    // Notify the sync client that the network is changed. This is done via
    // NetworkChangeNotifier in production, but here, we simulate the behavior
    // by directly calling OnConnectionTypeChanged().
    scheduler_->OnConnectionTypeChanged(type);
  }

  // Sets up FakeNetworkChangeNotifier as if it's connected to wifi network.
  void ConnectToWifi() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  }

  // Sets up FakeNetworkChangeNotifier as if it's connected to cellular network.
  void ConnectToCellular() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_2G);
  }

  // Sets up FakeNetworkChangeNotifier as if it's connected to wimax network.
  void ConnectToWimax() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_4G);
  }

  // Sets up FakeNetworkChangeNotifier as if it's disconnected.
  void ConnectToNone() {
    ChangeConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<JobScheduler> scheduler_;
  scoped_ptr<FakeNetworkChangeNotifier> fake_network_change_notifier_;
  scoped_ptr<google_apis::FakeDriveService> fake_drive_service_;
};

TEST_F(JobSchedulerTest, GetAboutResource) {
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

TEST_F(JobSchedulerTest, GetAppList) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AppList> app_list;

  scheduler_->GetAppList(
      google_apis::test_util::CreateCopyResultCallback(&error, &app_list));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(app_list);
}

TEST_F(JobSchedulerTest, GetAccountMetadata) {
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

TEST_F(JobSchedulerTest, GetAllResourceList) {
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

TEST_F(JobSchedulerTest, GetResourceListInDirectory) {
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

TEST_F(JobSchedulerTest, Search) {
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

TEST_F(JobSchedulerTest, GetChangeList) {
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

TEST_F(JobSchedulerTest, ContinueGetResourceList) {
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

TEST_F(JobSchedulerTest, GetResourceEntry) {
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

TEST_F(JobSchedulerTest, DeleteResource) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->DeleteResource(
      "file:2_file_resource_id",
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(JobSchedulerTest, CopyHostedDocument) {
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

TEST_F(JobSchedulerTest, RenameResource) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->RenameResource(
      "file:2_file_resource_id",
      "New Name",
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(JobSchedulerTest, AddResourceToDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->AddResourceToDirectory(
      "folder:1_folder_resource_id",
      "file:2_file_resource_id",
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(JobSchedulerTest, RemoveResourceFromDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->RemoveResourceFromDirectory(
      "folder:1_folder_resource_id",
      "file:subdirectory_file_1_id",  // resource ID
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(JobSchedulerTest, AddNewDirectory) {
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

TEST_F(JobSchedulerTest, GetResourceEntryPriority) {
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
      DriveClientContext(BACKGROUND),
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
  ASSERT_EQ(resource_ids[2], resource_2);
  ASSERT_EQ(resource_ids[3], resource_3);
}

TEST_F(JobSchedulerTest, GetResourceEntryNoConnection) {
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

TEST_F(JobSchedulerTest, DownloadFileCellularDisabled) {
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
      base::FilePath::FromUTF8Unsafe("drive/whatever.txt"),  // virtual path
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

TEST_F(JobSchedulerTest, DownloadFileWimaxDisabled) {
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
      base::FilePath::FromUTF8Unsafe("drive/whatever.txt"),  // virtual path
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

TEST_F(JobSchedulerTest, DownloadFileCellularEnabled) {
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
      base::FilePath::FromUTF8Unsafe("drive/whatever.txt"),  // virtual path
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

TEST_F(JobSchedulerTest, DownloadFileWimaxEnabled) {
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
      base::FilePath::FromUTF8Unsafe("drive/whatever.txt"),  // virtual path
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

TEST_F(JobSchedulerTest, JobInfo) {
  JobListLogger logger;
  scheduler_->AddObserver(&logger);

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
      base::FilePath::FromUTF8Unsafe("drive/whatever.txt"),  // virtual path
      temp_dir.path().AppendASCII("whatever.txt"),
      GURL("https://file_content_url/"),
      DriveClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(&error, &path),
      google_apis::GetContentCallback());

  // The number of jobs queued so far.
  EXPECT_EQ(4U, scheduler_->GetJobInfoList().size());
  EXPECT_TRUE(logger.Has(JobListLogger::ADDED, TYPE_ADD_NEW_DIRECTORY));
  EXPECT_TRUE(logger.Has(JobListLogger::ADDED, TYPE_GET_ACCOUNT_METADATA));
  EXPECT_TRUE(logger.Has(JobListLogger::ADDED, TYPE_RENAME_RESOURCE));
  EXPECT_TRUE(logger.Has(JobListLogger::ADDED, TYPE_DOWNLOAD_FILE));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_ADD_NEW_DIRECTORY));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_GET_ACCOUNT_METADATA));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_RENAME_RESOURCE));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_DOWNLOAD_FILE));

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
  std::set<JobID> job_ids;
  for (size_t i = 0; i < jobs.size(); ++i) {
    actual_types.insert(jobs[i].job_type);
    job_ids.insert(jobs[i].job_id);
  }
  EXPECT_EQ(expected_types, actual_types);
  EXPECT_EQ(6U, job_ids.size()) << "All job IDs must be unique";
  EXPECT_TRUE(logger.Has(JobListLogger::ADDED, TYPE_ADD_RESOURCE_TO_DIRECTORY));
  EXPECT_TRUE(logger.Has(JobListLogger::ADDED, TYPE_COPY_HOSTED_DOCUMENT));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_ADD_RESOURCE_TO_DIRECTORY));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_COPY_HOSTED_DOCUMENT));

  // Run the jobs.
  google_apis::test_util::RunBlockingPoolTask();

  // All jobs except the BACKGROUND job should have started running (UPDATED)
  // and then finished (DONE).
  jobs = scheduler_->GetJobInfoList();
  ASSERT_EQ(1U, jobs.size());
  EXPECT_EQ(TYPE_DOWNLOAD_FILE, jobs[0].job_type);

  EXPECT_TRUE(logger.Has(JobListLogger::UPDATED, TYPE_ADD_NEW_DIRECTORY));
  EXPECT_TRUE(logger.Has(JobListLogger::UPDATED, TYPE_GET_ACCOUNT_METADATA));
  EXPECT_TRUE(logger.Has(JobListLogger::UPDATED, TYPE_RENAME_RESOURCE));
  EXPECT_TRUE(logger.Has(JobListLogger::UPDATED,
                         TYPE_ADD_RESOURCE_TO_DIRECTORY));
  EXPECT_TRUE(logger.Has(JobListLogger::UPDATED, TYPE_COPY_HOSTED_DOCUMENT));
  EXPECT_FALSE(logger.Has(JobListLogger::UPDATED, TYPE_DOWNLOAD_FILE));

  EXPECT_TRUE(logger.Has(JobListLogger::DONE, TYPE_ADD_NEW_DIRECTORY));
  EXPECT_TRUE(logger.Has(JobListLogger::DONE, TYPE_GET_ACCOUNT_METADATA));
  EXPECT_TRUE(logger.Has(JobListLogger::DONE, TYPE_RENAME_RESOURCE));
  EXPECT_TRUE(logger.Has(JobListLogger::DONE, TYPE_ADD_RESOURCE_TO_DIRECTORY));
  EXPECT_TRUE(logger.Has(JobListLogger::DONE, TYPE_COPY_HOSTED_DOCUMENT));
  EXPECT_FALSE(logger.Has(JobListLogger::DONE, TYPE_DOWNLOAD_FILE));

  // Run the background downloading job as well.
  ConnectToWifi();
  google_apis::test_util::RunBlockingPoolTask();

  // All jobs should have finished.
  EXPECT_EQ(0U, scheduler_->GetJobInfoList().size());
  EXPECT_TRUE(logger.Has(JobListLogger::UPDATED, TYPE_DOWNLOAD_FILE));
  EXPECT_TRUE(logger.Has(JobListLogger::DONE, TYPE_DOWNLOAD_FILE));
}


TEST_F(JobSchedulerTest, JobInfoProgress) {
  JobListLogger logger;
  scheduler_->AddObserver(&logger);

  ConnectToWifi();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  base::FilePath path;

  // Download job.
  scheduler_->DownloadFile(
      base::FilePath::FromUTF8Unsafe("drive/whatever.txt"),  // virtual path
      temp_dir.path().AppendASCII("whatever.txt"),
      GURL("https://file_content_url/"),
      DriveClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(&error, &path),
      google_apis::GetContentCallback());
  google_apis::test_util::RunBlockingPoolTask();

  std::vector<int64> download_progress;
  logger.GetProgressInfo(TYPE_DOWNLOAD_FILE, &download_progress);
  ASSERT_TRUE(!download_progress.empty());
  EXPECT_TRUE(base::STLIsSorted(download_progress));
  EXPECT_GE(download_progress.front(), 0);
  EXPECT_LE(download_progress.back(), 10);

  // Upload job.
  path = temp_dir.path().AppendASCII("new_file.txt");
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(path, "Hello"));
  google_apis::GDataErrorCode upload_error =
      google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;

  scheduler_->UploadNewFile(
      fake_drive_service_->GetRootResourceId(),
      base::FilePath::FromUTF8Unsafe("drive/new_file.txt"),
      path,
      "dummy title",
      "plain/plain",
      DriveClientContext(BACKGROUND),
      google_apis::test_util::CreateCopyResultCallback(
          &upload_error, &path, &path, &entry));
  google_apis::test_util::RunBlockingPoolTask();

  std::vector<int64> upload_progress;
  logger.GetProgressInfo(TYPE_UPLOAD_NEW_FILE, &upload_progress);
  ASSERT_TRUE(!upload_progress.empty());
  EXPECT_TRUE(base::STLIsSorted(upload_progress));
  EXPECT_GE(upload_progress.front(), 0);
  EXPECT_LE(upload_progress.back(), 5);
}

}  // namespace drive
