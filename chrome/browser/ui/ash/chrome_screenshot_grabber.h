// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_SCREENSHOT_GRABBER_H_
#define CHROME_BROWSER_UI_ASH_CHROME_SCREENSHOT_GRABBER_H_

#include "ash/screenshot_delegate.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification.h"
#include "ui/snapshot/screenshot_grabber.h"

class Profile;

namespace ash {
namespace test {
class ChromeScreenshotGrabberTest;
}  // namespace test
}  // namespace ash

class ChromeScreenshotGrabber : public ash::ScreenshotDelegate,
                                public ui::ScreenshotGrabberDelegate,
                                public ui::ScreenshotGrabberObserver {
 public:
  ChromeScreenshotGrabber();
  ~ChromeScreenshotGrabber() override;

  ui::ScreenshotGrabber* screenshot_grabber() {
    return screenshot_grabber_.get();
  }

  // ash::ScreenshotDelegate:
  void HandleTakeScreenshotForAllRootWindows() override;
  void HandleTakePartialScreenshot(aura::Window* window,
                                   const gfx::Rect& rect) override;
  void HandleTakeWindowScreenshot(aura::Window* window) override;
  bool CanTakeScreenshot() override;

  // ui::ScreenshotGrabberDelegate:
  void PrepareFileAndRunOnBlockingPool(
      const base::FilePath& path,
      scoped_refptr<base::TaskRunner> blocking_task_runner,
      const FileCallback& callback_on_blocking_pool) override;

  // ui::ScreenshotGrabberObserver:
  void OnScreenshotCompleted(ui::ScreenshotGrabberObserver::Result result,
                             const base::FilePath& screenshot_path) override;

 private:
  friend class ash::test::ChromeScreenshotGrabberTest;

  Notification* CreateNotification(
      ui::ScreenshotGrabberObserver::Result screenshot_result,
      const base::FilePath& screenshot_path);

  void SetProfileForTest(Profile* profile);
  Profile* GetProfile();

  std::unique_ptr<ui::ScreenshotGrabber> screenshot_grabber_;
  Profile* profile_for_test_;

  DISALLOW_COPY_AND_ASSIGN(ChromeScreenshotGrabber);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_SCREENSHOT_GRABBER_H_
