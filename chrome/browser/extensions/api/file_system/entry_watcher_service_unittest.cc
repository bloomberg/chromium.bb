// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/entry_watcher_service.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "chrome/common/extensions/api/file_system.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_file_system_context.h"
#include "extensions/browser/event_router.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/fileapi/file_system_types.h"

namespace extensions {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";

void LogStatus(std::vector<base::File::Error>* log, base::File::Error status) {
  log->push_back(status);
}

}  // namespace

class EntryWatcherServiceTest : public testing::Test {
 protected:
  EntryWatcherServiceTest() {}
  virtual ~EntryWatcherServiceTest() {}

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    file_system_context_ =
        content::CreateFileSystemContextForTesting(NULL, data_dir_.path());
    service_.reset(new EntryWatcherService(profile_.get()));
    service_->SetDispatchEventImplForTesting(base::Bind(
        &EntryWatcherServiceTest::DispatchEventImpl, base::Unretained(this)));
    service_->SetGetFileSystemContextImplForTesting(
        base::Bind(&EntryWatcherServiceTest::GetFileSystemContextImpl,
                   base::Unretained(this)));
    testing_url_ = file_system_context_->CreateCrackedFileSystemURL(
        GURL(std::string("chrome-extension://") + kExtensionId),
        storage::kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe("/x/y/z"));
  }

  virtual void TearDown() OVERRIDE {
    dispatch_event_log_targets_.clear();
    dispatch_event_log_events_.clear();
  }

  void DispatchEventImpl(const std::string& extension_id,
                         scoped_ptr<Event> event) {
    dispatch_event_log_targets_.push_back(extension_id);
    dispatch_event_log_events_.push_back(event.release());
  }

  storage::FileSystemContext* GetFileSystemContextImpl(
      const std::string& extension_id,
      content::BrowserContext* context) {
    EXPECT_EQ(kExtensionId, extension_id);
    EXPECT_EQ(profile_.get(), context);
    return file_system_context_.get();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_ptr<EntryWatcherService> service_;
  storage::FileSystemURL testing_url_;
  std::vector<std::string> dispatch_event_log_targets_;
  ScopedVector<Event> dispatch_event_log_events_;
};

TEST_F(EntryWatcherServiceTest, GetWatchedEntries) {
  std::vector<base::File::Error> log;

  const bool recursive = false;
  service_->WatchDirectory(
      kExtensionId, testing_url_, recursive, base::Bind(&LogStatus, &log));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_OK, log[0]);

  {
    const std::vector<storage::FileSystemURL> watched_entries =
        service_->GetWatchedEntries(kExtensionId);
    ASSERT_EQ(1u, watched_entries.size());
    EXPECT_EQ(testing_url_, watched_entries[0]);
  }

  {
    const std::string wrong_extension_id = "abcabcabcabcabcabcabcabcabcabcab";
    const std::vector<storage::FileSystemURL> watched_entries =
        service_->GetWatchedEntries(wrong_extension_id);
    EXPECT_EQ(0u, watched_entries.size());
  }
}

TEST_F(EntryWatcherServiceTest, WatchDirectory) {
  std::vector<base::File::Error> log;

  const bool recursive = false;
  service_->WatchDirectory(
      kExtensionId, testing_url_, recursive, base::Bind(&LogStatus, &log));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_OK, log[0]);

  // The testing WatcherManager implementation emits two hard-coded fake
  // notifications as soon as the watcher is set properly. See:
  // TestWatcherManager::WatchDirectory() for details.
  ASSERT_LE(1u, dispatch_event_log_targets_.size());
  ASSERT_LE(1u, dispatch_event_log_events_.size());

  EXPECT_EQ(kExtensionId, dispatch_event_log_targets_[0]);
  EXPECT_EQ(api::file_system::OnEntryChanged::kEventName,
            dispatch_event_log_events_[0]->event_name);

  ASSERT_LE(2u, dispatch_event_log_targets_.size());
  ASSERT_LE(2u, dispatch_event_log_events_.size());
  EXPECT_EQ(kExtensionId, dispatch_event_log_targets_[1]);
  EXPECT_EQ(api::file_system::OnEntryRemoved::kEventName,
            dispatch_event_log_events_[1]->event_name);

  // No unexpected events.
  ASSERT_EQ(2u, dispatch_event_log_targets_.size());
  ASSERT_EQ(2u, dispatch_event_log_events_.size());

  const std::vector<storage::FileSystemURL> watched_entries =
      service_->GetWatchedEntries(kExtensionId);
  ASSERT_EQ(1u, watched_entries.size());
  EXPECT_EQ(testing_url_, watched_entries[0]);
}

