// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/user_data_dir_extractor_win.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

// These tests only apply to the Windows version; see chrome_browser_main.cc:
// function GetUserDataDir() for details.
class ChromeMainUserDataDirTest : public InProcessBrowserTest {
 public:
  ChromeMainUserDataDirTest()
      : did_get_user_data_dir_(false),
        did_create_local_state_before_get_user_data_dir_(false) {}

  bool did_get_user_data_dir() const { return did_get_user_data_dir_; }
  bool did_create_local_state_before_get_user_data_dir() const {
    return did_create_local_state_before_get_user_data_dir_;
  }

 protected:
  virtual void SetUp() OVERRIDE {
    // Install custom GetUserDataDir callback.
    callback_ = base::Bind(&ChromeMainUserDataDirTest::GetUserDataDirCallback,
                           this);
    chrome::InstallCustomGetUserDataDirCallbackForTest(&callback_);

    InProcessBrowserTest::SetUp();
  }

 private:
  base::FilePath GetUserDataDirCallback() {
    did_get_user_data_dir_ = true;
    did_create_local_state_before_get_user_data_dir_ =
        g_browser_process && g_browser_process->created_local_state();

    base::FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    return user_data_dir;
  }

  chrome::GetUserDataDirCallback callback_;
  bool did_get_user_data_dir_;
  bool did_create_local_state_before_get_user_data_dir_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMainUserDataDirTest);
};

IN_PROC_BROWSER_TEST_F(ChromeMainUserDataDirTest, GetUserDataDir) {
  // Verify that GetUserDataDir() has been invoked.
  ASSERT_TRUE(did_get_user_data_dir());

  // Verify that local state was not created before invoking GetUserDataDir().
  ASSERT_FALSE(did_create_local_state_before_get_user_data_dir());

  // Verify that we have a valid browser process.
  ASSERT_TRUE(g_browser_process);

  // Verify that local state is created now.
  ASSERT_TRUE(g_browser_process->created_local_state());
}
