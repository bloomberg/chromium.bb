// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <queue>
#include <string>
#include <vector>

#include <sys/wait.h>

#include "base/bind.h"
#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/process_proxy/process_output_watcher.h"

struct TestCase {
  std::string str;
  ProcessOutputType type;

  TestCase(const char* expected_string,
           size_t expected_string_length,
           ProcessOutputType expected_type)
      : str(expected_string, expected_string_length),
        type(expected_type) {
  }
  TestCase(const std::string& expected_string, ProcessOutputType expected_type)
      : str(expected_string),
        type(expected_type) {
  }
};

class ProcessWatcherExpectations {
 public:
  ProcessWatcherExpectations() {}

  void Init(const std::vector<TestCase>& expectations) {
    received_from_out_ = 0;

    for (size_t i = 0; i < expectations.size(); i++) {
      out_expectations_.append(expectations[i].str.c_str(),
                               expectations[i].str.length());
    }
  }

  bool CheckExpectations(const std::string& data, ProcessOutputType type) {
    EXPECT_EQ(PROCESS_OUTPUT_TYPE_OUT, type);
    if (!type == PROCESS_OUTPUT_TYPE_OUT)
      return false;

    EXPECT_LT(received_from_out_, out_expectations_.length());
    if (received_from_out_ >= out_expectations_.length())
      return false;

    EXPECT_EQ(received_from_out_,
              out_expectations_.find(data, received_from_out_));

    received_from_out_ += data.length();
    return true;
  }

  bool IsDone() {
    return received_from_out_ >= out_expectations_.length();
  }

 private:
  std::string out_expectations_;
  size_t received_from_out_;
};

class ProcessOutputWatcherTest : public testing::Test {
public:
  void StartWatch(int pt, int stop,
                  const std::vector<TestCase>& expectations) {
    expectations_.Init(expectations);

    // This will delete itself.
    ProcessOutputWatcher* crosh_watcher = new ProcessOutputWatcher(pt, stop,
        base::Bind(&ProcessOutputWatcherTest::OnRead, base::Unretained(this)));
    crosh_watcher->Start();
  }

  void OnRead(ProcessOutputType type, const std::string& output) {
    bool success = expectations_.CheckExpectations(output, type);
    if (!success || expectations_.IsDone())
      all_data_received_->Signal();
  }

 protected:
  std::string VeryLongString() {
    std::string result = "0123456789";
    for (int i = 0; i < 8; i++)
      result = result.append(result);
    return result;
  }

  scoped_ptr<base::WaitableEvent> all_data_received_;

 private:
  ProcessWatcherExpectations expectations_;
  std::vector<TestCase> exp;
};


TEST_F(ProcessOutputWatcherTest, OutputWatcher) {
  all_data_received_.reset(new base::WaitableEvent(true, false));

  base::Thread output_watch_thread("ProcessOutpuWatchThread");
  ASSERT_TRUE(output_watch_thread.Start());

  int pt_pipe[2], stop_pipe[2];
  ASSERT_FALSE(HANDLE_EINTR(pipe(pt_pipe)));
  ASSERT_FALSE(HANDLE_EINTR(pipe(stop_pipe)));

  // TODO(tbarzic): We don't support stderr anymore, so this can be simplified.
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("testing output\n", PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase("testing error\n", PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase("testing error1\n", PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase("testing output1\n", PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase("testing output2\n", PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase("testing output3\n", PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase(VeryLongString(), PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase("testing error2\n", PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase("line with \0 in it\n",
                                arraysize("line with \0 in it \n"),
                                PROCESS_OUTPUT_TYPE_OUT));

  output_watch_thread.message_loop()->PostTask(FROM_HERE,
      base::Bind(&ProcessOutputWatcherTest::StartWatch, base::Unretained(this),
                 pt_pipe[0], stop_pipe[0], test_cases));

  for (size_t i = 0; i < test_cases.size(); i++) {
    // Let's make inputs not NULL terminated.
    const std::string& test_str = test_cases[i].str;
    ssize_t test_size = test_str.length() * sizeof(*test_str.c_str());
    EXPECT_EQ(test_size,
              file_util::WriteFileDescriptor(pt_pipe[1], test_str.c_str(),
                                             test_size));
  }

  all_data_received_->Wait();

  // Send stop signal. It is not important which string we send.
  EXPECT_EQ(1, file_util::WriteFileDescriptor(stop_pipe[1], "q", 1));

  EXPECT_NE(-1, HANDLE_EINTR(close(stop_pipe[1])));
  EXPECT_NE(-1, HANDLE_EINTR(close(pt_pipe[1])));

  output_watch_thread.Stop();
};

