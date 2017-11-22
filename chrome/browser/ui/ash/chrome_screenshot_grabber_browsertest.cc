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
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

class ChromeScreenshotGrabberBrowserTest
    : public InProcessBrowserTest,
      public ui::ScreenshotGrabberObserver,
      public message_center::MessageCenterObserver,
      public ui::ClipboardObserver {
 public:
  ChromeScreenshotGrabberBrowserTest() : InProcessBrowserTest() {}
  ~ChromeScreenshotGrabberBrowserTest() override = default;

  // Overridden from ui::ScreenshotGrabberObserver
  void OnScreenshotCompleted(
      ScreenshotGrabberObserver::Result screenshot_result,
      const base::FilePath& screenshot_path) override {
    screenshot_result_ = screenshot_result;
    screenshot_path_ = screenshot_path;
  }

  // Overridden from message_center::MessageCenterObserver
  void OnNotificationAdded(const std::string& notification_id) override {
    notification_added_ = true;
    message_loop_runner_->Quit();
  }

  // Overridden from ui::ClipboardObserver
  void OnClipboardDataChanged() override {
    clipboard_changed_ = true;
    message_loop_runner_->Quit();
  }

  void RunLoop() {
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

  bool IsImageClipboardAvailable() {
    return ui::Clipboard::GetForCurrentThread()->IsFormatAvailable(
        ui::Clipboard::GetBitmapFormatType(), ui::CLIPBOARD_TYPE_COPY_PASTE);
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  ScreenshotGrabberObserver::Result screenshot_result_;
  base::FilePath screenshot_path_;
  bool notification_added_ = false;
  bool clipboard_changed_ = false;
};

IN_PROC_BROWSER_TEST_F(ChromeScreenshotGrabberBrowserTest, TakeScreenshot) {
  message_center::MessageCenter::Get()->AddObserver(this);

  ChromeScreenshotGrabber* chrome_screenshot_grabber =
      ChromeScreenshotGrabber::Get();
  chrome_screenshot_grabber->screenshot_grabber()->AddObserver(this);
  base::ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  EXPECT_TRUE(chrome_screenshot_grabber->CanTakeScreenshot());

  chrome_screenshot_grabber->screenshot_grabber()->TakeScreenshot(
      ash::Shell::GetPrimaryRootWindow(), gfx::Rect(0, 0, 100, 100),
      directory.GetPath().AppendASCII("Screenshot.png"));

  EXPECT_FALSE(
      chrome_screenshot_grabber->screenshot_grabber()->CanTakeScreenshot());

  RunLoop();
  chrome_screenshot_grabber->screenshot_grabber()->RemoveObserver(this);

  EXPECT_TRUE(notification_added_);
  const message_center::Notification* notification =
      g_browser_process->notification_ui_manager()->FindById(
          std::string("screenshot"),
          NotificationUIManager::GetProfileID(browser()->profile()));
  EXPECT_NE(nullptr, notification);

  EXPECT_EQ(ScreenshotGrabberObserver::SCREENSHOT_SUCCESS, screenshot_result_);
  EXPECT_TRUE(base::PathExists(screenshot_path_));

  EXPECT_FALSE(IsImageClipboardAvailable());
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);

  // Copy to clipboard button.
  notification->ButtonClick(0);

  RunLoop();
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);

  EXPECT_TRUE(clipboard_changed_);
  EXPECT_TRUE(IsImageClipboardAvailable());

  g_browser_process->notification_ui_manager()->CancelAll();
}
