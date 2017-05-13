// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/single_log_source.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

class SingleLogSourceTest : public ::testing::Test {
 public:
  SingleLogSourceTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        source_(SingleLogSource::SupportedSource::kMessages),
        num_callback_calls_(0) {
    CHECK(dir_.CreateUniqueTempDir());
    log_file_path_ = dir_.GetPath().Append("log_file");

    // Create the dummy log file for writing.
    base::File new_file;
    new_file.Initialize(log_file_path_, base::File::FLAG_CREATE_ALWAYS |
                                            base::File::FLAG_WRITE);
    new_file.Close();
    CHECK(base::PathExists(log_file_path_));

    // Open the dummy log file for reading from within the log source.
    source_.file_.Initialize(log_file_path_,
                             base::File::FLAG_OPEN | base::File::FLAG_READ);
    CHECK(source_.file_.IsValid());
  }

  ~SingleLogSourceTest() override {}

  // Writes a string to |log_file_path_|.
  bool WriteFile(const std::string& input) {
    return base::WriteFile(log_file_path_, input.data(), input.size());
  }
  // Appends a string to |log_file_path_|.
  bool AppendToFile(const std::string& input) {
    return base::AppendToFile(log_file_path_, input.data(), input.size());
  }

  // Calls source_.Fetch() to start a logs fetch operation. Passes in
  // OnFileRead() as a callback. Runs until Fetch() has completed.
  void FetchFromSource() {
    source_.Fetch(
        base::Bind(&SingleLogSourceTest::OnFileRead, base::Unretained(this)));
    scoped_task_environment_.RunUntilIdle();
  }

  // Callback for fetching logs from |source_|. Overwrites the previous stored
  // value of |latest_response_|.
  void OnFileRead(SystemLogsResponse* response) {
    ++num_callback_calls_;
    if (response->empty())
      return;

    // Since |source_| represents a single log source, it should only return a
    // single string result.
    EXPECT_EQ(1U, response->size());
    latest_response_ = std::move(response->begin()->second);
  }

  int num_callback_calls() const { return num_callback_calls_; }

  const std::string& latest_response() const { return latest_response_; }

  const base::FilePath& log_file_path() const { return log_file_path_; }

 private:
  // For running scheduled tasks.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // Creates the necessary browser threads. Defined after
  // |scoped_task_environment_| in order to use the MessageLoop it created.
  content::TestBrowserThreadBundle browser_thread_bundle_;

  // Unit under test.
  SingleLogSource source_;

  // Counts the number of times that |source_| has invoked the callback.
  int num_callback_calls_;

  // Stores the string response returned from |source_| the last time it invoked
  // OnFileRead.
  std::string latest_response_;

  // Temporary dir for creating a dummy log file.
  base::ScopedTempDir dir_;

  // Path to the dummy log file in |dir_|.
  base::FilePath log_file_path_;

  DISALLOW_COPY_AND_ASSIGN(SingleLogSourceTest);
};

TEST_F(SingleLogSourceTest, EmptyFile) {
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("", latest_response());
}

TEST_F(SingleLogSourceTest, SingleRead) {
  EXPECT_TRUE(AppendToFile("Hello world!\n"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("Hello world!\n", latest_response());
}

TEST_F(SingleLogSourceTest, IncrementalReads) {
  EXPECT_TRUE(AppendToFile("Hello world!\n"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("Hello world!\n", latest_response());

  EXPECT_TRUE(AppendToFile("The quick brown fox jumps over the lazy dog\n"));
  FetchFromSource();

  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("The quick brown fox jumps over the lazy dog\n", latest_response());

  EXPECT_TRUE(AppendToFile("Some like it hot.\nSome like it cold\n"));
  FetchFromSource();

  EXPECT_EQ(3, num_callback_calls());
  EXPECT_EQ("Some like it hot.\nSome like it cold\n", latest_response());

  // As a sanity check, read entire contents of file separately to make sure it
  // was written incrementally, and hence read incrementally.
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(log_file_path(), &file_contents));
  EXPECT_EQ(
      "Hello world!\nThe quick brown fox jumps over the lazy dog\n"
      "Some like it hot.\nSome like it cold\n",
      file_contents);
}

// The log files read by SingleLogSource are not expected to be overwritten.
// This test is just to ensure that the SingleLogSource class is robust enough
// not to break in the event of an overwrite.
TEST_F(SingleLogSourceTest, FileOverwrite) {
  EXPECT_TRUE(AppendToFile("0123456789\n"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("0123456789\n", latest_response());

  // Overwrite the file.
  EXPECT_TRUE(WriteFile("abcdefg\n"));
  FetchFromSource();

  // Should re-read from the beginning.
  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("abcdefg\n", latest_response());

  // Append to the file to make sure incremental read still works.
  EXPECT_TRUE(AppendToFile("hijk\n"));
  FetchFromSource();

  EXPECT_EQ(3, num_callback_calls());
  EXPECT_EQ("hijk\n", latest_response());

  // Overwrite again, this time with a longer length than the existing file.
  // Previous contents:
  //   abcdefg~hijk~     <-- "~" is a single-char representation of newline.
  // New contents:
  //   lmnopqrstuvwxyz~  <-- excess text beyond end of prev contents: "yz~"
  EXPECT_TRUE(WriteFile("lmnopqrstuvwxyz\n"));
  FetchFromSource();

  EXPECT_EQ(4, num_callback_calls());
  EXPECT_EQ("yz\n", latest_response());
}

TEST_F(SingleLogSourceTest, IncompleteLines) {
  EXPECT_TRUE(AppendToFile("0123456789"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("", latest_response());

  EXPECT_TRUE(AppendToFile("abcdefg"));
  FetchFromSource();

  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("", latest_response());

  EXPECT_TRUE(AppendToFile("hijk\n"));
  FetchFromSource();

  EXPECT_EQ(3, num_callback_calls());
  // All the previously written text should be read this time.
  EXPECT_EQ("0123456789abcdefghijk\n", latest_response());

  EXPECT_TRUE(AppendToFile("Hello world\n"));
  EXPECT_TRUE(AppendToFile("Goodbye world"));
  FetchFromSource();

  // Partial whole-line reads are not supported. The last byte of the read must
  // be a new line.
  EXPECT_EQ(4, num_callback_calls());
  EXPECT_EQ("", latest_response());

  EXPECT_TRUE(AppendToFile("\n"));
  FetchFromSource();

  EXPECT_EQ(5, num_callback_calls());
  EXPECT_EQ("Hello world\nGoodbye world\n", latest_response());
}

TEST_F(SingleLogSourceTest, Anonymize) {
  EXPECT_TRUE(AppendToFile("My MAC address is: 11:22:33:44:55:66\n"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("My MAC address is: 11:22:33:00:00:01\n", latest_response());

  // Suppose the write operation is not atomic, and the MAC address is written
  // across two separate writes.
  EXPECT_TRUE(AppendToFile("Your MAC address is: AB:88:C"));
  FetchFromSource();

  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("", latest_response());

  EXPECT_TRUE(AppendToFile("D:99:EF:77\n"));
  FetchFromSource();

  EXPECT_EQ(3, num_callback_calls());
  EXPECT_EQ("Your MAC address is: ab:88:cd:00:00:02\n", latest_response());
}

}  // namespace system_logs
