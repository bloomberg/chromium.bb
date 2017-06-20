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
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

namespace {

// Counts number of lines in a string.
size_t GetNumLinesInString(const std::string& string) {
  size_t split_size = base::SplitString(string, "\n", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL)
                          .size();
  // If there is an extra newline at the end, it will generate an extra empty
  // string. Do not count this as an extra line.
  if (!string.empty() && string.back() == '\n')
    --split_size;

  return split_size;
}

}  // namespace

class SingleLogSourceTest : public ::testing::Test {
 public:
  SingleLogSourceTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        num_callback_calls_(0) {
    InitializeTestLogDir();
  }

  ~SingleLogSourceTest() override {
    SingleLogSource::SetChromeStartTimeForTesting(nullptr);
  }

 protected:
  // Sets up a dummy system log directory.
  void InitializeTestLogDir() {
    ASSERT_TRUE(log_dir_.CreateUniqueTempDir());

    // Create file "messages".
    const base::FilePath messages_path = log_dir_.GetPath().Append("messages");
    base::WriteFile(messages_path, "", 0);
    EXPECT_TRUE(base::PathExists(messages_path)) << messages_path.value();

    // Create file "ui/ui.LATEST".
    const base::FilePath ui_dir_path = log_dir_.GetPath().Append("ui");
    ASSERT_TRUE(base::CreateDirectory(ui_dir_path)) << ui_dir_path.value();

    const base::FilePath ui_latest_path = ui_dir_path.Append("ui.LATEST");
    base::WriteFile(ui_latest_path, "", 0);
    ASSERT_TRUE(base::PathExists(ui_latest_path)) << ui_latest_path.value();
  }

  // Initializes the unit under test, |source_| to read a file from the dummy
  // system log directory.
  void InitializeSource(SingleLogSource::SupportedSource source_type) {
    source_ = base::MakeUnique<SingleLogSource>(source_type);
    source_->log_file_dir_path_ = log_dir_.GetPath();
    log_file_path_ = source_->log_file_dir_path_.Append(source_->source_name());
    ASSERT_TRUE(base::PathExists(log_file_path_)) << log_file_path_.value();
  }

  // Writes/appends (respectively) a string |input| to file indicated by
  // |relative_path| under |log_dir_|.
  bool WriteFile(const base::FilePath& relative_path,
                 const std::string& input) {
    return base::WriteFile(log_dir_.GetPath().Append(relative_path),
                           input.data(),
                           input.size()) == static_cast<int>(input.size());
  }
  bool AppendToFile(const base::FilePath& relative_path,
                    const std::string& input) {
    return base::AppendToFile(log_dir_.GetPath().Append(relative_path),
                              input.data(), input.size());
  }

  // Moves source file to destination path, then creates an empty file at the
  // path of the original source file.
  //
  // |src_relative_path|: Source file path relative to |log_dir_|.
  // |dest_relative_path|: Destination path relative to |log_dir_|.
  bool RotateFile(const base::FilePath& src_relative_path,
                  const base::FilePath& dest_relative_path) {
    return base::Move(log_dir_.GetPath().Append(src_relative_path),
                      log_dir_.GetPath().Append(dest_relative_path)) &&
           WriteFile(src_relative_path, "");
  }

