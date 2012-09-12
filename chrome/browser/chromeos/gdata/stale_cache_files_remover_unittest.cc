// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_api_parser.h"
#include "chrome/browser/chromeos/gdata/drive_file_system.h"
#include "chrome/browser/chromeos/gdata/drive_test_util.h"
#include "chrome/browser/chromeos/gdata/drive_uploader.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/mock_directory_change_observer.h"
#include "chrome/browser/chromeos/gdata/mock_drive_cache_observer.h"
#include "chrome/browser/chromeos/gdata/mock_drive_service.h"
#include "chrome/browser/chromeos/gdata/mock_drive_uploader.h"
#include "chrome/browser/chromeos/gdata/mock_drive_web_apps_registry.h"
#include "chrome/browser/chromeos/gdata/mock_free_disk_space_getter.h"
#include "chrome/browser/chromeos/gdata/stale_cache_files_remover.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace gdata {
namespace {

const int64 kLotsOfSpace = kMinFreeSpace * 10;

// Callback for DriveCache::StoreOnUIThread used in RemoveStaleCacheFiles test.
// Verifies that the result is not an error.
void VerifyCacheFileState(DriveFileError error,
                          const std::string& resource_id,
                          const std::string& md5) {
  EXPECT_EQ(DRIVE_FILE_OK, error);
}

}  // namespace

class StaleCacheFilesRemoverTest : public testing::Test {
 protected:
  StaleCacheFilesRemoverTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO),
        cache_(NULL),
        file_system_(NULL),
        mock_drive_service_(NULL),
        mock_webapps_registry_(NULL),
        root_feed_changestamp_(0) {
  }

  virtual void SetUp() OVERRIDE {
    io_thread_.StartIOThread();

    profile_.reset(new TestingProfile);

    // Allocate and keep a pointer to the mock, and inject it into the
    // DriveFileSystem object, which will own the mock object.
    mock_drive_service_ = new StrictMock<MockDriveService>;

    EXPECT_CALL(*mock_drive_service_, Initialize(profile_.get())).Times(1);

    // Likewise, this will be owned by DriveFileSystem.
    mock_free_disk_space_checker_ = new StrictMock<MockFreeDiskSpaceGetter>;
    SetFreeDiskSpaceGetterForTesting(mock_free_disk_space_checker_);

    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());

    cache_ = DriveCache::CreateDriveCacheOnUIThread(
        DriveCache::GetCacheRootPath(profile_.get()), blocking_task_runner_);

    mock_uploader_.reset(new StrictMock<MockDriveUploader>);
    mock_webapps_registry_.reset(new StrictMock<MockDriveWebAppsRegistry>);

    ASSERT_FALSE(file_system_);
    file_system_ = new DriveFileSystem(profile_.get(),
                                       cache_,
                                       mock_drive_service_,
                                       mock_uploader_.get(),
                                       mock_webapps_registry_.get(),
                                       blocking_task_runner_);

    mock_cache_observer_.reset(new StrictMock<MockDriveCacheObserver>);
    cache_->AddObserver(mock_cache_observer_.get());

    mock_directory_observer_.reset(new StrictMock<MockDirectoryChangeObserver>);
    file_system_->AddObserver(mock_directory_observer_.get());

    file_system_->Initialize();
    cache_->RequestInitializeOnUIThreadForTesting();

    stale_cache_files_remover_.reset(new StaleCacheFilesRemover(file_system_,
                                                                cache_));

    test_util::RunBlockingPoolTask();
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(file_system_);
    stale_cache_files_remover_.reset();
    EXPECT_CALL(*mock_drive_service_, CancelAll()).Times(1);
    delete file_system_;
    file_system_ = NULL;
    delete mock_drive_service_;
    mock_drive_service_ = NULL;
    SetFreeDiskSpaceGetterForTesting(NULL);
    cache_->DestroyOnUIThread();
    // The cache destruction requires to post a task to the blocking pool.
    test_util::RunBlockingPoolTask();

    profile_.reset(NULL);
  }

  // Loads test json file as root ("/drive") element.
  void LoadRootFeedDocument(const std::string& filename) {
    test_util::LoadChangeFeed(filename,
                              file_system_,
                              0,
                              root_feed_changestamp_++);
  }

  MessageLoopForUI message_loop_;
  // The order of the test threads is important, do not change the order.
  // See also content/browser/browser_thread_impl.cc.
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<TestingProfile> profile_;
  DriveCache* cache_;
  scoped_ptr<StrictMock<MockDriveUploader> > mock_uploader_;
  DriveFileSystem* file_system_;
  StrictMock<MockDriveService>* mock_drive_service_;
  scoped_ptr<StrictMock<MockDriveWebAppsRegistry> > mock_webapps_registry_;
  StrictMock<MockFreeDiskSpaceGetter>* mock_free_disk_space_checker_;
  scoped_ptr<StrictMock<MockDriveCacheObserver> > mock_cache_observer_;
  scoped_ptr<StrictMock<MockDirectoryChangeObserver> > mock_directory_observer_;
  scoped_ptr<StaleCacheFilesRemover> stale_cache_files_remover_;

  int root_feed_changestamp_;
};

TEST_F(StaleCacheFilesRemoverTest, RemoveStaleCacheFiles) {
  FilePath dummy_file = test_util::GetTestFilePath("gdata/root_feed.json");
  std::string resource_id("pdf:1a2b3c");
  std::string md5("abcdef0123456789");

  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  // Create a stale cache file.
  cache_->StoreOnUIThread(resource_id, md5, dummy_file,
                          DriveCache::FILE_OPERATION_COPY,
                          base::Bind(&gdata::VerifyCacheFileState));
  test_util::RunBlockingPoolTask();

  // Verify that the cache file exists.
  FilePath path = cache_->GetCacheFilePath(resource_id,
                                           md5,
                                           DriveCache::CACHE_TYPE_TMP,
                                           DriveCache::CACHED_FILE_FROM_SERVER);
  EXPECT_TRUE(file_util::PathExists(path));

  // Verify that the corresponding file entry doesn't exist.
  EXPECT_CALL(*mock_drive_service_, GetAccountMetadata(_)).Times(1);
  EXPECT_CALL(*mock_drive_service_, GetDocuments(Eq(GURL()), _, "", _, _))
      .Times(1);
  EXPECT_CALL(*mock_webapps_registry_, UpdateFromFeed(_)).Times(1);

  DriveFileError error(DRIVE_FILE_OK);
  FilePath unused;
  scoped_ptr<DriveEntryProto> entry_proto;
  file_system_->GetEntryInfoByResourceId(
      resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error,
                 &unused,
                 &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);

  file_system_->GetEntryInfoByPath(
      path,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error,
                 &entry_proto));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());

  // Load a root feed.
  LoadRootFeedDocument("gdata/root_feed.json");

  // Wait for StaleCacheFilesRemover to finish cleaning up the stale file.
  test_util::RunBlockingPoolTask();

  // Verify that the cache file is deleted.
  path = cache_->GetCacheFilePath(resource_id,
                                  md5,
                                  DriveCache::CACHE_TYPE_TMP,
                                  DriveCache::CACHED_FILE_FROM_SERVER);
  EXPECT_FALSE(file_util::PathExists(path));
}

}   // namespace gdata
