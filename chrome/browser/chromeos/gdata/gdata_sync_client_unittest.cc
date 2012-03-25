// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_sync_client.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/chromeos/gdata/mock_gdata_file_system.h"
#include "chrome/common/chrome_paths.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::_;

namespace gdata {

class GDataSyncClientTest : public testing::Test {
 public:
  GDataSyncClientTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        mock_file_system_(new MockGDataFileSystem),
        sync_client_(new GDataSyncClient(mock_file_system_.get())) {
  }

  virtual void SetUp() OVERRIDE {
    // Create a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    EXPECT_CALL(*mock_file_system_, AddObserver(sync_client_.get()))
        .Times(1);
    EXPECT_CALL(*mock_file_system_, RemoveObserver(sync_client_.get()))
        .Times(1);

    sync_client_->Initialize();
  }

  // Sets up test files in the temporary directory.
  void SetUpTestFiles() {
    // Create a symlink in the temporary directory to /dev/null.
    // We'll collect this resource ID as a file to be fetched.
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            FilePath::FromUTF8Unsafe("/dev/null"),
            temp_dir_.path().Append("resource_id_not_fetched_foo")));
    // Create some more.
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            FilePath::FromUTF8Unsafe("/dev/null"),
            temp_dir_.path().Append("resource_id_not_fetched_bar")));
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            FilePath::FromUTF8Unsafe("/dev/null"),
            temp_dir_.path().Append("resource_id_not_fetched_baz")));

    // Create a symlink in the temporary directory to a test file
    // We won't collect this resource ID, as it already exists.
    FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    FilePath test_file_path = test_data_dir.AppendASCII("chromeos")
        .AppendASCII("gdata").AppendASCII("testfile.txt");
    ASSERT_TRUE(
        file_util::CreateSymbolicLink(
            test_file_path,
            temp_dir_.path().Append("resource_id_fetched")));
  }

  // Called when StartInitialScan() is complete.
  void OnInitialScanComplete() {
    message_loop_.Quit();
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  ScopedTempDir temp_dir_;
  scoped_ptr<MockGDataFileSystem> mock_file_system_;
  scoped_ptr<GDataSyncClient> sync_client_;
};

// Action used to set mock expectations for GetFileForResourceId().
ACTION_P3(MockGetFileForResourceId, error, local_path, file_type) {
  arg1.Run(error, local_path, file_type);
}

TEST_F(GDataSyncClientTest, StartInitialScan) {
  SetUpTestFiles();

  EXPECT_CALL(*mock_file_system_, GetGDataCachePinnedDirectory())
      .WillOnce(Return(temp_dir_.path()));

  sync_client_->StartInitialScan(
      base::Bind(&GDataSyncClientTest::OnInitialScanComplete,
                 base::Unretained(this)));
  // Wait until the initial scan is complete.
  message_loop_.Run();

  // Check the contents of the queue.
  const std::deque<std::string>& resource_ids =
      sync_client_->GetResourceIdsForTesting();
  ASSERT_EQ(3U, resource_ids.size());
  // Since these are the list of file names read from the disk, the order is
  // not guaranteed
  EXPECT_TRUE(std::find(resource_ids.begin(), resource_ids.end(),
                        "resource_id_not_fetched_foo") != resource_ids.end());
  EXPECT_TRUE(std::find(resource_ids.begin(), resource_ids.end(),
                        "resource_id_not_fetched_bar") != resource_ids.end());
  EXPECT_TRUE(std::find(resource_ids.begin(), resource_ids.end(),
                        "resource_id_not_fetched_baz") != resource_ids.end());
  // resource_id_fetched is not collected in the queue.
}

TEST_F(GDataSyncClientTest, DoFetchLoop) {
  SetUpTestFiles();

  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_foo");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_bar");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_baz");

  // The three files will be fetched by GetFileForResourceId(), once
  // DoFetchLoop() starts.
  EXPECT_CALL(*mock_file_system_,
              GetFileForResourceId("resource_id_not_fetched_foo", _))
      .WillOnce(MockGetFileForResourceId(
          base::PLATFORM_FILE_OK,
          FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
          REGULAR_FILE));
  EXPECT_CALL(*mock_file_system_,
              GetFileForResourceId("resource_id_not_fetched_bar", _))
      .WillOnce(MockGetFileForResourceId(
          base::PLATFORM_FILE_OK,
          FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
          REGULAR_FILE));
  EXPECT_CALL(*mock_file_system_,
              GetFileForResourceId("resource_id_not_fetched_baz", _))
      .WillOnce(MockGetFileForResourceId(
          base::PLATFORM_FILE_OK,
          FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
          REGULAR_FILE));

  sync_client_->DoFetchLoop();
}

TEST_F(GDataSyncClientTest, OnFilePinned) {
  SetUpTestFiles();

  // This file will be fetched by GetFileForResourceId() as OnFilePinned()
  // will kick off the fetch loop.
  EXPECT_CALL(*mock_file_system_,
              GetFileForResourceId("resource_id_not_fetched_foo", _))
      .WillOnce(MockGetFileForResourceId(
          base::PLATFORM_FILE_OK,
          FilePath::FromUTF8Unsafe("local_path_does_not_matter"),
          REGULAR_FILE));

  sync_client_->OnFilePinned("resource_id_not_fetched_foo", "md5");
}

TEST_F(GDataSyncClientTest, OnFileUnpinned) {
  SetUpTestFiles();

  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_foo");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_bar");
  sync_client_->AddResourceIdForTesting("resource_id_not_fetched_baz");
  ASSERT_EQ(3U, sync_client_->GetResourceIdsForTesting().size());

  sync_client_->OnFileUnpinned("resource_id_not_fetched_bar", "md5");
  // "bar" should be gone.
  std::deque<std::string> resource_ids =
      sync_client_->GetResourceIdsForTesting();
  ASSERT_EQ(2U, resource_ids.size());
  EXPECT_EQ("resource_id_not_fetched_foo", resource_ids[0]);
  EXPECT_EQ("resource_id_not_fetched_baz", resource_ids[1]);

  sync_client_->OnFileUnpinned("resource_id_not_fetched_foo", "md5");
  // "foo" should be gone.
  resource_ids = sync_client_->GetResourceIdsForTesting();
  ASSERT_EQ(1U, resource_ids.size());
  EXPECT_EQ("resource_id_not_fetched_baz", resource_ids[1]);

  sync_client_->OnFileUnpinned("resource_id_not_fetched_baz", "md5");
  // "baz" should be gone.
  resource_ids = sync_client_->GetResourceIdsForTesting();
  ASSERT_TRUE(resource_ids.empty());
}

}  // namespace gdata