  // Calls source_.Fetch() to start a logs fetch operation. Passes in
  // OnFileRead() as a callback. Runs until Fetch() has completed.
  void FetchFromSource() {
    source_->Fetch(
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
  std::unique_ptr<SingleLogSource> source_;

  // Counts the number of times that |source_| has invoked the callback.
  int num_callback_calls_;

  // Stores the string response returned from |source_| the last time it invoked
  // OnFileRead.
  std::string latest_response_;

  // Temporary dir for creating a dummy log file.
  base::ScopedTempDir log_dir_;

  // Path to the dummy log file in |log_dir_|.
  base::FilePath log_file_path_;

  DISALLOW_COPY_AND_ASSIGN(SingleLogSourceTest);
};

TEST_F(SingleLogSourceTest, EmptyFile) {
  InitializeSource(SingleLogSource::SupportedSource::kMessages);
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("", latest_response());
}

TEST_F(SingleLogSourceTest, SingleRead) {
  InitializeSource(SingleLogSource::SupportedSource::kUiLatest);

  EXPECT_TRUE(AppendToFile(base::FilePath("ui/ui.LATEST"), "Hello world!\n"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("Hello world!\n", latest_response());
}

TEST_F(SingleLogSourceTest, IncrementalReads) {
  InitializeSource(SingleLogSource::SupportedSource::kMessages);

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "Hello world!\n"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("Hello world!\n", latest_response());

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"),
                           "The quick brown fox jumps over the lazy dog\n"));
  FetchFromSource();

  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("The quick brown fox jumps over the lazy dog\n", latest_response());

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"),
                           "Some like it hot.\nSome like it cold\n"));
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
  InitializeSource(SingleLogSource::SupportedSource::kUiLatest);

  EXPECT_TRUE(AppendToFile(base::FilePath("ui/ui.LATEST"), "0123456789\n"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("0123456789\n", latest_response());

  // Overwrite the file.
  EXPECT_TRUE(WriteFile(base::FilePath("ui/ui.LATEST"), "abcdefg\n"));
  FetchFromSource();

  // Should re-read from the beginning.
  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("abcdefg\n", latest_response());

  // Append to the file to make sure incremental read still works.
  EXPECT_TRUE(AppendToFile(base::FilePath("ui/ui.LATEST"), "hijk\n"));
  FetchFromSource();

  EXPECT_EQ(3, num_callback_calls());
  EXPECT_EQ("hijk\n", latest_response());

  // Overwrite again, this time with a longer length than the existing file.
  // Previous contents:
  //   abcdefg~hijk~     <-- "~" is a single-char representation of newline.
  // New contents:
  //   lmnopqrstuvwxyz~  <-- excess text beyond end of prev contents: "yz~"
  EXPECT_TRUE(WriteFile(base::FilePath("ui/ui.LATEST"), "lmnopqrstuvwxyz\n"));
  FetchFromSource();

  EXPECT_EQ(4, num_callback_calls());
  EXPECT_EQ("yz\n", latest_response());
}

TEST_F(SingleLogSourceTest, IncompleteLines) {
  InitializeSource(SingleLogSource::SupportedSource::kMessages);

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "0123456789"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("", latest_response());

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "abcdefg"));
  FetchFromSource();

  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("", latest_response());

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "hijk\n"));
  FetchFromSource();

  EXPECT_EQ(3, num_callback_calls());
  // All the previously written text should be read this time.
  EXPECT_EQ("0123456789abcdefghijk\n", latest_response());

  // Check ability to read whole lines while leaving the remainder for later.
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "Hello world\n"));
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "Goodbye world"));
  FetchFromSource();

  EXPECT_EQ(4, num_callback_calls());
  EXPECT_EQ("Hello world\n", latest_response());

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "\n"));
  FetchFromSource();

  EXPECT_EQ(5, num_callback_calls());
  EXPECT_EQ("Goodbye world\n", latest_response());
}

TEST_F(SingleLogSourceTest, Anonymize) {
  InitializeSource(SingleLogSource::SupportedSource::kUiLatest);

  EXPECT_TRUE(AppendToFile(base::FilePath("ui/ui.LATEST"),
                           "My MAC address is: 11:22:33:44:55:66\n"));
  FetchFromSource();

  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("My MAC address is: 11:22:33:00:00:01\n", latest_response());

  // Suppose the write operation is not atomic, and the MAC address is written
  // across two separate writes.
  EXPECT_TRUE(AppendToFile(base::FilePath("ui/ui.LATEST"),
                           "Your MAC address is: AB:88:C"));
  FetchFromSource();

  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("", latest_response());

  EXPECT_TRUE(AppendToFile(base::FilePath("ui/ui.LATEST"), "D:99:EF:77\n"));
  FetchFromSource();

  EXPECT_EQ(3, num_callback_calls());
  EXPECT_EQ("Your MAC address is: ab:88:cd:00:00:02\n", latest_response());
}

