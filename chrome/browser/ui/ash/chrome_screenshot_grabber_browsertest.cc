// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

class ChromeScreenshotGrabberBrowserTest
    : public InProcessBrowserTest,
      public ui::ScreenshotGrabberObserver,
      public message_center::MessageCenterObserver {
 public:
  ChromeScreenshotGrabberBrowserTest() : InProcessBrowserTest() {}

  // Overridden from ui::ScreenshotGrabberObserver
  void OnScreenshotCompleted(
      ScreenshotGrabberObserver::Result screenshot_result,
      const base::FilePath& screenshot_path) override {
    screenshot_result_ = screenshot_result;
    screenshot_path_ = screenshot_path;
  }

  void OnNotificationAdded(const std::string& notification_id) override {
    notification_added_ = true;
    message_loop_runner_->Quit();
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  ScreenshotGrabberObserver::Result screenshot_result_;
  base::FilePath screenshot_path_;
  bool notification_added_ = false;
};

IN_PROC_BROWSER_TEST_F(ChromeScreenshotGrabberBrowserTest, TakeScreenshot) {
  message_center::MessageCenter::Get()->AddObserver(this);

  std::unique_ptr<ChromeScreenshotGrabber> chrome_screenshot_grabber =
      base::MakeUnique<ChromeScreenshotGrabber>();
  chrome_screenshot_grabber->screenshot_grabber()->AddObserver(this);
  base::ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  EXPECT_TRUE(chrome_screenshot_grabber->CanTakeScreenshot());

  chrome_screenshot_grabber->screenshot_grabber()->TakeScreenshot(
      ash::Shell::GetPrimaryRootWindow(), gfx::Rect(0, 0, 100, 100),
      directory.GetPath().AppendASCII("Screenshot.png"));

  EXPECT_FALSE(
      chrome_screenshot_grabber->screenshot_grabber()->CanTakeScreenshot());

  message_loop_runner_ = new content::MessageLoopRunner;
  message_loop_runner_->Run();

  EXPECT_TRUE(notification_added_);
  EXPECT_NE(nullptr,
            g_browser_process->notification_ui_manager()->FindById(
                std::string("screenshot"),
                NotificationUIManager::GetProfileID(browser()->profile())));
  g_browser_process->notification_ui_manager()->CancelAll();

  EXPECT_EQ(ScreenshotGrabberObserver::SCREENSHOT_SUCCESS, screenshot_result_);
  EXPECT_TRUE(base::PathExists(screenshot_path_));
}
