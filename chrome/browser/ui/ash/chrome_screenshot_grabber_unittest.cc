// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/login/login_state.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/snapshot/screenshot_grabber.h"

namespace ash {
namespace test {

class ChromeScreenshotGrabberTest : public AshTestBase,
                                    public ui::ScreenshotGrabberObserver {
 public:
  ChromeScreenshotGrabberTest()
      : profile_(NULL),
        running_(false),
        screenshot_complete_(false),
        screenshot_result_(ScreenshotGrabberObserver::SCREENSHOT_SUCCESS) {}

  void SetUp() override {
    AshTestBase::SetUp();
    chrome_screenshot_grabber_.reset(new ChromeScreenshotGrabber);
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    CHECK(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test_profile");
    chrome_screenshot_grabber_->SetProfileForTest(profile_);
    screenshot_grabber()->AddObserver(this);
  }

  void TearDown() override {
    RunAllPendingInMessageLoop();
    screenshot_grabber()->RemoveObserver(this);
    chrome_screenshot_grabber_.reset();
    profile_manager_.reset();
    AshTestBase::TearDown();
  }

  // Overridden from ui::ScreenshotGrabberObserver
  void OnScreenshotCompleted(
      ScreenshotGrabberObserver::Result screenshot_result,
      const base::FilePath& screenshot_path) override {
    screenshot_complete_ = true;
    screenshot_result_ = screenshot_result;
    screenshot_path_ = screenshot_path;
    if (!running_)
      return;
    message_loop_runner_->Quit();
    running_ = false;
  }

 protected:
  void Wait() {
    if (screenshot_complete_)
      return;
    running_ = true;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    EXPECT_TRUE(screenshot_complete_);
  }

  ChromeScreenshotGrabber* chrome_screenshot_grabber() {
    return chrome_screenshot_grabber_.get();
  }

  ui::ScreenshotGrabber* screenshot_grabber() {
    return chrome_screenshot_grabber_->screenshot_grabber_.get();
  }

  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  bool running_;
  bool screenshot_complete_;
  ScreenshotGrabberObserver::Result screenshot_result_;
  std::unique_ptr<ChromeScreenshotGrabber> chrome_screenshot_grabber_;
  base::FilePath screenshot_path_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeScreenshotGrabberTest);
};

TEST_F(ChromeScreenshotGrabberTest, TakeScreenshot) {
  // Note that within the test framework the LoginState object will always
  // claim that the user did log in.
  ASSERT_FALSE(chromeos::LoginState::IsInitialized());
  chromeos::LoginState::Initialize();

  base::ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  EXPECT_TRUE(chrome_screenshot_grabber()->CanTakeScreenshot());

  screenshot_grabber()->TakeScreenshot(
      Shell::GetPrimaryRootWindow(), gfx::Rect(0, 0, 100, 100),
      directory.path().AppendASCII("Screenshot.png"));

  EXPECT_FALSE(screenshot_grabber()->CanTakeScreenshot());

  Wait();

  // Screenshot notifications on Windows not yet turned on.
  EXPECT_TRUE(g_browser_process->notification_ui_manager()->FindById(
                  std::string("screenshot"),
                  NotificationUIManager::GetProfileID(profile_)) != NULL);
  g_browser_process->notification_ui_manager()->CancelAll();

  EXPECT_EQ(ScreenshotGrabberObserver::SCREENSHOT_SUCCESS, screenshot_result_);

  if (ScreenshotGrabberObserver::SCREENSHOT_SUCCESS == screenshot_result_)
    EXPECT_TRUE(base::PathExists(screenshot_path_));

  chromeos::LoginState::Shutdown();
}

}  // namespace test
}  // namespace ash
