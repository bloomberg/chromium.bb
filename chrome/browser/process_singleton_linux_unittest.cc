// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/test/thread_test_helper.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

bool NotificationCallback(const CommandLine& command_line,
                          const base::FilePath& current_directory) {
  return true;
}

class ProcessSingletonLinuxTest : public testing::Test {
 public:
  // A ProcessSingleton exposing some protected methods for testing.
  class TestableProcessSingleton : public ProcessSingleton {
   public:
    explicit TestableProcessSingleton(const base::FilePath& user_data_dir)
        : ProcessSingleton(user_data_dir) {}
    using ProcessSingleton::NotifyOtherProcessWithTimeout;
    using ProcessSingleton::NotifyOtherProcessWithTimeoutOrCreate;
    using ProcessSingleton::OverrideCurrentPidForTesting;
    using ProcessSingleton::OverrideKillCallbackForTesting;
  };

  ProcessSingletonLinuxTest()
      : kill_callbacks_(0),
        io_thread_(BrowserThread::IO),
        wait_event_(true, false),
        signal_event_(true, false),
        process_singleton_on_thread_(NULL) {
    io_thread_.StartIOThread();
  }

  virtual void SetUp() {
    testing::Test::SetUp();

    ProcessSingleton::DisablePromptForTesting();
    // Put the lock in a temporary directory.  Doesn't need to be a
    // full profile to test this code.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    lock_path_ = temp_dir_.path().Append(chrome::kSingletonLockFilename);
    socket_path_ = temp_dir_.path().Append(chrome::kSingletonSocketFilename);
    cookie_path_ = temp_dir_.path().Append(chrome::kSingletonCookieFilename);
  }

  virtual void TearDown() {
    scoped_refptr<base::ThreadTestHelper> io_helper(new base::ThreadTestHelper(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
    ASSERT_TRUE(io_helper->Run());

    // Destruct the ProcessSingleton object before the IO thread so that its
    // internals are destructed properly.
    if (process_singleton_on_thread_) {
      worker_thread_->message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&ProcessSingletonLinuxTest::DestructProcessSingleton,
                     base::Unretained(this)));

      scoped_refptr<base::ThreadTestHelper> helper(
          new base::ThreadTestHelper(worker_thread_->message_loop_proxy()));
      ASSERT_TRUE(helper->Run());
    }

    io_thread_.Stop();
    testing::Test::TearDown();
  }

  void CreateProcessSingletonOnThread() {
    ASSERT_EQ(NULL, worker_thread_.get());
    worker_thread_.reset(new base::Thread("BlockingThread"));
    worker_thread_->Start();

    worker_thread_->message_loop()->PostTask(
       FROM_HERE,
       base::Bind(&ProcessSingletonLinuxTest::
                      CreateProcessSingletonInternal,
                  base::Unretained(this)));

    scoped_refptr<base::ThreadTestHelper> helper(
        new base::ThreadTestHelper(
            worker_thread_->message_loop_proxy()));
    ASSERT_TRUE(helper->Run());
  }

  TestableProcessSingleton* CreateProcessSingleton() {
    return new TestableProcessSingleton(temp_dir_.path());
  }

  ProcessSingleton::NotifyResult NotifyOtherProcess(
      bool override_kill,
      base::TimeDelta timeout) {
    scoped_ptr<TestableProcessSingleton> process_singleton(
        CreateProcessSingleton());
    CommandLine command_line(CommandLine::ForCurrentProcess()->GetProgram());
    command_line.AppendArg("about:blank");
    if (override_kill) {
      process_singleton->OverrideCurrentPidForTesting(base::GetCurrentProcId() + 1);
      process_singleton->OverrideKillCallbackForTesting(
          base::Bind(&ProcessSingletonLinuxTest::KillCallback,
                     base::Unretained(this)));
    }

    return process_singleton->NotifyOtherProcessWithTimeout(
        command_line, timeout.InSeconds(), true);
  }

  // A helper method to call ProcessSingleton::NotifyOtherProcessOrCreate().
  ProcessSingleton::NotifyResult NotifyOtherProcessOrCreate(
      const std::string& url,
      base::TimeDelta timeout) {
    scoped_ptr<TestableProcessSingleton> process_singleton(
        CreateProcessSingleton());
    CommandLine command_line(CommandLine::ForCurrentProcess()->GetProgram());
    command_line.AppendArg(url);
    return process_singleton->NotifyOtherProcessWithTimeoutOrCreate(
        command_line, base::Bind(&NotificationCallback), timeout.InSeconds());
  }