TEST_F(SingleLogSourceTest, HandleLogFileRotation) {
  InitializeSource(SingleLogSource::SupportedSource::kMessages);

  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "1st log file\n"));
  FetchFromSource();
  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ("1st log file\n", latest_response());

  // Rotate file. Make sure the rest of the old file and the contents of the new
  // file are both read.
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "More 1st log file\n"));
  EXPECT_TRUE(
      RotateFile(base::FilePath("messages"), base::FilePath("messages.1")));
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "2nd log file\n"));

  FetchFromSource();
  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ("More 1st log file\n2nd log file\n", latest_response());

  // Rotate again, but this time omit the newline before rotating.
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "No newline here..."));
  EXPECT_TRUE(
      RotateFile(base::FilePath("messages"), base::FilePath("messages.1")));
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "3rd log file\n"));
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "Also no newline here"));

  FetchFromSource();
  EXPECT_EQ(3, num_callback_calls());
  // Make sure the rotation didn't break anything: the last part of the new file
  // does not end with a newline; thus the new file should not be read.
  EXPECT_EQ("No newline here...3rd log file\n", latest_response());

  // Finish the previous read attempt by adding the missing newline.
  EXPECT_TRUE(AppendToFile(base::FilePath("messages"), "...yet\n"));
  FetchFromSource();
  EXPECT_EQ(4, num_callback_calls());
  EXPECT_EQ("Also no newline here...yet\n", latest_response());
}

