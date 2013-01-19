// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_scheduler.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/prefs/pref_service.h"
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
        "gdata/root_feed.json");
    fake_drive_service_->LoadAccountMetadataForWapi(
        "gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi(
        "drive/applist.json");

    scheduler_.reset(new DriveScheduler(profile_.get(),
                                        fake_drive_service_.get()));

    scheduler_->Initialize();
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

TEST_F(DriveSchedulerTest, GetAppList) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AppList> app_list;

  scheduler_->GetAppList(
      base::Bind(
          &google_apis::test_util::CopyResultsFromGetAppListCallback,
          &error,
          &app_list));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(app_list);
}

TEST_F(DriveSchedulerTest, GetAccountMetadata) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::AccountMetadataFeed> account_metadata;

  scheduler_->GetAccountMetadata(
      base::Bind(
          &google_apis::test_util::CopyResultsFromGetAccountMetadataCallback,
          &error,
          &account_metadata));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(account_metadata);
}


TEST_F(DriveSchedulerTest, GetResourceList) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceList> resource_list;

  scheduler_->GetResourceList(
      GURL("http://example.com/gdata/root_feed.json"),
      0,
      std::string(),
      true,
      std::string(),
      base::Bind(
          &google_apis::test_util::CopyResultsFromGetResourceListCallback,
          &error,
          &resource_list));
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
      base::Bind(
          &google_apis::test_util::CopyResultsFromGetResourceEntryCallback,
          &error,
          &entry));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(entry);
}

TEST_F(DriveSchedulerTest, DeleteResource) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->DeleteResource(
      GURL("https://file1_link_self/file:2_file_resource_id"),
      base::Bind(
          &google_apis::test_util::CopyResultsFromEntryActionCallback,
          &error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(DriveSchedulerTest, CopyHostedDocument) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;

  scheduler_->CopyHostedDocument(
      "document:5_document_resource_id",  // resource ID
      FILE_PATH_LITERAL("New Document"),  // new name
      base::Bind(
          &google_apis::test_util::CopyResultsFromGetResourceEntryCallback,
          &error,
          &entry));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(entry);
}

TEST_F(DriveSchedulerTest, RenameResource) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->RenameResource(
      GURL("https://file1_link_self/file:2_file_resource_id"),
      FILE_PATH_LITERAL("New Name"),
      base::Bind(
          &google_apis::test_util::CopyResultsFromEntryActionCallback,
          &error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(DriveSchedulerTest, AddResourceToDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->AddResourceToDirectory(
      GURL("https://dir_1_self_link/folder:1_folder_resource_id"),
      GURL("https://file1_link_self/file:2_file_resource_id"),
      base::Bind(
          &google_apis::test_util::CopyResultsFromEntryActionCallback,
          &error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(DriveSchedulerTest, RemoveResourceFromDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  scheduler_->RemoveResourceFromDirectory(
      GURL("https://dir_1_self_link/folder:1_folder_resource_id"),
      "file:subdirectory_file_1_id",  // resource ID
      base::Bind(
          &google_apis::test_util::CopyResultsFromEntryActionCallback,
          &error));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(DriveSchedulerTest, AddNewDirectory) {
  ConnectToWifi();

  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> entry;

  scheduler_->AddNewDirectory(
      GURL(),  // Root directory.
      FILE_PATH_LITERAL("New Directory"),
      base::Bind(
          &google_apis::test_util::CopyResultsFromGetResourceEntryCallback,
          &error,
          &entry));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(entry);
}

}  // namespace drive
