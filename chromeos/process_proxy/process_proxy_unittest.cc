// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/kill.h"
#include "base/threading/thread.h"
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
  virtual void SetupExpectations(pid_t pid) = 0;
  virtual void OnSomeRead(pid_t pid, const std::string& type,
                          const std::string& output) = 0;
  virtual void StartRegistryTest(ProcessProxyRegistry* registry) = 0;

 protected:
  pid_t pid_;
};

class RegistryTestRunner : public TestRunner {
 public:
  virtual ~RegistryTestRunner() {}

  virtual void SetupExpectations(pid_t pid) OVERRIDE {
    pid_ = pid;
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
  virtual void OnSomeRead(pid_t pid, const std::string& type,
                          const std::string& output) OVERRIDE {
    EXPECT_EQ(type, kStdoutType);
    EXPECT_EQ(pid_, pid);

    bool valid = true;
    for (size_t i = 0; i < output.length(); i++) {
      // The character output[i] should be next in at least one of the lines we
      // are testing.
      valid = (ProcessReceivedCharacter(output[i], 0) ||
               ProcessReceivedCharacter(output[i], 1));
      EXPECT_TRUE(valid) << "Received: " << output;
    }

    if (!valid || TestSucceeded()) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::MessageLoop::QuitClosure());
    }
  }

  virtual void StartRegistryTest(ProcessProxyRegistry* registry) OVERRIDE {
    for (int i = 0; i < kTestLineNum; i++) {
      EXPECT_TRUE(registry->SendInput(pid_, kTestLineToSend));
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
  virtual ~RegistryNotifiedOnProcessExitTestRunner() {}

  virtual void SetupExpectations(pid_t pid) OVERRIDE {
    output_received_ = false;
    pid_ = pid;
  }

  virtual void OnSomeRead(pid_t pid, const std::string& type,
                          const std::string& output) OVERRIDE {
    EXPECT_EQ(pid_, pid);
    if (!output_received_) {
      output_received_ = true;
      EXPECT_EQ(type, "stdout");
      EXPECT_EQ(output, "p");
      base::KillProcess(pid_, 0 , true);
      return;
    }
    EXPECT_EQ("exit", type);
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
  }

  virtual void StartRegistryTest(ProcessProxyRegistry* registry) OVERRIDE {
    EXPECT_TRUE(registry->SendInput(pid_, "p"));
  }

 private:
  bool output_received_;
};

class SigIntTestRunner : public TestRunner {
 public:
  virtual ~SigIntTestRunner() {}

  virtual void SetupExpectations(pid_t pid) OVERRIDE {
    pid_ = pid;
  }

  virtual void OnSomeRead(pid_t pid, const std::string& type,
                          const std::string& output) OVERRIDE {
    EXPECT_EQ(pid_, pid);
    // We may receive ^C on stdout, but we don't care about that, as long as we
    // eventually received exit event.
    if (type == "exit") {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::MessageLoop::QuitClosure());
    }
  }

  virtual void StartRegistryTest(ProcessProxyRegistry* registry) OVERRIDE {
    // Send SingInt and verify the process exited.
    EXPECT_TRUE(registry->SendInput(pid_, "\003"));
  }
};

}  // namespace

class ProcessProxyTest : public testing::Test {
 public:
  ProcessProxyTest() {}
  virtual ~ProcessProxyTest() {}

 protected:
  void InitRegistryTest() {
    registry_ = ProcessProxyRegistry::Get();

    EXPECT_TRUE(registry_->OpenProcess(
                    kCatCommand, &pid_,
                    base::Bind(&TestRunner::OnSomeRead,
                               base::Unretained(test_runner_.get()))));

    test_runner_->SetupExpectations(pid_);
    test_runner_->StartRegistryTest(registry_);
  }

  void EndRegistryTest() {
    registry_->CloseProcess(pid_);

    base::TerminationStatus status = base::GetTerminationStatus(pid_, NULL);
    EXPECT_NE(base::TERMINATION_STATUS_STILL_RUNNING, status);
    if (status == base::TERMINATION_STATUS_STILL_RUNNING)
      base::KillProcess(pid_, 0, true);

    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
  }

  void RunTest() {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ProcessProxyTest::InitRegistryTest,
                   base::Unretained(this)));

    // Wait until all data from output watcher is received (QuitTask will be
    // fired on watcher thread).
    base::MessageLoop::current()->Run();

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ProcessProxyTest::EndRegistryTest,
                   base::Unretained(this)));

    // Wait until we clean up the process proxy.
    base::MessageLoop::current()->Run();
  }

  scoped_ptr<TestRunner> test_runner_;

 private:
  ProcessProxyRegistry* registry_;
  pid_t pid_;

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

// Test verifies that \003 message send to process is processed as SigInt.
// Timing out on the waterfall: http://crbug.com/115064
TEST_F(ProcessProxyTest, DISABLED_SigInt) {
  test_runner_.reset(new SigIntTestRunner());
  RunTest();
}

}  // namespace chromeos
