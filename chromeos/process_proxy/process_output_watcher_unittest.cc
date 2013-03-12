// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <queue>
#include <string>
#include <vector>

#include <sys/wait.h>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chromeos/process_proxy/process_output_watcher.h"

namespace chromeos {

struct TestCase {
  std::string str;
  bool should_send_terminating_null;

  TestCase(const std::string& expected_string,
           bool send_terminating_null)
      : str(expected_string),
        should_send_terminating_null(send_terminating_null) {
  }
};

class ProcessWatcherExpectations {
 public:
  ProcessWatcherExpectations() {}

  void Init(const std::vector<TestCase>& expectations) {
    received_from_out_ = 0;

    for (size_t i = 0; i < expectations.size(); i++) {
      out_expectations_.append(expectations[i].str);
      if (expectations[i].should_send_terminating_null)
        out_expectations_.append(std::string("", 1));
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

  void RunTest(const std::vector<TestCase>& test_cases) {
    all_data_received_.reset(new base::WaitableEvent(true, false));

    base::Thread output_watch_thread("ProcessOutpuWatchThread");
    ASSERT_TRUE(output_watch_thread.Start());

    int pt_pipe[2], stop_pipe[2];
    ASSERT_FALSE(HANDLE_EINTR(pipe(pt_pipe)));
    ASSERT_FALSE(HANDLE_EINTR(pipe(stop_pipe)));

    output_watch_thread.message_loop()->PostTask(FROM_HERE,
        base::Bind(&ProcessOutputWatcherTest::StartWatch,
                   base::Unretained(this),
                   pt_pipe[0], stop_pipe[0], test_cases));

    for (size_t i = 0; i < test_cases.size(); i++) {
      const std::string& test_str = test_cases[i].str;
      // Let's make inputs not NULL terminated, unless other is specified in
      // the test case.
      ssize_t test_size = test_str.length() * sizeof(*test_str.c_str());
      if (test_cases[i].should_send_terminating_null)
        test_size += sizeof(*test_str.c_str());
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
  }

  scoped_ptr<base::WaitableEvent> all_data_received_;

 private:
  ProcessWatcherExpectations expectations_;
  std::vector<TestCase> exp;
};


TEST_F(ProcessOutputWatcherTest, OutputWatcher) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("testing output\n", false));
  test_cases.push_back(TestCase("testing error\n", false));
  test_cases.push_back(TestCase("testing error1\n", false));
  test_cases.push_back(TestCase("testing output1\n", false));
  test_cases.push_back(TestCase("testing output2\n", false));
  test_cases.push_back(TestCase("testing output3\n", false));
  test_cases.push_back(TestCase(VeryLongString(), false));
  test_cases.push_back(TestCase("testing error2\n", false));

  RunTest(test_cases);
};

// Verifies that sending '\0' generates PROCESS_OUTPUT_TYPE_OUT event and does
// not terminate output watcher.
TEST_F(ProcessOutputWatcherTest, SendNull) {
  std::vector<TestCase> test_cases;
  // This will send '\0' to output wathcer.
  test_cases.push_back(TestCase("", true));
  // Let's verify that next input also gets detected (i.e. output watcher does
  // not exit after seeing '\0' from previous test case).
  test_cases.push_back(TestCase("a", true));

  RunTest(test_cases);
};

}  // namespace chromeos