  void CheckNotified() {
    ASSERT_EQ(1u, callback_command_lines_.size());
    bool found = false;
    for (size_t i = 0; i < callback_command_lines_[0].size(); ++i) {
      if (callback_command_lines_[0][i] == "about:blank") {
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
    ASSERT_EQ(0, kill_callbacks_);
  }

  void BlockWorkerThread() {
    worker_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&ProcessSingletonLinuxTest::BlockThread,
                   base::Unretained(this)));
  }

  void UnblockWorkerThread() {
    wait_event_.Signal();  // Unblock the worker thread for shutdown.
    signal_event_.Wait();  // Ensure thread unblocks before continuing.
  }

  void BlockThread() {
    wait_event_.Wait();
    signal_event_.Signal();
  }

  base::FilePath lock_path_;
  base::FilePath socket_path_;
  base::FilePath cookie_path_;
  int kill_callbacks_;

 private:
  void CreateProcessSingletonInternal() {
    ASSERT_TRUE(!process_singleton_on_thread_);
    process_singleton_on_thread_ = CreateProcessSingleton();
    ASSERT_EQ(ProcessSingleton::PROCESS_NONE,
              process_singleton_on_thread_->NotifyOtherProcessOrCreate(
                  base::Bind(&ProcessSingletonLinuxTest::InternalCallback,
                             base::Unretained(this))));
  }

  void DestructProcessSingleton() {
    ASSERT_TRUE(process_singleton_on_thread_);
    delete process_singleton_on_thread_;
  }

  bool InternalCallback(const CommandLine& command_line,
                        const base::FilePath& current_directory) {
    callback_command_lines_.push_back(command_line.argv());
    return true;
  }

  void KillCallback(int pid) {
    kill_callbacks_++;
  }

  content::TestBrowserThread io_thread_;
  base::ScopedTempDir temp_dir_;
  base::WaitableEvent wait_event_;
  base::WaitableEvent signal_event_;

  scoped_ptr<base::Thread> worker_thread_;
  TestableProcessSingleton* process_singleton_on_thread_;

  std::vector<CommandLine::StringVector> callback_command_lines_;
};

}  // namespace

// Test if the socket file and symbol link created by ProcessSingletonLinux
// are valid.
// If this test flakes, use http://crbug.com/74554.
TEST_F(ProcessSingletonLinuxTest, CheckSocketFile) {
  CreateProcessSingletonOnThread();
  struct stat statbuf;
  ASSERT_EQ(0, lstat(lock_path_.value().c_str(), &statbuf));
  ASSERT_TRUE(S_ISLNK(statbuf.st_mode));
  char buf[PATH_MAX];
  ssize_t len = readlink(lock_path_.value().c_str(), buf, PATH_MAX);
  ASSERT_GT(len, 0);

  ASSERT_EQ(0, lstat(socket_path_.value().c_str(), &statbuf));
  ASSERT_TRUE(S_ISLNK(statbuf.st_mode));

  len = readlink(socket_path_.value().c_str(), buf, PATH_MAX);
  ASSERT_GT(len, 0);
  base::FilePath socket_target_path = base::FilePath(std::string(buf, len));

  ASSERT_EQ(0, lstat(socket_target_path.value().c_str(), &statbuf));
  ASSERT_TRUE(S_ISSOCK(statbuf.st_mode));

  len = readlink(cookie_path_.value().c_str(), buf, PATH_MAX);
  ASSERT_GT(len, 0);
  std::string cookie(buf, len);

  base::FilePath remote_cookie_path = socket_target_path.DirName().
      Append(chrome::kSingletonCookieFilename);
  len = readlink(remote_cookie_path.value().c_str(), buf, PATH_MAX);
  ASSERT_GT(len, 0);
  EXPECT_EQ(cookie, std::string(buf, len));
}

// TODO(james.su@gmail.com): port following tests to Windows.
// Test success case of NotifyOtherProcess().
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessSuccess) {
  CreateProcessSingletonOnThread();
  EXPECT_EQ(ProcessSingleton::PROCESS_NOTIFIED,
            NotifyOtherProcess(true, TestTimeouts::action_timeout()));
  CheckNotified();
}

