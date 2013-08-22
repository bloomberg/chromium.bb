// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"

#include "base/prefs/testing_pref_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/change_list_loader.h"
#include "chrome/browser/chromeos/drive/change_list_processor.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/browser/browser_thread.h"

namespace drive {
namespace file_system {

OperationTestBase::LoggingObserver::LoggingObserver() {
}

OperationTestBase::LoggingObserver::~LoggingObserver() {
}

void OperationTestBase::LoggingObserver::OnDirectoryChangedByOperation(
    const base::FilePath& path) {
  changed_paths_.insert(path);
}

void OperationTestBase::LoggingObserver::OnCacheFileUploadNeededByOperation(
    const std::string& resource_id) {
  upload_needed_resource_ids_.insert(resource_id);
}

OperationTestBase::OperationTestBase() {
}

OperationTestBase::OperationTestBase(int test_thread_bundle_options)
    : thread_bundle_(test_thread_bundle_options) {
}

OperationTestBase::~OperationTestBase() {
}

void OperationTestBase::SetUp() {
  scoped_refptr<base::SequencedWorkerPool> pool =
      content::BrowserThread::GetBlockingPool();
  blocking_task_runner_ =
      pool->GetSequencedTaskRunner(pool->GetSequenceToken());

  pref_service_.reset(new TestingPrefServiceSimple);
  test_util::RegisterDrivePrefs(pref_service_->registry());

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  fake_drive_service_.reset(new FakeDriveService);
  fake_drive_service_->LoadResourceListForWapi(
      "gdata/root_feed.json");
  fake_drive_service_->LoadAccountMetadataForWapi(
      "gdata/account_metadata.json");

  scheduler_.reset(new JobScheduler(
      pref_service_.get(),
      fake_drive_service_.get(),
      blocking_task_runner_.get()));

  metadata_storage_.reset(new internal::ResourceMetadataStorage(
      temp_dir_.path(), blocking_task_runner_.get()));
  bool success = false;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadataStorage::Initialize,
                 base::Unretained(metadata_storage_.get())),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  ASSERT_TRUE(success);

  metadata_.reset(new internal::ResourceMetadata(metadata_storage_.get(),
                                                 blocking_task_runner_));

  FileError error = FILE_ERROR_FAILED;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::Initialize,
                 base::Unretained(metadata_.get())),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  ASSERT_EQ(FILE_ERROR_OK, error);

  fake_free_disk_space_getter_.reset(new FakeFreeDiskSpaceGetter);
  cache_.reset(new internal::FileCache(metadata_storage_.get(),
                                       temp_dir_.path(),
                                       blocking_task_runner_.get(),
                                       fake_free_disk_space_getter_.get()));
  success = false;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::FileCache::Initialize,
                 base::Unretained(cache_.get())),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  ASSERT_TRUE(success);

  // Makes sure the FakeDriveService's content is loaded to the metadata_.
  internal::ChangeListLoader change_list_loader(
      blocking_task_runner_.get(), metadata_.get(), scheduler_.get());

  change_list_loader.LoadIfNeeded(
      internal::DirectoryFetchInfo(),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  ASSERT_EQ(FILE_ERROR_OK, error);
}

FileError OperationTestBase::GetLocalResourceEntry(const base::FilePath& path,
                                                   ResourceEntry* entry) {
  FileError error = FILE_ERROR_FAILED;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::GetResourceEntryByPath,
                 base::Unretained(metadata()), path, entry),
      base::Bind(google_apis::test_util::CreateCopyResultCallback(&error)));
  test_util::RunBlockingPoolTask();
  return error;
}

}  // namespace file_system
}  // namespace drive
