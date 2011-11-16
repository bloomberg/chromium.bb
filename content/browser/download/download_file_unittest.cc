// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_id.h"
#include "content/browser/download/download_id_factory.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_status_updater.h"
#include "content/browser/download/mock_download_manager.h"
#include "content/browser/download/mock_download_manager_delegate.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::BrowserThreadImpl;

DownloadId::Domain kValidIdDomain = "valid DownloadId::Domain";

class DownloadFileTest : public testing::Test {
 public:

  static const char* kTestData1;
  static const char* kTestData2;
  static const char* kTestData3;
  static const char* kDataHash;
  static const int32 kDummyDownloadId;
  static const int kDummyChildId;
  static const int kDummyRequestId;

  // We need a UI |BrowserThread| in order to destruct |download_manager_|,
  // which has trait |BrowserThread::DeleteOnUIThread|.  Without this,
  // calling Release() on |download_manager_| won't ever result in its
  // destructor being called and we get a leak.
  DownloadFileTest() :
      id_factory_(new DownloadIdFactory(kValidIdDomain)),
      ui_thread_(BrowserThread::UI, &loop_),
      file_thread_(BrowserThread::FILE, &loop_) {
  }

  ~DownloadFileTest() {
  }

  virtual void SetUp() {
    download_manager_delegate_.reset(new MockDownloadManagerDelegate());
    download_manager_ = new MockDownloadManager(
        download_manager_delegate_.get(),
        id_factory_,
        &download_status_updater_);
  }

  virtual void TearDown() {
    // When a DownloadManager's reference count drops to 0, it is not
    // deleted immediately. Instead, a task is posted to the UI thread's
    // message loop to delete it.
    // So, drop the reference count to 0 and run the message loop once
    // to ensure that all resources are cleaned up before the test exits.
    download_manager_ = NULL;
    ui_thread_.message_loop()->RunAllPending();
  }

  virtual void CreateDownloadFile(scoped_ptr<DownloadFile>* file, int offset) {
    DownloadCreateInfo info;
    info.download_id = DownloadId(kValidIdDomain, kDummyDownloadId + offset);
    // info.request_handle default constructed to null.
    info.save_info.file_stream = file_stream_;
    file->reset(
        new DownloadFileImpl(&info, new DownloadRequestHandle(),
                             download_manager_));
  }

  virtual void DestroyDownloadFile(scoped_ptr<DownloadFile>* file, int offset) {
    EXPECT_EQ(kDummyDownloadId + offset, (*file)->Id());
    EXPECT_EQ(download_manager_, (*file)->GetDownloadManager());
    EXPECT_FALSE((*file)->InProgress());
    EXPECT_EQ(static_cast<int64>(expected_data_.size()),
              (*file)->BytesSoFar());

    // Make sure the data has been properly written to disk.
    std::string disk_data;
    EXPECT_TRUE(file_util::ReadFileToString((*file)->FullPath(),
                                            &disk_data));
    EXPECT_EQ(expected_data_, disk_data);

    // Make sure the Browser and File threads outlive the DownloadFile
    // to satisfy thread checks inside it.
    file->reset();
  }

  void AppendDataToFile(scoped_ptr<DownloadFile>* file,
                        const std::string& data) {
    EXPECT_TRUE((*file)->InProgress());
    (*file)->AppendDataToFile(data.data(), data.size());
    expected_data_ += data;
    EXPECT_EQ(static_cast<int64>(expected_data_.size()),
              (*file)->BytesSoFar());
  }

 protected:
  DownloadStatusUpdater download_status_updater_;
  scoped_ptr<MockDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<DownloadManager> download_manager_;

  linked_ptr<net::FileStream> file_stream_;

  // DownloadFile instance we are testing.
  scoped_ptr<DownloadFile> download_file_;

 private:
  MessageLoop loop_;
  scoped_refptr<DownloadIdFactory> id_factory_;
  // UI thread.
  BrowserThreadImpl ui_thread_;
  // File thread to satisfy debug checks in DownloadFile.
  BrowserThreadImpl file_thread_;

  // Keep track of what data should be saved to the disk file.
  std::string expected_data_;
};

const char* DownloadFileTest::kTestData1 =
    "Let's write some data to the file!\n";
const char* DownloadFileTest::kTestData2 = "Writing more data.\n";
const char* DownloadFileTest::kTestData3 = "Final line.";
const char* DownloadFileTest::kDataHash =
    "CBF68BF10F8003DB86B31343AFAC8C7175BD03FB5FC905650F8C80AF087443A8";

const int32 DownloadFileTest::kDummyDownloadId = 23;
const int DownloadFileTest::kDummyChildId = 3;
const int DownloadFileTest::kDummyRequestId = 67;

// Rename the file before any data is downloaded, after some has, after it all
// has, and after it's closed.
TEST_F(DownloadFileTest, RenameFileFinal) {
  CreateDownloadFile(&download_file_, 0);
  ASSERT_EQ(net::OK, download_file_->Initialize(true));
  FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(file_util::PathExists(initial_path));
  FilePath path_1(initial_path.InsertBeforeExtensionASCII("_1"));
  FilePath path_2(initial_path.InsertBeforeExtensionASCII("_2"));
  FilePath path_3(initial_path.InsertBeforeExtensionASCII("_3"));
  FilePath path_4(initial_path.InsertBeforeExtensionASCII("_4"));

  // Rename the file before downloading any data.
  EXPECT_EQ(net::OK, download_file_->Rename(path_1));
  FilePath renamed_path = download_file_->FullPath();
  EXPECT_EQ(path_1, renamed_path);

  // Check the files.
  EXPECT_FALSE(file_util::PathExists(initial_path));
  EXPECT_TRUE(file_util::PathExists(path_1));

  // Download the data.
  AppendDataToFile(&download_file_, kTestData1);
  AppendDataToFile(&download_file_, kTestData2);

  // Rename the file after downloading some data.
  EXPECT_EQ(net::OK, download_file_->Rename(path_2));
  renamed_path = download_file_->FullPath();
  EXPECT_EQ(path_2, renamed_path);

  // Check the files.
  EXPECT_FALSE(file_util::PathExists(path_1));
  EXPECT_TRUE(file_util::PathExists(path_2));

  AppendDataToFile(&download_file_, kTestData3);

  // Rename the file after downloading all the data.
  EXPECT_EQ(net::OK, download_file_->Rename(path_3));
  renamed_path = download_file_->FullPath();
  EXPECT_EQ(path_3, renamed_path);

  // Check the files.
  EXPECT_FALSE(file_util::PathExists(path_2));
  EXPECT_TRUE(file_util::PathExists(path_3));

  // Should not be able to get the hash until the file is closed.
  std::string hash;
  EXPECT_FALSE(download_file_->GetSha256Hash(&hash));

  download_file_->Finish();

  // Rename the file after downloading all the data and closing the file.
  EXPECT_EQ(net::OK, download_file_->Rename(path_4));
  renamed_path = download_file_->FullPath();
  EXPECT_EQ(path_4, renamed_path);

  // Check the files.
  EXPECT_FALSE(file_util::PathExists(path_3));
  EXPECT_TRUE(file_util::PathExists(path_4));

  // Check the hash.
  EXPECT_TRUE(download_file_->GetSha256Hash(&hash));
  EXPECT_EQ(kDataHash, base::HexEncode(hash.data(), hash.size()));

  DestroyDownloadFile(&download_file_, 0);
}
