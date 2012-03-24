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

    EXPECT_CALL(*mock_file_system_, GetGDataCachePinnedDirectory())
        .WillOnce(Return(temp_dir_.path()));
    EXPECT_CALL(*mock_file_system_, AddObserver(sync_client_.get()))
        .Times(1);
    EXPECT_CALL(*mock_file_system_, RemoveObserver(sync_client_.get()))
        .Times(1);

    sync_client_->Initialize();
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

TEST_F(GDataSyncClientTest, StartInitialScan) {
  // Create a symlink in the temporary directory to /dev/null.
  // We'll collect this resource ID as a file to be fetched.
  ASSERT_TRUE(
      file_util::CreateSymbolicLink(
          FilePath::FromUTF8Unsafe("/dev/null"),
          temp_dir_.path().Append("resource_id_not_fetched")));
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

  sync_client_->StartInitialScan(
      base::Bind(&GDataSyncClientTest::OnInitialScanComplete,
                 base::Unretained(this)));
  // Wait until the initial scan is complete.
  message_loop_.Run();

  // Check the contents of the queue.
  std::vector<std::string> resource_ids =
      sync_client_->GetResourceIdInQueueForTesting();
  ASSERT_EQ(1U, resource_ids.size());
  EXPECT_EQ("resource_id_not_fetched", resource_ids[0]);
  // resource_id_fetched is not collected in the queue.
}

}  // namespace gdata
