// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SCREENSHOT_TAKER_H_
#define CHROME_BROWSER_UI_ASH_SCREENSHOT_TAKER_H_

#include "ash/screenshot_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/notification.h"

class Profile;

namespace ash {
namespace test {
class ScreenshotTakerTest;
}
}
namespace aura {
class Window;
}

class ScreenshotTakerObserver {
 public:
  enum Result {
    SCREENSHOT_SUCCESS = 0,
    SCREENSHOT_GRABWINDOW_PARTIAL_FAILED,
    SCREENSHOT_GRABWINDOW_FULL_FAILED,
    SCREENSHOT_CREATE_DIR_FAILED,
    SCREENSHOT_GET_DIR_FAILED,
    SCREENSHOT_CHECK_DIR_FAILED,
    SCREENSHOT_CREATE_FILE_FAILED,
    SCREENSHOT_WRITE_FILE_FAILED,
    SCREENSHOTS_DISABLED,
    SCREENSHOT_RESULT_COUNT
  };

  virtual void OnScreenshotCompleted(
      Result screenshot_result,
      const base::FilePath& screenshot_path) = 0;

 protected:
  virtual ~ScreenshotTakerObserver() {}
};

class ScreenshotTaker : public ash::ScreenshotDelegate {
 public:
  ScreenshotTaker();

  virtual ~ScreenshotTaker();

  // Overridden from ash::ScreenshotDelegate:
  virtual void HandleTakeScreenshotForAllRootWindows() OVERRIDE;
  virtual void HandleTakePartialScreenshot(aura::Window* window,
                                           const gfx::Rect& rect) OVERRIDE;
  virtual bool CanTakeScreenshot() OVERRIDE;

  void ShowNotification(
      ScreenshotTakerObserver::Result screenshot_result,
      const base::FilePath& screenshot_path);

  void AddObserver(ScreenshotTakerObserver* observer);
  void RemoveObserver(ScreenshotTakerObserver* observer);
  bool HasObserver(ScreenshotTakerObserver* observer) const;

 private:
  friend class ash::test::ScreenshotTakerTest;

  void GrabWindowSnapshotAsyncCallback(
      base::FilePath screenshot_path,
      bool is_partial,
      int window_idx,
      scoped_refptr<base::RefCountedBytes> png_data);
  void GrabPartialWindowSnapshotAsync(aura::Window* window,
                                      const gfx::Rect& snapshot_bounds,
                                      Profile* profile,
                                      base::FilePath screenshot_path);
  void GrabFullWindowSnapshotAsync(aura::Window* window,
                                   const gfx::Rect& snapshot_bounds,
                                   Profile* profile,
                                   base::FilePath screenshot_path,
                                   int window_idx);

  Profile* GetProfile();
  void SetScreenshotDirectoryForTest(const base::FilePath& directory);
  void SetScreenshotBasenameForTest(const std::string& basename);
  void SetScreenshotProfileForTest(Profile* profile);

#if defined(OS_CHROMEOS)
  Notification* CreateNotification(
      ScreenshotTakerObserver::Result screenshot_result,
      const base::FilePath& screenshot_path);
#endif

  // The timestamp when the screenshot task was issued last time.
  base::Time last_screenshot_timestamp_;

  ObserverList<ScreenshotTakerObserver> observers_;

  base::FilePath screenshot_directory_for_test_;
  std::string screenshot_basename_for_test_;
  Profile* profile_for_test_;

  base::WeakPtrFactory<ScreenshotTaker> factory_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotTaker);
};

#endif  // CHROME_BROWSER_UI_ASH_SCREENSHOT_TAKER_H_