TEST_F(SingleLogSourceTest, ReadRecentLinesFromMessages) {
  // Write some lines to messages. Use various timestamp styles. Some lines have
  // timestamps, Some do not. All timestamps are in chronological order.
  const base::FilePath messages_path = base::FilePath("messages");
  // All timestamps below include time zone info. Some are EDT (-0400) and
  // others are PDT (-0700). These make sure that SingleLogSource is able to
  // read various standard timestamp formats.
  EXPECT_TRUE(
      AppendToFile(messages_path, "13 Jun 2017 15:00:00 -0400 : Alpha\n"));
  EXPECT_TRUE(
      AppendToFile(messages_path, "13-Jun-2017 15:30 -04:00 : Bravo\n"));
  EXPECT_TRUE(AppendToFile(messages_path, "Missing timestamp #1\n"));
  EXPECT_TRUE(
      AppendToFile(messages_path, "2017-06-13 3:30pm -04:00 : Charlie\n"));
  EXPECT_TRUE(AppendToFile(messages_path, "\n"));
  EXPECT_TRUE(
      AppendToFile(messages_path, "13 Jun 2017 13:30:00 -0700 : Delta\n"));
  EXPECT_TRUE(
      AppendToFile(messages_path, "Tue Jun 13 17:00 EDT 2017 : Echo\n"));
  EXPECT_TRUE(AppendToFile(messages_path, "Missing timestamp #2\n"));
  EXPECT_TRUE(
      AppendToFile(messages_path, "2017-06-13T17:30:00.125-04:00 : Foxtrot\n"));
  EXPECT_TRUE(
      AppendToFile(messages_path, "2017-06-13 3:00:00 PM PDT : Golf\n"));

  // The timestamp of the first message in file "messages".
  base::Time first_line_time;
  EXPECT_TRUE(
      base::Time::FromString("13 Jun 2017 15:00:00 EDT", &first_line_time));

  // Confirm that it was correctly parsed.
  base::Time::Exploded exploded;
  first_line_time.UTCExplode(&exploded);
  EXPECT_EQ(2017, exploded.year);
  EXPECT_EQ(6, exploded.month);
  EXPECT_EQ(2 /* Tue */, exploded.day_of_week);
  EXPECT_EQ(13, exploded.day_of_month);
  EXPECT_EQ(15 + 4 /* Convert from EDT to UTC */, exploded.hour);
  EXPECT_EQ(0, exploded.minute);
  EXPECT_EQ(0, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);

  // Provide a fake Chrome start time for testing: 15:00 EDT
  base::Time chrome_start_time = first_line_time;
  SingleLogSource::SetChromeStartTimeForTesting(&chrome_start_time);

  // Read from messages. The first line of messages should not have been read.
  InitializeSource(SingleLogSource::SupportedSource::kMessages);
  FetchFromSource();
  EXPECT_EQ(1, num_callback_calls());
  EXPECT_EQ(10U, GetNumLinesInString(latest_response())) << latest_response();

  // Update Chrome start time to: 15:15 EDT
  chrome_start_time += base::TimeDelta::FromMinutes(15);
  SingleLogSource::SetChromeStartTimeForTesting(&chrome_start_time);

  InitializeSource(SingleLogSource::SupportedSource::kMessages);
  FetchFromSource();
  EXPECT_EQ(2, num_callback_calls());
  EXPECT_EQ(9U, GetNumLinesInString(latest_response())) << latest_response();

  // Update Chrome start time: 15:45 EDT
  chrome_start_time += base::TimeDelta::FromMinutes(30);
  SingleLogSource::SetChromeStartTimeForTesting(&chrome_start_time);

  InitializeSource(SingleLogSource::SupportedSource::kMessages);
  FetchFromSource();
  EXPECT_EQ(3, num_callback_calls());
  EXPECT_EQ(5U, GetNumLinesInString(latest_response())) << latest_response();

  // Update Chrome start time: 17:10 EDT
  chrome_start_time = first_line_time + base::TimeDelta::FromHours(2) +
                      base::TimeDelta::FromMinutes(10);
  SingleLogSource::SetChromeStartTimeForTesting(&chrome_start_time);

  InitializeSource(SingleLogSource::SupportedSource::kMessages);
  FetchFromSource();
  EXPECT_EQ(4, num_callback_calls());
  EXPECT_EQ(4U, GetNumLinesInString(latest_response())) << latest_response();

  // Update Chrome start time: 17:40:00.125 EDT
  chrome_start_time +=
      base::TimeDelta::FromMinutes(30) + base::TimeDelta::FromMilliseconds(125);
  SingleLogSource::SetChromeStartTimeForTesting(&chrome_start_time);

  InitializeSource(SingleLogSource::SupportedSource::kMessages);
  FetchFromSource();
  EXPECT_EQ(5, num_callback_calls());
  EXPECT_EQ(2U, GetNumLinesInString(latest_response())) << latest_response();

  // Update Chrome start time: 17:40:00.126 EDT
  chrome_start_time += base::TimeDelta::FromMilliseconds(1);
  SingleLogSource::SetChromeStartTimeForTesting(&chrome_start_time);

  InitializeSource(SingleLogSource::SupportedSource::kMessages);
  FetchFromSource();
  EXPECT_EQ(6, num_callback_calls());
  EXPECT_EQ(1U, GetNumLinesInString(latest_response())) << latest_response();

  // Update Chrome start time: 18:10 EDT
  chrome_start_time = first_line_time + base::TimeDelta::FromHours(3) +
                      base::TimeDelta::FromMinutes(10);
  SingleLogSource::SetChromeStartTimeForTesting(&chrome_start_time);

  InitializeSource(SingleLogSource::SupportedSource::kMessages);
  FetchFromSource();
  EXPECT_EQ(7, num_callback_calls());
  EXPECT_EQ(1U, GetNumLinesInString(latest_response())) << latest_response();

  // Update Chrome start time: 18:15 EDT
  chrome_start_time += base::TimeDelta::FromMinutes(5);
  SingleLogSource::SetChromeStartTimeForTesting(&chrome_start_time);

  InitializeSource(SingleLogSource::SupportedSource::kMessages);
  FetchFromSource();
  EXPECT_EQ(8, num_callback_calls());
  EXPECT_EQ(0U, GetNumLinesInString(latest_response())) << latest_response();
}

}  // namespace system_logs
