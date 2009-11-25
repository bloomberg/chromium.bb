// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/chrome_process_util.h"
#include "chrome/test/ui/ui_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef UITest ProcessSingletonLinuxTest;

// A helper method to call ProcessSingleton::NotifyOtherProcess().
// |url| will be added to CommandLine for current process, so that it can be
// sent to browser process by ProcessSingleton::NotifyOtherProcess().
ProcessSingleton::NotifyResult NotifyOtherProcess(const std::string& url) {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

  // Hack: mutate the current process's command line so we don't show a dialog.
  // Note that this only works if we have no loose values on the command line,
  // but that's fine for unit tests.  In a UI test we disable error dialogs
  // when spawning Chrome, but this test hits the ProcessSingleton directly.
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kNoProcessSingletonDialog))
    cmd_line->AppendSwitch(switches::kNoProcessSingletonDialog);

  CommandLine new_cmd_line(*cmd_line);
  new_cmd_line.AppendLooseValue(ASCIIToWide(url));

  ProcessSingleton process_singleton(user_data_dir);

  // Use a short timeout to keep tests fast.
  const int kTimeoutSeconds = 3;
  return process_singleton.NotifyOtherProcessWithTimeout(new_cmd_line,
                                                         kTimeoutSeconds);
}

}  // namespace

// Test if the socket file and symbol link created by ProcessSingletonLinux
// are valid. When running this test, the ProcessSingleton object is already
// initiated by UITest. So we just test against this existing object.
TEST_F(ProcessSingletonLinuxTest, CheckSocketFile) {
  FilePath user_data_dir;
  FilePath socket_path;
  FilePath lock_path;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

  socket_path = user_data_dir.Append(chrome::kSingletonSocketFilename);
  lock_path = user_data_dir.Append(chrome::kSingletonLockFilename);

  struct stat statbuf;
  ASSERT_EQ(0, lstat(lock_path.value().c_str(), &statbuf));
  ASSERT_TRUE(S_ISLNK(statbuf.st_mode));
  char buf[PATH_MAX + 1];
  ssize_t len = readlink(lock_path.value().c_str(), buf, PATH_MAX);
  ASSERT_GT(len, 0);
  buf[len] = '\0';

  ASSERT_EQ(0, lstat(socket_path.value().c_str(), &statbuf));
  ASSERT_TRUE(S_ISSOCK(statbuf.st_mode));
}

#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
// This test is flaky on linux/views builds.
// See http://crbug.com/28808.
#define NotifyOtherProcessSuccess FLAKY_NotifyOtherProcessSuccess
#define NotifyOtherProcessHostChanged FLAKY_NotifyOtherProcessHostChanged
#endif

// TODO(james.su@gmail.com): port following tests to Windows.
// Test success case of NotifyOtherProcess().
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessSuccess) {
  std::string url("about:blank");
  int original_tab_count = GetTabCount();

  EXPECT_EQ(ProcessSingleton::PROCESS_NOTIFIED, NotifyOtherProcess(url));
  EXPECT_EQ(original_tab_count + 1, GetTabCount());
  EXPECT_EQ(url, GetActiveTabURL().spec());
}

// Test failure case of NotifyOtherProcess().
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessFailure) {
  base::ProcessId pid = ChromeBrowserProcessId(user_data_dir());

  ASSERT_GE(pid, 1);

  // Block the browser process, then it'll be killed by
  // ProcessSingleton::NotifyOtherProcess().
  kill(pid, SIGSTOP);

  // Wait to make sure the browser process is actually stopped.
  // It's necessary when running with valgrind.
  HANDLE_EINTR(waitpid(pid, 0, WUNTRACED));

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROCESS_NONE, NotifyOtherProcess(url));

  // Wait for a while to make sure the browser process is actually killed.
  EXPECT_FALSE(CrashAwareSleep(1000));
}

// Test that we can still notify a process on the same host even after the
// hostname changed.
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessHostChanged) {
  FilePath lock_path = user_data_dir().Append(chrome::kSingletonLockFilename);
  EXPECT_EQ(0, unlink(lock_path.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path.value().c_str()));

  int original_tab_count = GetTabCount();

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROCESS_NOTIFIED, NotifyOtherProcess(url));
  EXPECT_EQ(original_tab_count + 1, GetTabCount());
  EXPECT_EQ(url, GetActiveTabURL().spec());
}

// Test that we fail when lock says process is on another host and we can't
// notify it over the socket.
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessDifferingHost) {
  base::ProcessId pid = ChromeBrowserProcessId(user_data_dir());

  ASSERT_GE(pid, 1);

  // Kill the browser process, so that it does not respond on the socket.
  kill(pid, SIGKILL);
  // Wait for a while to make sure the browser process is actually killed.
  EXPECT_FALSE(CrashAwareSleep(1000));

  FilePath lock_path = user_data_dir().Append(chrome::kSingletonLockFilename);
  EXPECT_EQ(0, unlink(lock_path.value().c_str()));
  EXPECT_EQ(0, symlink("FAKEFOOHOST-1234", lock_path.value().c_str()));

  std::string url("about:blank");
  EXPECT_EQ(ProcessSingleton::PROFILE_IN_USE, NotifyOtherProcess(url));
}
