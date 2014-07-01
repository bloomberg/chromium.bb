// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OPERATION_TEST_BASE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OPERATION_TEST_BASE_H_

#include <set>

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_change.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestingPrefServiceSimple;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace drive {

class EventLogger;
class FakeDriveService;
class FakeFreeDiskSpaceGetter;
class JobScheduler;

namespace internal {
class AboutResourceLoader;
class ChangeListLoader;
class FileCache;
class LoaderController;
class ResourceMetadata;
class ResourceMetadataStorage;
}  // namespace internal

namespace file_system {

// Base fixture class for testing Drive file system operations. It sets up the
// basic set of Drive internal classes (ResourceMetadata, Cache, etc) on top of
// FakeDriveService for testing.
class OperationTestBase : public testing::Test {
 protected:
  // OperationObserver that records all the events.
  class LoggingObserver : public OperationObserver {
   public:
    LoggingObserver();
    ~LoggingObserver();

    // OperationObserver overrides.
    virtual void OnFileChangedByOperation(
        const FileChange& changed_files) OVERRIDE;
    virtual void OnEntryUpdatedByOperation(
        const std::string& local_id) OVERRIDE;
    virtual void OnDriveSyncError(DriveSyncErrorType type,
                                  const std::string& local_id) OVERRIDE;

    // Gets the set of changed paths.
    const FileChange& get_changed_files() { return changed_files_; }

    // Gets the set of updated local IDs.
    const std::set<std::string>& updated_local_ids() const {
      return updated_local_ids_;
    }

    // Gets the list of drive sync errors.
    const std::vector<DriveSyncErrorType>& drive_sync_errors() const {
      return drive_sync_errors_;
    }

   private:
    FileChange changed_files_;
    std::set<std::string> updated_local_ids_;
    std::vector<DriveSyncErrorType> drive_sync_errors_;
  };

  OperationTestBase();
  explicit OperationTestBase(int test_thread_bundle_options);
  virtual ~OperationTestBase();

  // testing::Test overrides.
  virtual void SetUp() OVERRIDE;

  // Returns the path of the temporary directory for putting test files.
  base::FilePath temp_dir() const { return temp_dir_.path(); }

  // Synchronously gets the resource entry corresponding to the path from local
  // ResourceMetadta.
  FileError GetLocalResourceEntry(const base::FilePath& path,
                                  ResourceEntry* entry);

  // Synchronously gets the resource entry corresponding to the ID from local
  // ResourceMetadta.
  FileError GetLocalResourceEntryById(const std::string& local_id,
                                      ResourceEntry* entry);

  // Gets the local ID of the entry specified by the path.
  std::string GetLocalId(const base::FilePath& path);

  // Synchronously updates |metadata_| by fetching the change feed from the
  // |fake_service_|.
  FileError CheckForUpdates();

  // Accessors for the components.
  FakeDriveService* fake_service() {
    return fake_drive_service_.get();
  }
  EventLogger* logger() { return logger_.get(); }
  LoggingObserver* observer() { return &observer_; }
  JobScheduler* scheduler() { return scheduler_.get(); }
  base::SequencedTaskRunner* blocking_task_runner() {
    return blocking_task_runner_.get();
  }
  FakeFreeDiskSpaceGetter* fake_free_disk_space_getter() {
    return fake_free_disk_space_getter_.get();
  }
  internal::FileCache* cache() { return cache_.get(); }
  internal::ResourceMetadata* metadata() { return metadata_.get(); }
  internal::LoaderController* loader_controller() {
    return loader_controller_.get();
  }
  internal::ChangeListLoader* change_list_loader() {
    return change_list_loader_.get();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<TestingPrefServiceSimple> pref_service_;
  base::ScopedTempDir temp_dir_;

  LoggingObserver observer_;
  scoped_ptr<EventLogger> logger_;
  scoped_ptr<FakeDriveService> fake_drive_service_;
  scoped_ptr<JobScheduler> scheduler_;
  scoped_ptr<internal::ResourceMetadataStorage,
             test_util::DestroyHelperForTests> metadata_storage_;
  scoped_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
  scoped_ptr<internal::FileCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<internal::ResourceMetadata, test_util::DestroyHelperForTests>
      metadata_;
  scoped_ptr<internal::AboutResourceLoader> about_resource_loader_;
  scoped_ptr<internal::LoaderController> loader_controller_;
  scoped_ptr<internal::ChangeListLoader> change_list_loader_;
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OPERATION_TEST_BASE_H_
