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

#include "base/path_service.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/ui/ui_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProcessSingletonLinuxTest : public UITest {
 protected:
  // A helper method to call ProcessSingleton::NotifyOtherProcess().
  // |url| will be added to CommandLine for current process, so that it can be
  // sent to browser process by ProcessSingleton::NotifyOtherProcess().
  void NotifyOtherProcess(const std::string& url, bool expect_result) {
    FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

    std::vector<std::string> old_argv =
        CommandLine::ForCurrentProcess()->argv();
    std::vector<std::string> argv;
    argv.push_back(old_argv[0]);
    argv.push_back(url);

    CommandLine::Reset();
    CommandLine::Init(argv);

    ProcessSingleton process_singleton(user_data_dir);

    if (expect_result)
      EXPECT_TRUE(process_singleton.NotifyOtherProcess());
    else
      EXPECT_FALSE(process_singleton.NotifyOtherProcess());
  }
};

// Test if the socket file and symbol link created by ProcessSingletonLinux
// are valid. When running this test, the ProcessSingleton object is already
// initiated by UITest. So we just test against this existing object.
TEST_F(ProcessSingletonLinuxTest, CheckSocketFile) {
  FilePath user_data_dir;
  FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

  path = user_data_dir.Append(chrome::kSingletonSocketFilename);

  struct stat statbuf;
  ASSERT_EQ(0, lstat(path.value().c_str(), &statbuf));
  ASSERT_TRUE(S_ISLNK(statbuf.st_mode));
  char buf[PATH_MAX + 1];
  ssize_t len = readlink(path.value().c_str(), buf, PATH_MAX);
  ASSERT_GT(len, 0);
  buf[len] = '\0';

  path = user_data_dir.Append(buf);
  ASSERT_EQ(0, lstat(path.value().c_str(), &statbuf));
  ASSERT_TRUE(S_ISSOCK(statbuf.st_mode));
}

// TODO(james.su@gmail.com): port following tests to Windows.
// Test success case of NotifyOtherProcess().
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessSuccess) {
  std::string url("about:blank");
  int original_tab_count = GetTabCount();

  NotifyOtherProcess(url, true);
  EXPECT_EQ(original_tab_count + 1, GetTabCount());
  EXPECT_EQ(url, GetActiveTabURL().spec());
}

// Test failure case of NotifyOtherProcess().
TEST_F(ProcessSingletonLinuxTest, NotifyOtherProcessFailure) {
  // Block the browser process, then it'll be killed by
  // ProcessSingleton::NotifyOtherProcess().
  kill(process(), SIGSTOP);

  // Wait for a while to make sure the browser process is actually stopped.
  // It's necessary when running with valgrind.
  sleep(1);

  std::string url("about:blank");
  NotifyOtherProcess(url, false);

  // Wait for a while to make sure the browser process is actually killed.
  sleep(1);

  EXPECT_FALSE(IsBrowserRunning());
}
