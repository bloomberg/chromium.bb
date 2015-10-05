// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/scoped_file_opener.h"

#include <vector>

#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

class TestingProvidedFileSystem : public FakeProvidedFileSystem {
 public:
  TestingProvidedFileSystem()
      : FakeProvidedFileSystem(ProvidedFileSystemInfo()) {}

  ~TestingProvidedFileSystem() override {}

  AbortCallback OpenFile(const base::FilePath& file_path,
                         OpenFileMode mode,
                         const OpenFileCallback& callback) override {
    open_callback_ = callback;
    return base::Bind(&TestingProvidedFileSystem::AbortOpen,
                      base::Unretained(this));
  }

  AbortCallback CloseFile(
      int file_handle,
      const storage::AsyncFileUtil::StatusCallback& callback) override {
    close_requests_.push_back(file_handle);
    callback.Run(base::File::FILE_OK);
    return AbortCallback();
  }

  const OpenFileCallback& open_callback() const { return open_callback_; }
  const std::vector<int> close_requests() const { return close_requests_; }

 private:
  OpenFileCallback open_callback_;
  std::vector<int> close_requests_;

  void AbortOpen() {
    open_callback_.Run(0, base::File::FILE_ERROR_ABORT);
    open_callback_ = OpenFileCallback();
  }
};

typedef std::vector<std::pair<int, base::File::Error>> OpenLog;

void LogOpen(OpenLog* log, int file_handle, base::File::Error result) {
  log->push_back(std::make_pair(file_handle, result));
}

}  // namespace

TEST(ScopedFileOpenerTest, AbortWhileOpening) {
  TestingProvidedFileSystem file_system;
  content::TestBrowserThreadBundle thread_bundle;
  OpenLog log;
  {
    ScopedFileOpener file_opener(&file_system, base::FilePath(),
                                 OPEN_FILE_MODE_READ,
                                 base::Bind(&LogOpen, &log));
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(file_system.open_callback().is_null());
  }
  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(0, log[0].first);
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, log[0].second);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, file_system.close_requests().size());
}

TEST(ScopedFileOpenerTest, CloseWhileOpening) {
  TestingProvidedFileSystem file_system;
  content::TestBrowserThreadBundle thread_bundle;
  OpenLog log;
  {
    ScopedFileOpener file_opener(&file_system, base::FilePath(),
                                 OPEN_FILE_MODE_READ,
                                 base::Bind(&LogOpen, &log));
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(file_system.open_callback().is_null());
    // Complete opening asynchronously, so after trying to abort.
    const ProvidedFileSystemInterface::OpenFileCallback open_callback =
        file_system.open_callback();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(open_callback, 123, base::File::FILE_OK));
  }

  // Wait until the open callback is called asynchonously.
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(0, log[0].first);
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, log[0].second);

  ASSERT_EQ(1u, file_system.close_requests().size());
  EXPECT_EQ(123, file_system.close_requests()[0]);
}

TEST(ScopedFileOpenerTest, CloseAfterOpening) {
  TestingProvidedFileSystem file_system;
  content::TestBrowserThreadBundle thread_bundle;
  OpenLog log;
  {
    ScopedFileOpener file_opener(&file_system, base::FilePath(),
                                 OPEN_FILE_MODE_READ,
                                 base::Bind(&LogOpen, &log));
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(file_system.open_callback().is_null());
    file_system.open_callback().Run(123, base::File::FILE_OK);
  }

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(123, log[0].first);
  EXPECT_EQ(base::File::FILE_OK, log[0].second);

  ASSERT_EQ(1u, file_system.close_requests().size());
  EXPECT_EQ(123, file_system.close_requests()[0]);
}

TEST(ScopedFileOpenerTest, CloseAfterAborting) {
  TestingProvidedFileSystem file_system;
  content::TestBrowserThreadBundle thread_bundle;
  OpenLog log;
  {
    ScopedFileOpener file_opener(&file_system, base::FilePath(),
                                 OPEN_FILE_MODE_READ,
                                 base::Bind(&LogOpen, &log));
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(file_system.open_callback().is_null());
    file_system.open_callback().Run(0, base::File::FILE_ERROR_ABORT);
  }

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(0, log[0].first);
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, log[0].second);
  EXPECT_EQ(0u, file_system.close_requests().size());
}

}  // namespace file_system_provider
}  // namespace chromeos
