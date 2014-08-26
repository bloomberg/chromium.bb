// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/screenshot_taker.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/window_event_dispatcher.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state.h"
#endif

namespace ash {
namespace test {

class ScreenshotTakerTest : public AshTestBase,
                            public ScreenshotTakerObserver {
 public:
  ScreenshotTakerTest()
      : running_(false),
        screenshot_complete_(false),
        screenshot_result_(ScreenshotTakerObserver::SCREENSHOT_SUCCESS) {
  }

  virtual void SetUp() {
    AshTestBase::SetUp();
  }

  virtual void TearDown() {
    RunAllPendingInMessageLoop();
    AshTestBase::TearDown();
  }

  // Overridden from ScreenshotTakerObserver
  virtual void OnScreenshotCompleted(
      ScreenshotTakerObserver::Result screenshot_result,
      const base::FilePath& screenshot_path) OVERRIDE {
    screenshot_complete_ = true;
    screenshot_result_ = screenshot_result;
    screenshot_path_ = screenshot_path;
    if (!running_)
      return;
    message_loop_runner_->Quit();
    running_ = false;
  }

 protected:
  // ScreenshotTakerTest is a friend of ScreenshotTaker and therefore
  // allowed to set the directory, basename and profile.
  void SetScreenshotDirectoryForTest(
      ScreenshotTaker* screenshot_taker,
      const base::FilePath& screenshot_directory) {
    screenshot_taker->SetScreenshotDirectoryForTest(screenshot_directory);
  }
  void SetScreenshotBasenameForTest(
      ScreenshotTaker* screenshot_taker,
      const std::string& screenshot_basename) {
    screenshot_taker->SetScreenshotBasenameForTest(screenshot_basename);
  }
  void SetScreenshotProfileForTest(
      ScreenshotTaker* screenshot_taker,
      Profile* profile) {
    screenshot_taker->SetScreenshotProfileForTest(profile);
  }

  void Wait() {
    if (screenshot_complete_)
      return;
    running_ = true;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    EXPECT_TRUE(screenshot_complete_);
  }

  bool running_;
  bool screenshot_complete_;
  ScreenshotTakerObserver::Result screenshot_result_;
  base::FilePath screenshot_path_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenshotTakerTest);
};

TEST_F(ScreenshotTakerTest, TakeScreenshot) {
#if defined(OS_CHROMEOS)
  // Note that within the test framework the LoginState object will always
  // claim that the user did log in.
  ASSERT_FALSE(chromeos::LoginState::IsInitialized());
  chromeos::LoginState::Initialize();
#endif
  scoped_ptr<TestingProfileManager> profile_manager(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  ASSERT_TRUE(profile_manager->SetUp());
  TestingProfile* profile =
      profile_manager->CreateTestingProfile("test_profile");
  ScreenshotTaker screenshot_taker;
  screenshot_taker.AddObserver(this);
  base::ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  SetScreenshotDirectoryForTest(&screenshot_taker, directory.path());
  SetScreenshotBasenameForTest(&screenshot_taker, "Screenshot");
  SetScreenshotProfileForTest(&screenshot_taker, profile);

  EXPECT_TRUE(screenshot_taker.CanTakeScreenshot());

  screenshot_taker.HandleTakePartialScreenshot(
      Shell::GetPrimaryRootWindow(), gfx::Rect(0, 0, 100, 100));

  EXPECT_FALSE(screenshot_taker.CanTakeScreenshot());

  Wait();

#if defined(OS_CHROMEOS)
  // Screenshot notifications on Windows not yet turned on.
  EXPECT_TRUE(g_browser_process->notification_ui_manager()->FindById(
      std::string("screenshot")) != NULL);
  g_browser_process->notification_ui_manager()->CancelAll();
#endif

  EXPECT_EQ(ScreenshotTakerObserver::SCREENSHOT_SUCCESS, screenshot_result_);

  if (ScreenshotTakerObserver::SCREENSHOT_SUCCESS == screenshot_result_)
    EXPECT_TRUE(base::PathExists(screenshot_path_));

#if defined(OS_CHROMEOS)
  chromeos::LoginState::Shutdown();
#endif
}

}  // namespace test
}  // namespace ash
