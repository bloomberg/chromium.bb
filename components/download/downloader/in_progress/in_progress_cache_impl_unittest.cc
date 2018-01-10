// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/downloader/in_progress/in_progress_cache_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace download {
namespace {

const base::FilePath::CharType kInProgressCachePath[] =
    FILE_PATH_LITERAL("/test/in_progress_cache");

class InProgressCacheImplTest : public testing::Test {
 public:
  InProgressCacheImplTest()
      : cache_initialized_(false),
        task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}
  ~InProgressCacheImplTest() override = default;

  void SetUp() override {
    base::FilePath file_path(kInProgressCachePath);
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BACKGROUND,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
    cache_ = std::make_unique<InProgressCacheImpl>(file_path, task_runner);
  }

 protected:
  void InitializeCache() {
    cache_->Initialize(base::BindRepeating(
        &InProgressCacheImplTest::OnCacheInitialized, base::Unretained(this)));
  }

  void OnCacheInitialized() { cache_initialized_ = true; }

  std::unique_ptr<InProgressCacheImpl> cache_;
  bool cache_initialized_;
  base::test::ScopedTaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InProgressCacheImplTest);
};  // class InProgressCacheImplTest

}  // namespace

TEST_F(InProgressCacheImplTest, AddAndRetrieveAndReplaceAndRemove) {
  // Initialize cache.
  InitializeCache();
  while (!cache_initialized_) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(cache_initialized_);

  // Add entry.
  std::string guid1("guid");
  DownloadEntry entry1(guid1, "request origin", DownloadSource::UNKNOWN, 999);
  cache_->AddOrReplaceEntry(entry1);

  base::Optional<DownloadEntry> retrieved_entry1 = cache_->RetrieveEntry(guid1);
  EXPECT_TRUE(retrieved_entry1.has_value());
  EXPECT_EQ(entry1, retrieved_entry1.value());

  // Replace entry.
  std::string new_request_origin("new request origin");
  entry1.request_origin = new_request_origin;
  cache_->AddOrReplaceEntry(entry1);

  base::Optional<DownloadEntry> replaced_entry1 = cache_->RetrieveEntry(guid1);
  EXPECT_TRUE(replaced_entry1.has_value());
  EXPECT_EQ(entry1, replaced_entry1.value());
  EXPECT_EQ(new_request_origin, replaced_entry1.value().request_origin);

  // Add another entry.
  std::string guid2("guid2");
  DownloadEntry entry2(guid2, "other request origin", DownloadSource::UNKNOWN,
                       888);
  cache_->AddOrReplaceEntry(entry2);

  base::Optional<DownloadEntry> retrieved_entry2 = cache_->RetrieveEntry(guid2);
  EXPECT_TRUE(retrieved_entry2.has_value());
  EXPECT_EQ(entry2, retrieved_entry2.value());

  // Remove original entry.
  cache_->RemoveEntry(guid1);
  base::Optional<DownloadEntry> removed_entry1 = cache_->RetrieveEntry(guid1);
  EXPECT_FALSE(removed_entry1.has_value());

  // Remove other entry.
  cache_->RemoveEntry(guid2);
  base::Optional<DownloadEntry> removed_entry2 = cache_->RetrieveEntry(guid2);
  EXPECT_FALSE(removed_entry2.has_value());
}

}  // namespace download
