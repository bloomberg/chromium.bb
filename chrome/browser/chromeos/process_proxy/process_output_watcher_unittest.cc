// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

  TestCase(std::string expected_string, ProcessOutputType expected_type)
      : str(expected_string),
        type(expected_type) {
  }
};

class ProcessWatcherExpectations {
 public:
  ProcessWatcherExpectations() {}

  void Init(const std::vector<TestCase>& expectations) {
    received_from_out_ = 0;
    received_from_err_ = 0;

    for (size_t i = 0; i < expectations.size(); i++) {
      if (expectations[i].type == PROCESS_OUTPUT_TYPE_OUT) {
        out_expectations_.append(expectations[i].str);
      } else {
        err_expectations_.append(expectations[i].str);
      }
    }
  }

  void CheckExpectations(const std::string& data, ProcessOutputType type) {
    const std::string& expectations =
        (type == PROCESS_OUTPUT_TYPE_OUT) ? out_expectations_
                                          : err_expectations_;
    size_t& received =
        (type == PROCESS_OUTPUT_TYPE_OUT) ? received_from_out_
                                          : received_from_err_;

    EXPECT_LT(received, expectations.length());
    if (received >= expectations.length())
      return;

    EXPECT_EQ(received, expectations.find(data, received));

    received += data.length();
  }

  bool IsDone() {
    return received_from_out_ >= out_expectations_.length() &&
           received_from_err_ >= err_expectations_.length();
  }

 private:
  std::string out_expectations_;
  size_t received_from_out_;
  std::string err_expectations_;
  size_t received_from_err_;
};

class ProcessOutputWatcherTest : public testing::Test {
public:
  void StartWatch(int out, int err, int stop,
                  const std::vector<TestCase>& expectations) {
    expectations_.Init(expectations);

    // This will delete itself.
    ProcessOutputWatcher* crosh_watcher = new ProcessOutputWatcher(
        out, err, stop,
        base::Bind(&ProcessOutputWatcherTest::OnRead, base::Unretained(this)));
    crosh_watcher->Start();
  }

  void OnRead(ProcessOutputType type, const std::string& output) {
    expectations_.CheckExpectations(output, type);
    if (expectations_.IsDone())
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

  int out_pipe[2], err_pipe[2], stop_pipe[2], foo_pipe[2];
  ASSERT_FALSE(HANDLE_EINTR(pipe(out_pipe)));
  ASSERT_FALSE(HANDLE_EINTR(pipe(err_pipe)));
  ASSERT_FALSE(HANDLE_EINTR(pipe(stop_pipe)));
  ASSERT_FALSE(HANDLE_EINTR(pipe(foo_pipe)));

  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("testing output\n", PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase("testing error\n", PROCESS_OUTPUT_TYPE_ERR));
  test_cases.push_back(TestCase("testing error1\n", PROCESS_OUTPUT_TYPE_ERR));
  test_cases.push_back(TestCase("testing output1\n", PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase("testing output2\n", PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase("testing output3\n", PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase(VeryLongString(), PROCESS_OUTPUT_TYPE_OUT));
  test_cases.push_back(TestCase("testing error2\n", PROCESS_OUTPUT_TYPE_ERR));

  output_watch_thread.message_loop()->PostTask(FROM_HERE,
      base::Bind(&ProcessOutputWatcherTest::StartWatch, base::Unretained(this),
                 out_pipe[0], err_pipe[0], stop_pipe[0], test_cases));

  for (size_t i = 0; i < test_cases.size(); i++) {
    int fd = (test_cases[i].type == PROCESS_OUTPUT_TYPE_OUT) ? out_pipe[1]
                                                             : err_pipe[1];
    // Let's make inputs not NULL terminated.
    const std::string& test_str = test_cases[i].str;
    ssize_t test_size = test_str.length() * sizeof(*test_str.c_str());
    EXPECT_EQ(test_size,
              file_util::WriteFileDescriptor(fd, test_str.c_str(), test_size));
  }

  all_data_received_->Wait();

  // Send stop signal. It is not important which string we send.
  EXPECT_EQ(1, file_util::WriteFileDescriptor(stop_pipe[1], "q", 1));

  output_watch_thread.Stop();
};

