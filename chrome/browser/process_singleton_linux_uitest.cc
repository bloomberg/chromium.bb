// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <vector>
#include <string>

#include "base/eintr_wrapper.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/chrome_process_util.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ProcessSingletonLinuxTest : public UITest {
 public:
  virtual void SetUp() {
    UITest::SetUp();
    lock_path_ = user_data_dir().Append(chrome::kSingletonLockFilename);
    socket_path_ = user_data_dir().Append(chrome::kSingletonSocketFilename);
    cookie_path_ = user_data_dir().Append(chrome::kSingletonCookieFilename);
  }

  virtual void TearDown() {
    UITest::TearDown();

    // Check that the test cleaned up after itself.
    struct stat statbuf;
    bool lock_exists = lstat(lock_path_.value().c_str(), &statbuf) == 0;
    EXPECT_FALSE(lock_exists);

    if (lock_exists) {
      // Unlink to prevent failing future tests if the lock still exists.
      EXPECT_EQ(unlink(lock_path_.value().c_str()), 0);
    }
  }

  FilePath lock_path_;
  FilePath socket_path_;
  FilePath cookie_path_;
};

ProcessSingleton* CreateProcessSingleton() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

  return new ProcessSingleton(user_data_dir);
}

CommandLine CommandLineForUrl(const std::string& url) {
  // Hack: mutate the current process's command line so we don't show a dialog.
  // Note that this only works if we have no loose values on the command line,
  // but that's fine for unit tests.  In a UI test we disable error dialogs
  // when spawning Chrome, but this test hits the ProcessSingleton directly.
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kNoProcessSingletonDialog))
    cmd_line->AppendSwitch(switches::kNoProcessSingletonDialog);

  CommandLine new_cmd_line(*cmd_line);
  new_cmd_line.AppendArg(url);
  return new_cmd_line;
}

// A helper method to call ProcessSingleton::NotifyOtherProcess().
// |url| will be added to CommandLine for current process, so that it can be
// sent to browser process by ProcessSingleton::NotifyOtherProcess().
ProcessSingleton::NotifyResult NotifyOtherProcess(const std::string& url,
                                                  int timeout_ms) {
  scoped_ptr<ProcessSingleton> process_singleton(CreateProcessSingleton());
  return process_singleton->NotifyOtherProcessWithTimeout(
      CommandLineForUrl(url), timeout_ms / 1000, true);
}

// A helper method to call ProcessSingleton::NotifyOtherProcessOrCreate().
// |url| will be added to CommandLine for current process, so that it can be
// sent to browser process by ProcessSingleton::NotifyOtherProcessOrCreate().
ProcessSingleton::NotifyResult NotifyOtherProcessOrCreate(
    const std::string& url,
    int timeout_ms) {
  scoped_ptr<ProcessSingleton> process_singleton(CreateProcessSingleton());
  return process_singleton->NotifyOtherProcessWithTimeoutOrCreate(
      CommandLineForUrl(url), timeout_ms / 1000);
}

}  // namespace

// Test if the socket file and symbol link created by ProcessSingletonLinux
// are valid. When running this test, the ProcessSingleton object is already
// initiated by UITest. So we just test against this existing object.
// This test is flaky as per http://crbug.com/74554.
TEST_F(ProcessSingletonLinuxTest, FLAKY_CheckSocketFile) {
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
  FilePath socket_target_path = FilePath(std::string(buf, len));

  ASSERT_EQ(0, lstat(socket_target_path.value().c_str(), &statbuf));
  ASSERT_TRUE(S_ISSOCK(statbuf.st_mode));

  len = readlink(cookie_path_.value().c_str(), buf, PATH_MAX);
  ASSERT_GT(len, 0);
  std::string cookie(buf, len);

  FilePath remote_cookie_path = socket_target_path.DirName().
      Append(chrome::kSingletonCookieFilename);
  len = readlink(remote_cookie_path.value().c_str(), buf, PATH_MAX);
  ASSERT_GT(len, 0);
  EXPECT_EQ(cookie, std::string(buf, len));
}

#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
// The following tests in linux/view does not pass without a window manager,
// which is true in build/try bots.
// See http://crbug.com/30953.
#define NotifyOtherProcessSuccess FAILS_NotifyOtherProcessSuccess
#define NotifyOtherProcessHostChanged FAILS_NotifyOtherProcessHostChanged
#endif

// TODO(james.su@gmail.com): port following tests to Windows.
// Test success case of NotifyOtherProcess().
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessSuccess) {
  std::string url("about:blank");
  int original_tab_count = GetTabCount();

  EXPECT_EQ(ProcessSingleton::PROCESS_NOTIFIED,
            NotifyOtherProcess(url, TestTimeouts::action_timeout_ms()));
  EXPECT_EQ(original_tab_count + 1, GetTabCount());
  EXPECT_EQ(url, GetActiveTabURL().spec());
}

