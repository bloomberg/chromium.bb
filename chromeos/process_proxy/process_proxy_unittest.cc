// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/process_proxy/process_proxy_registry.h"

namespace chromeos {

namespace {

// The test line must have all distinct characters.
const char kTestLineToSend[] = "abcdefgh\n";
const char kTestLineExpected[] = "abcdefgh\r\n";

const char kCatCommand[] = "cat";
const char kStdoutType[] = "stdout";
const int kTestLineNum = 100;

class TestRunner {
 public:
  virtual ~TestRunner() {}
  virtual void SetupExpectations(int terminal_id) = 0;
  virtual void OnSomeRead(int terminal_id,
                          const std::string& type,
                          const std::string& output) = 0;
  virtual void StartRegistryTest(ProcessProxyRegistry* registry) = 0;

 protected:
  int terminal_id_;
};

class RegistryTestRunner : public TestRunner {
 public:
  ~RegistryTestRunner() override {}

  void SetupExpectations(int terminal_id) override {
    terminal_id_ = terminal_id;
    left_to_check_index_[0] = 0;
    left_to_check_index_[1] = 0;
    // We consider that a line processing has started if a value in
    // left_to_check__[index] is set to 0, thus -2.
    lines_left_ = 2 * kTestLineNum - 2;
    expected_line_ = kTestLineExpected;
  }

  // Method to test validity of received input. We will receive two streams of
  // the same data. (input will be echoed twice by the testing process). Each
  // stream will contain the same string repeated |kTestLineNum| times. So we
  // have to match 2 * |kTestLineNum| lines. The problem is the received lines
  // from different streams may be interleaved (e.g. we may receive
  // abc|abcdef|defgh|gh). To deal with that, we allow to test received text
  // against two lines. The lines MUST NOT have two same characters for this
  // algorithm to work.
  void OnSomeRead(int terminal_id,
                  const std::string& type,
                  const std::string& output) override {
    EXPECT_EQ(type, kStdoutType);
    EXPECT_EQ(terminal_id_, terminal_id);

    bool valid = true;
    for (size_t i = 0; i < output.length(); i++) {
      // The character output[i] should be next in at least one of the lines we
      // are testing.
      valid = (ProcessReceivedCharacter(output[i], 0) ||
               ProcessReceivedCharacter(output[i], 1));
      EXPECT_TRUE(valid) << "Received: " << output;
    }

    if (!valid || TestSucceeded()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    }
  }

  void StartRegistryTest(ProcessProxyRegistry* registry) override {
    for (int i = 0; i < kTestLineNum; i++) {
      EXPECT_TRUE(registry->SendInput(terminal_id_, kTestLineToSend));
    }
  }

 private:
  bool ProcessReceivedCharacter(char received, size_t stream) {
    if (stream >= arraysize(left_to_check_index_))
      return false;
    bool success = left_to_check_index_[stream] < expected_line_.length() &&
        expected_line_[left_to_check_index_[stream]] == received;
    if (success)
      left_to_check_index_[stream]++;
    if (left_to_check_index_[stream] == expected_line_.length() &&
        lines_left_ > 0) {
      // Take another line to test for this stream, if there are any lines left.
      // If not, this stream is done.
      left_to_check_index_[stream] = 0;
      lines_left_--;
    }
    return success;
  }

  bool TestSucceeded() {
    return left_to_check_index_[0] == expected_line_.length() &&
        left_to_check_index_[1] == expected_line_.length() &&
        lines_left_ == 0;
  }

  size_t left_to_check_index_[2];
  size_t lines_left_;
  std::string expected_line_;
};

class RegistryNotifiedOnProcessExitTestRunner : public TestRunner {
 public:
  ~RegistryNotifiedOnProcessExitTestRunner() override {}

  void SetupExpectations(int terminal_id) override {
    output_received_ = false;
    terminal_id_ = terminal_id;
  }

  void OnSomeRead(int terminal_id,
                  const std::string& type,
                  const std::string& output) override {
    EXPECT_EQ(terminal_id_, terminal_id);
    if (!output_received_) {
      output_received_ = true;
      EXPECT_EQ(type, "stdout");
      EXPECT_EQ(output, "p");
      base::Process process =
          base::Process::DeprecatedGetProcessFromHandle(terminal_id_);
      process.Terminate(0, true);
      return;
    }
    EXPECT_EQ("exit", type);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

  void StartRegistryTest(ProcessProxyRegistry* registry) override {
    EXPECT_TRUE(registry->SendInput(terminal_id_, "p"));
  }

 private:
  bool output_received_;
};

}  // namespace

class ProcessProxyTest : public testing::Test {
 public:
  ProcessProxyTest() {}
  ~ProcessProxyTest() override {}

 protected:
  void InitRegistryTest() {
    registry_ = ProcessProxyRegistry::Get();

    terminal_id_ = registry_->OpenProcess(
        kCatCommand,
        base::Bind(&ProcessProxyTest::HandleRead, base::Unretained(this)));

    EXPECT_GE(terminal_id_, 0);
    test_runner_->SetupExpectations(terminal_id_);
    test_runner_->StartRegistryTest(registry_);
  }

  void HandleRead(int terminal_id,
                  const std::string& output_type,
                  const std::string& output) {
    test_runner_->OnSomeRead(terminal_id, output_type, output);
    registry_->AckOutput(terminal_id);
  }

  void EndRegistryTest() {
    registry_->CloseProcess(terminal_id_);

    base::TerminationStatus status =
        base::GetTerminationStatus(terminal_id_, NULL);
    EXPECT_NE(base::TERMINATION_STATUS_STILL_RUNNING, status);
    if (status == base::TERMINATION_STATUS_STILL_RUNNING) {
      base::Process process =
          base::Process::DeprecatedGetProcessFromHandle(terminal_id_);
      process.Terminate(0, true);
    }

    registry_->ShutDown();

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

  void RunTest() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ProcessProxyTest::InitRegistryTest,
                              base::Unretained(this)));

    // Wait until all data from output watcher is received (QuitTask will be
    // fired on watcher thread).
    base::RunLoop().Run();

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ProcessProxyTest::EndRegistryTest, base::Unretained(this)));

    // Wait until we clean up the process proxy.
    base::RunLoop().Run();
  }

  std::unique_ptr<TestRunner> test_runner_;

 private:
  ProcessProxyRegistry* registry_;
  int terminal_id_;

  base::MessageLoop message_loop_;
};

// Test will open new process that will run cat command, and verify data we
// write to process gets echoed back.
TEST_F(ProcessProxyTest, RegistryTest) {
  test_runner_.reset(new RegistryTestRunner());
  RunTest();
}

// Open new process, then kill it. Verifiy that we detect when the process dies.
TEST_F(ProcessProxyTest, RegistryNotifiedOnProcessExit) {
  test_runner_.reset(new RegistryNotifiedOnProcessExitTestRunner());
  RunTest();
}

}  // namespace chromeos
