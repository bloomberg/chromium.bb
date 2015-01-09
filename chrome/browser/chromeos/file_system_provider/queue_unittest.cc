// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/queue.h"

#include <vector>

#include "base/files/file.h"
#include "base/run_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

void OnAbort(int* abort_counter,
             const storage::AsyncFileUtil::StatusCallback& callback) {
  (*abort_counter)++;
  callback.Run(base::File::FILE_ERROR_FAILED);
}

AbortCallback OnRun(int* run_counter, int* abort_counter) {
  (*run_counter)++;
  return base::Bind(&OnAbort, abort_counter);
}

void OnAbortCallback(std::vector<base::File::Error>* log,
                     base::File::Error result) {
  log->push_back(result);
}

}  // namespace

class FileSystemProviderQueueTest : public testing::Test {
 protected:
  FileSystemProviderQueueTest() {}
  virtual ~FileSystemProviderQueueTest() {}

  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(FileSystemProviderQueueTest, NewToken) {
  Queue queue(1);
  EXPECT_EQ(1u, queue.NewToken());
  EXPECT_EQ(2u, queue.NewToken());
  EXPECT_EQ(3u, queue.NewToken());
}

TEST_F(FileSystemProviderQueueTest, Enqueue_OneAtOnce) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  const size_t second_token = queue.NewToken();
  int second_counter = 0;
  int second_abort_counter = 0;
  const AbortCallback abort_callback = queue.Enqueue(
      second_token, base::Bind(&OnRun, &second_counter, &second_abort_counter));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(0, second_counter);
  EXPECT_EQ(0, second_abort_counter);

  // Complete the first task, which should not run the second one, yet.
  queue.Complete(first_token);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(0, second_counter);
  EXPECT_EQ(0, second_abort_counter);

  // Removing the first task from the queue should run the second task.
  queue.Remove(first_token);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(1, second_counter);
  EXPECT_EQ(0, second_abort_counter);

  const size_t third_token = queue.NewToken();
  int third_counter = 0;
  int third_abort_counter = 0;
  queue.Enqueue(third_token,
                base::Bind(&OnRun, &third_counter, &third_abort_counter));

  // The second task is still running, so the third one is blocked.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(1, second_counter);
  EXPECT_EQ(0, second_abort_counter);
  EXPECT_EQ(0, third_counter);
  EXPECT_EQ(0, third_abort_counter);

  // After aborting the second task, the third should run.
  std::vector<base::File::Error> abort_callback_log;
  abort_callback.Run(base::Bind(&OnAbortCallback, &abort_callback_log));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(1, second_counter);
  EXPECT_EQ(1, second_abort_counter);
  ASSERT_EQ(1u, abort_callback_log.size());
  EXPECT_EQ(base::File::FILE_ERROR_FAILED, abort_callback_log[0]);
  EXPECT_EQ(1, third_counter);
  EXPECT_EQ(0, third_abort_counter);
}

TEST_F(FileSystemProviderQueueTest, Enqueue_MultipleAtOnce) {
  Queue queue(2);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  const size_t second_token = queue.NewToken();
  int second_counter = 0;
  int second_abort_counter = 0;
  queue.Enqueue(second_token,
                base::Bind(&OnRun, &second_counter, &second_abort_counter));

  const size_t third_token = queue.NewToken();
  int third_counter = 0;
  int third_abort_counter = 0;
  queue.Enqueue(third_token,
                base::Bind(&OnRun, &third_counter, &third_abort_counter));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(1, second_counter);
  EXPECT_EQ(0, second_abort_counter);
  EXPECT_EQ(0, third_counter);
  EXPECT_EQ(0, third_abort_counter);

  // Completing and removing the second task, should start the last one.
  queue.Complete(second_token);
  queue.Remove(second_token);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(1, second_counter);
  EXPECT_EQ(0, second_abort_counter);
  EXPECT_EQ(1, third_counter);
  EXPECT_EQ(0, third_abort_counter);
}

#if !defined(NDEBUG) && defined(GTEST_HAS_DEATH_TEST)

TEST_F(FileSystemProviderQueueTest, InvalidUsage_DuplicatedTokens) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  // Use the first token on purpose.
  int second_counter = 0;
  int second_abort_counter = 0;
  EXPECT_DEATH(queue.Enqueue(first_token, base::Bind(&OnRun, &second_counter,
                                                     &second_abort_counter)),
               "");
}