// Test failure case of NotifyOtherProcess().
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessFailure) {
  base::ProcessId pid = browser_process_id();

  ASSERT_GE(pid, 1);

  // Block the browser process, then it'll be killed by
  // ProcessSingleton::NotifyOtherProcess().
  kill(pid, SIGSTOP);

  // Wait to make sure the browser process is actually stopped.
  // It's necessary when running with valgrind.
  EXPECT_GE(HANDLE_EINTR(waitpid(pid, 0, WUNTRACED)), 0);

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROCESS_NONE,
            NotifyOtherProcess(url, TestTimeouts::action_timeout_ms()));

  // Wait for a while to make sure the browser process is actually killed.
  EXPECT_FALSE(CrashAwareSleep(TestTimeouts::action_timeout_ms()));
}

// Test that we don't kill ourselves by accident if a lockfile with the same pid
// happens to exist.
// TODO(mattm): This doesn't really need to be a uitest.  (We don't use the
// uitest created browser process, but we do use some uitest provided stuff like
// the user_data_dir and the NotifyOtherProcess function in this file, which
// would have to be duplicated or shared if this test was moved into a
// unittest.)
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessNoSuicide) {
  // Replace lockfile with one containing our own pid.
  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  std::string symlink_content = StringPrintf(
      "%s%c%u",
      net::GetHostName().c_str(),
      '-',
      base::GetCurrentProcId());
  EXPECT_EQ(0, symlink(symlink_content.c_str(), lock_path_.value().c_str()));

  // Remove socket so that we will not be able to notify the existing browser.
  EXPECT_EQ(0, unlink(socket_path_.value().c_str()));

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROCESS_NONE,
            NotifyOtherProcess(url, TestTimeouts::action_timeout_ms()));
  // If we've gotten to this point without killing ourself, the test succeeded.
}

// Test that we can still notify a process on the same host even after the
// hostname changed.
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessHostChanged) {
  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  int original_tab_count = GetTabCount();

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROCESS_NOTIFIED,
            NotifyOtherProcess(url, TestTimeouts::action_timeout_ms()));
  EXPECT_EQ(original_tab_count + 1, GetTabCount());
  EXPECT_EQ(url, GetActiveTabURL().spec());
}

// Test that we fail when lock says process is on another host and we can't
// notify it over the socket.
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessDifferingHost) {
  base::ProcessId pid = browser_process_id();

  ASSERT_GE(pid, 1);

  // Kill the browser process, so that it does not respond on the socket.
  kill(pid, SIGKILL);
  // Wait for a while to make sure the browser process is actually killed.
  EXPECT_FALSE(CrashAwareSleep(TestTimeouts::action_timeout_ms()));

  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROFILE_IN_USE,
            NotifyOtherProcess(url, TestTimeouts::action_timeout_ms()));

  ASSERT_EQ(0, unlink(lock_path_.value().c_str()));
}

// Test that we fail when lock says process is on another host and we can't
// notify it over the socket.
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessOrCreate_DifferingHost) {
  base::ProcessId pid = browser_process_id();

  ASSERT_GE(pid, 1);

  // Kill the browser process, so that it does not respond on the socket.
  kill(pid, SIGKILL);
  // Wait for a while to make sure the browser process is actually killed.
  EXPECT_FALSE(CrashAwareSleep(TestTimeouts::action_timeout_ms()));

  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROFILE_IN_USE,
            NotifyOtherProcessOrCreate(url, TestTimeouts::action_timeout_ms()));

  ASSERT_EQ(0, unlink(lock_path_.value().c_str()));
}

// Test that Create fails when another browser is using the profile directory.
TEST_F(ProcessSingletonLinuxTest, CreateFailsWithExistingBrowser) {
  scoped_ptr<ProcessSingleton> process_singleton(CreateProcessSingleton());
  EXPECT_FALSE(process_singleton->Create());
}

// Test that Create fails when another browser is using the profile directory
// but with the old socket location.
TEST_F(ProcessSingletonLinuxTest, CreateChecksCompatibilitySocket) {
  scoped_ptr<ProcessSingleton> process_singleton(CreateProcessSingleton());

  // Do some surgery so as to look like the old configuration.
  char buf[PATH_MAX];
  ssize_t len = readlink(socket_path_.value().c_str(), buf, sizeof(buf));
  ASSERT_GT(len, 0);
  FilePath socket_target_path = FilePath(std::string(buf, len));
  ASSERT_EQ(0, unlink(socket_path_.value().c_str()));
  ASSERT_EQ(0, rename(socket_target_path.value().c_str(),
                      socket_path_.value().c_str()));
  ASSERT_EQ(0, unlink(cookie_path_.value().c_str()));

  EXPECT_FALSE(process_singleton->Create());
}

// Test that we fail when lock says process is on another host and we can't
// notify it over the socket before of a bad cookie.
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessOrCreate_BadCookie) {
  // Change the cookie.
  EXPECT_EQ(0, unlink(cookie_path_.value().c_str()));
  EXPECT_EQ(0, symlink("INCORRECTCOOKIE", cookie_path_.value().c_str()));

  // Also change the hostname, so the remote does not retry.
  EXPECT_EQ(0, unlink(lock_path_.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path_.value().c_str()));

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROFILE_IN_USE,
            NotifyOtherProcessOrCreate(url, TestTimeouts::action_timeout_ms()));
}