TEST_F(EntryWatcherServiceTest, WatchDirectory_AlreadyExists) {
  std::vector<base::File::Error> log;

  const bool recursive = false;
  service_->WatchDirectory(
      kExtensionId, testing_url_, recursive, base::Bind(&LogStatus, &log));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_OK, log[0]);

  ASSERT_EQ(2u, dispatch_event_log_targets_.size());
  ASSERT_EQ(2u, dispatch_event_log_events_.size());

  {
    const std::vector<storage::FileSystemURL> watched_entries =
        service_->GetWatchedEntries(kExtensionId);
    EXPECT_EQ(1u, watched_entries.size());
  }

  service_->WatchDirectory(
      kExtensionId, testing_url_, recursive, base::Bind(&LogStatus, &log));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, log.size());
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS, log[1]);

  // No new unexpected events.
  ASSERT_EQ(2u, dispatch_event_log_targets_.size());
  ASSERT_EQ(2u, dispatch_event_log_events_.size());

  {
    const std::vector<storage::FileSystemURL> watched_entries =
        service_->GetWatchedEntries(kExtensionId);
    EXPECT_EQ(1u, watched_entries.size());
  }
}

TEST_F(EntryWatcherServiceTest, WatchDirectory_Recursive) {
  std::vector<base::File::Error> log;

  const bool recursive = true;
  service_->WatchDirectory(
      kExtensionId, testing_url_, recursive, base::Bind(&LogStatus, &log));
  base::RunLoop().RunUntilIdle();

  // Recursive watchers are not supported yet.
  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, log[0]);

  // No unexpected events.
  ASSERT_EQ(0u, dispatch_event_log_targets_.size());
  ASSERT_EQ(0u, dispatch_event_log_events_.size());

  const std::vector<storage::FileSystemURL> watched_entries =
      service_->GetWatchedEntries(kExtensionId);
  EXPECT_EQ(0u, watched_entries.size());
}

TEST_F(EntryWatcherServiceTest, UnwatchEntry) {
  std::vector<base::File::Error> watch_log;

  const bool recursive = false;
  service_->WatchDirectory(kExtensionId,
                           testing_url_,
                           recursive,
                           base::Bind(&LogStatus, &watch_log));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, watch_log.size());
  EXPECT_EQ(base::File::FILE_OK, watch_log[0]);

  ASSERT_EQ(2u, dispatch_event_log_targets_.size());
  ASSERT_EQ(2u, dispatch_event_log_events_.size());

  {
    const std::vector<storage::FileSystemURL> watched_entries =
        service_->GetWatchedEntries(kExtensionId);
    EXPECT_EQ(1u, watched_entries.size());
  }

  std::vector<base::File::Error> unwatch_log;
  service_->UnwatchEntry(
      kExtensionId, testing_url_, base::Bind(&LogStatus, &unwatch_log));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, unwatch_log.size());
  EXPECT_EQ(base::File::FILE_OK, unwatch_log[0]);

  {
    const std::vector<storage::FileSystemURL> watched_entries =
        service_->GetWatchedEntries(kExtensionId);
    EXPECT_EQ(0u, watched_entries.size());
  }
}

TEST_F(EntryWatcherServiceTest, UnwatchEntry_NotFound) {
  std::vector<base::File::Error> log;
  service_->UnwatchEntry(
      kExtensionId, testing_url_, base::Bind(&LogStatus, &log));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, log[0]);
}

}  // namespace extensions