TEST_F(FileSystemProviderQueueTest, InvalidUsage_CompleteNotStarted) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  // Completing and removing the first task, which however hasn't started.
  // That should not invoke the second task.
  EXPECT_DEATH(queue.Complete(first_token), "");
  EXPECT_DEATH(queue.Remove(first_token), "");
}

TEST_F(FileSystemProviderQueueTest, InvalidUsage_RemoveNotCompleted) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  base::RunLoop().RunUntilIdle();

  // Remove before completing.
  EXPECT_DEATH(queue.Remove(first_token), "");
}

TEST_F(FileSystemProviderQueueTest, InvalidUsage_CompleteAfterAborting) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  AbortCallback first_abort_callback = queue.Enqueue(
      first_token, base::Bind(&OnRun, &first_counter, &first_abort_counter));

  base::RunLoop().RunUntilIdle();

  // Run, then abort.
  std::vector<base::File::Error> first_abort_callback_log;
  first_abort_callback.Run(
      base::Bind(&OnAbortCallback, &first_abort_callback_log));

  EXPECT_DEATH(queue.Complete(first_token), "");
}

TEST_F(FileSystemProviderQueueTest, InvalidUsage_RemoveAfterAborting) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  AbortCallback first_abort_callback = queue.Enqueue(
      first_token, base::Bind(&OnRun, &first_counter, &first_abort_counter));

  base::RunLoop().RunUntilIdle();

  // Abort after executing.
  std::vector<base::File::Error> first_abort_callback_log;
  first_abort_callback.Run(
      base::Bind(&OnAbortCallback, &first_abort_callback_log));

  base::RunLoop().RunUntilIdle();

  // Remove before completing.
  EXPECT_DEATH(queue.Remove(first_token), "");
}

TEST_F(FileSystemProviderQueueTest, InvalidUsage_CompleteTwice) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  AbortCallback first_abort_callback = queue.Enqueue(
      first_token, base::Bind(&OnRun, &first_counter, &first_abort_counter));

  base::RunLoop().RunUntilIdle();

  queue.Complete(first_token);
  EXPECT_DEATH(queue.Complete(first_token), "");
}

TEST_F(FileSystemProviderQueueTest, InvalidUsage_RemoveTwice) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  AbortCallback first_abort_callback = queue.Enqueue(
      first_token, base::Bind(&OnRun, &first_counter, &first_abort_counter));

  base::RunLoop().RunUntilIdle();

  queue.Complete(first_token);
  queue.Remove(first_token);
  EXPECT_DEATH(queue.Complete(first_token), "");
}

#endif

TEST_F(FileSystemProviderQueueTest, Enqueue_Abort) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  const AbortCallback first_abort_callback = queue.Enqueue(
      first_token, base::Bind(&OnRun, &first_counter, &first_abort_counter));

  const size_t second_token = queue.NewToken();
  int second_counter = 0;
  int second_abort_counter = 0;
  const AbortCallback second_abort_callback = queue.Enqueue(
      second_token, base::Bind(&OnRun, &second_counter, &second_abort_counter));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(0, second_counter);
  EXPECT_EQ(0, second_abort_counter);

  // Abort the first task while it's being executed.
  std::vector<base::File::Error> first_abort_callback_log;
  first_abort_callback.Run(
      base::Bind(&OnAbortCallback, &first_abort_callback_log));

  // Abort the second task, before it's started.
  EXPECT_EQ(0, second_counter);
  std::vector<base::File::Error> second_abort_callback_log;
  second_abort_callback.Run(
      base::Bind(&OnAbortCallback, &second_abort_callback_log));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(1, first_abort_counter);
  ASSERT_EQ(1u, first_abort_callback_log.size());
  EXPECT_EQ(base::File::FILE_ERROR_FAILED, first_abort_callback_log[0]);
  EXPECT_EQ(0, second_counter);
  EXPECT_EQ(0, second_abort_counter);
  ASSERT_EQ(1u, second_abort_callback_log.size());
  EXPECT_EQ(base::File::FILE_OK, second_abort_callback_log[0]);

  // Aborting again, should result in the FILE_ERROR_INVALID_MODIFICATION error
  // code.
  second_abort_callback.Run(
      base::Bind(&OnAbortCallback, &second_abort_callback_log));

  ASSERT_EQ(2u, second_abort_callback_log.size());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION,
            second_abort_callback_log[1]);
}

}  // namespace file_system_provider
}  // namespace chromeos
