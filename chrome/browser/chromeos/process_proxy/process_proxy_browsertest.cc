// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <string>
#include <sys/wait.h>

#include "base/bind.h"
#include "base/eintr_wrapper.h"
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

const char kTestLine[] = "testline\n";
const char kCatCommand[] = "cat";
const char kStdoutType[] = "stdout";
const int kTestLineNum = 100;

}  // namespace

class ProcessProxyTest : public InProcessBrowserTest {
 public:
  ProcessProxyTest() : output_received_(false) {};
  ~ProcessProxyTest() {}

  void SetupExpectations() {
    expected_output_.clear();
    left_to_check_index_ = 0;
    for (int i = 0; i < kTestLineNum; i++)
      expected_output_.append(kTestLine);
  }

  void OnSomeRead(pid_t pid, const std::string& type,
                  const std::string& output) {
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

  void InitRegistryTest() {
    registry_ = ProcessProxyRegistry::Get();

    EXPECT_TRUE(registry_->OpenProcess(kCatCommand, &pid_,
                                       base::Bind(&ProcessProxyTest::OnSomeRead,
                                                  base::Unretained(this))));

    SetupExpectations();
    StartRegistryTest();
  }

  void StartRegistryTest() {
    for (int i = 0; i < kTestLineNum; i++) {
      EXPECT_TRUE(registry_->SendInput(pid_, kTestLine));
    }
  }

  void EndRegistryTest() {
    registry_->CloseProcess(pid_);

    // Make sure process gets reaped.
    // TODO(tbarzic): Revisit this.
    EXPECT_NE(0, HANDLE_EINTR(waitpid(pid_,NULL, 0)));

    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     MessageLoop::QuitClosure());
  }

  bool output_received_;

  pid_t pid_;

  std::string expected_output_;
  size_t left_to_check_index_;

  ProcessProxyRegistry* registry_;
};

// Test will open new process that will run cat command, and verify data we
// write to process gets echoed back.
IN_PROC_BROWSER_TEST_F(ProcessProxyTest, RegistryTest) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
       base::Bind(&ProcessProxyTest::InitRegistryTest, base::Unretained(this)));

  // Wait until all data from output watcher is received (QuitTask will be fired
  // on watcher thread).
  ui_test_utils::RunMessageLoop();

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
       base::Bind(&ProcessProxyTest::EndRegistryTest, base::Unretained(this)));

  // Wait until we clean up the process proxy.
  ui_test_utils::RunMessageLoop();
}

