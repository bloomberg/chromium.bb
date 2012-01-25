// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <string>
#include <sys/wait.h>

#include "base/bind.h"
#include "base/eintr_wrapper.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/process_proxy/process_proxy_registry.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

const char kTestLineToSend[] = "testline\n";
const char kTestLineExpected[] = "testline\r\n";
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
    expected_output_.clear();
    left_to_check_index_ = 0;
    // Each line will be echoed twice:
    //   once as a result of terminal default functionality;
    //   once by cat command.
    for (int i = 0; i < kTestLineNum * 2; i++)
      expected_output_.append(kTestLineExpected);
  }

  virtual void OnSomeRead(pid_t pid, const std::string& type,
                          const std::string& output) OVERRIDE {
    EXPECT_EQ(type, kStdoutType);
    EXPECT_EQ(pid_, pid);

    size_t find_result = expected_output_.find(output, left_to_check_index_);
    EXPECT_EQ(left_to_check_index_, find_result);

    size_t new_left_to_check_index = left_to_check_index_ + output.length();
    if (find_result != left_to_check_index_ ||
        new_left_to_check_index >= expected_output_.length()) {
      content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                       MessageLoop::QuitClosure());
    } else {
      left_to_check_index_ = new_left_to_check_index;
    }
  }

  virtual void StartRegistryTest(ProcessProxyRegistry* registry) OVERRIDE {
    for (int i = 0; i < kTestLineNum; i++) {
      EXPECT_TRUE(registry->SendInput(pid_, kTestLineToSend));
    }
  }

 private:
  std::string expected_output_;
  size_t left_to_check_index_;
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
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     MessageLoop::QuitClosure());
  }

  virtual void StartRegistryTest(ProcessProxyRegistry* registry) {
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
      content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                       MessageLoop::QuitClosure());
    }
 }

  virtual void StartRegistryTest(ProcessProxyRegistry* registry) {
    // Send SingInt and verify the process exited.
    EXPECT_TRUE(registry->SendInput(pid_, "\003"));
  }

 private:
  bool output_received_;
};

}  // namespace

class ProcessProxyTest : public InProcessBrowserTest {
 public:
  ProcessProxyTest() {}
  ~ProcessProxyTest() {}

 protected:
  void InitRegistryTest() {
    registry_ = ProcessProxyRegistry::Get();

    EXPECT_TRUE(registry_->OpenProcess(kCatCommand, &pid_,
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

    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     MessageLoop::QuitClosure());
  }

  void RunTest() {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
         base::Bind(&ProcessProxyTest::InitRegistryTest,
                    base::Unretained(this)));

    // Wait until all data from output watcher is received (QuitTask will be
    // fired on watcher thread).
    ui_test_utils::RunMessageLoop();

    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
         base::Bind(&ProcessProxyTest::EndRegistryTest,
                    base::Unretained(this)));

    // Wait until we clean up the process proxy.
    ui_test_utils::RunMessageLoop();
  }

  scoped_ptr<TestRunner> test_runner_;

 private:
  ProcessProxyRegistry* registry_;
  pid_t pid_;
};

// Test will open new process that will run cat command, and verify data we
// write to process gets echoed back.
IN_PROC_BROWSER_TEST_F(ProcessProxyTest, RegistryTest) {
  test_runner_.reset(new RegistryTestRunner());
  RunTest();
}

// Open new process, then kill it. Verifiy that we detect when the process dies.
IN_PROC_BROWSER_TEST_F(ProcessProxyTest, RegistryNotifiedOnProcessExit) {
  test_runner_.reset(new RegistryNotifiedOnProcessExitTestRunner());
  RunTest();
}

// Test verifies that \003 message send to process is processed as SigInt.
IN_PROC_BROWSER_TEST_F(ProcessProxyTest, SigInt) {
  test_runner_.reset(new SigIntTestRunner());
  RunTest();
}