// Test failure case of NotifyOtherProcess().
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessFailure) {
  CreateProcessSingletonOnThread();

  BlockWorkerThread();
  EXPECT_EQ(ProcessSingleton::PROCESS_NONE,
            NotifyOtherProcess(true, TestTimeouts::action_timeout()));

  ASSERT_EQ(1, kill_callbacks_);
  UnblockWorkerThread();
}

// Test that we don't kill ourselves by accident if a lockfile with the same pid
// happens to exist.
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessNoSuicide) {
  CreateProcessSingletonOnThread();
  // Replace lockfile with one containing our own pid.
  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  std::string symlink_content = base::StringPrintf(
      "%s%c%u",
      net::GetHostName().c_str(),
      '-',
      base::GetCurrentProcId());
  EXPECT_EQ(0, symlink(symlink_content.c_str(), lock_path_.value().c_str()));

  // Remove socket so that we will not be able to notify the existing browser.
  EXPECT_EQ(0, unlink(socket_path_.value().c_str()));

  EXPECT_EQ(ProcessSingleton::PROCESS_NONE,
            NotifyOtherProcess(false, TestTimeouts::action_timeout()));
  // If we've gotten to this point without killing ourself, the test succeeded.
}

// Test that we can still notify a process on the same host even after the
// hostname changed.
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessHostChanged) {
  CreateProcessSingletonOnThread();
  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  EXPECT_EQ(ProcessSingleton::PROCESS_NOTIFIED,
            NotifyOtherProcess(false, TestTimeouts::action_timeout()));
  CheckNotified();
}

// Test that we fail when lock says process is on another host and we can't
// notify it over the socket.
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessDifferingHost) {
  CreateProcessSingletonOnThread();

  BlockWorkerThread();

  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  EXPECT_EQ(ProcessSingleton::PROFILE_IN_USE,
            NotifyOtherProcess(false, TestTimeouts::action_timeout()));

  ASSERT_EQ(0, unlink(lock_path_.value().c_str()));

  UnblockWorkerThread();
}

// Test that we fail when lock says process is on another host and we can't
// notify it over the socket.
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessOrCreate_DifferingHost) {
  CreateProcessSingletonOnThread();

  BlockWorkerThread();

  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROFILE_IN_USE,
            NotifyOtherProcessOrCreate(url, TestTimeouts::action_timeout()));

  ASSERT_EQ(0, unlink(lock_path_.value().c_str()));

  UnblockWorkerThread();
}

// Test that Create fails when another browser is using the profile directory.
TEST_F(ProcessSingletonLinuxTest, CreateFailsWithExistingBrowser) {
  CreateProcessSingletonOnThread();

  scoped_ptr<TestableProcessSingleton> process_singleton(
      CreateProcessSingleton());
  process_singleton->OverrideCurrentPidForTesting(base::GetCurrentProcId() + 1);
  EXPECT_FALSE(process_singleton->Create(
               base::Bind(&NotificationCallback)));
}

// Test that Create fails when another browser is using the profile directory
// but with the old socket location.
TEST_F(ProcessSingletonLinuxTest, CreateChecksCompatibilitySocket) {
  CreateProcessSingletonOnThread();
  scoped_ptr<TestableProcessSingleton> process_singleton(
      CreateProcessSingleton());
  process_singleton->OverrideCurrentPidForTesting(base::GetCurrentProcId() + 1);

  // Do some surgery so as to look like the old configuration.
  char buf[PATH_MAX];
  ssize_t len = readlink(socket_path_.value().c_str(), buf, sizeof(buf));
  ASSERT_GT(len, 0);
  base::FilePath socket_target_path = base::FilePath(std::string(buf, len));
  ASSERT_EQ(0, unlink(socket_path_.value().c_str()));
  ASSERT_EQ(0, rename(socket_target_path.value().c_str(),
                      socket_path_.value().c_str()));
  ASSERT_EQ(0, unlink(cookie_path_.value().c_str()));

  EXPECT_FALSE(process_singleton->Create(base::Bind(&NotificationCallback)));
}

// Test that we fail when lock says process is on another host and we can't
// notify it over the socket before of a bad cookie.
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessOrCreate_BadCookie) {
  CreateProcessSingletonOnThread();
  // Change the cookie.
  EXPECT_EQ(0, unlink(cookie_path_.value().c_str()));
  EXPECT_EQ(0, symlink("INCORRECTCOOKIE", cookie_path_.value().c_str()));

  // Also change the hostname, so the remote does not retry.
  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROFILE_IN_USE,
            NotifyOtherProcessOrCreate(url, TestTimeouts::action_timeout()));
}

