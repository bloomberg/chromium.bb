// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"
#include "content/public/test/test_browser_thread.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace {

class WallpaperPrivateApiUnittest : public ash::test::AshTestBase {
 public:
  WallpaperPrivateApiUnittest()
      : ui_thread_(content::BrowserThread::UI, message_loop()) {}

 private:
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperPrivateApiUnittest);
};

class TestMinimizeFunction
    : public WallpaperPrivateMinimizeInactiveWindowsFunction {
 public:
  TestMinimizeFunction() {}

  virtual bool RunImpl() OVERRIDE {
    return WallpaperPrivateMinimizeInactiveWindowsFunction::RunImpl();
  }

 protected:
  virtual ~TestMinimizeFunction() {}
};

class TestRestoreFunction
    : public WallpaperPrivateRestoreMinimizedWindowsFunction {
 public:
  TestRestoreFunction() {}

  virtual bool RunImpl() OVERRIDE {
    return WallpaperPrivateRestoreMinimizedWindowsFunction::RunImpl();
  }
 protected:
  virtual ~TestRestoreFunction() {}
};

}  // namespace

TEST_F(WallpaperPrivateApiUnittest, HideAndRestoreWindows) {
  scoped_ptr<aura::Window> window3(CreateTestWindowInShellWithId(3));
  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  scoped_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));

  ash::wm::MinimizeWindow(window3.get());
  ash::wm::MaximizeWindow(window1.get());

  // Window 3 starts minimized, window 1 starts maximized.
  EXPECT_FALSE(ash::wm::IsWindowMinimized(window0.get()));
  EXPECT_FALSE(ash::wm::IsWindowMinimized(window1.get()));
  EXPECT_FALSE(ash::wm::IsWindowMinimized(window2.get()));
  EXPECT_TRUE(ash::wm::IsWindowMinimized(window3.get()));

  // We then activate window 0 (i.e. wallpaper picker) and call the minimize
  // function.
  ash::wm::ActivateWindow(window0.get());
  EXPECT_TRUE(ash::wm::IsActiveWindow(window0.get()));
  scoped_refptr<TestMinimizeFunction> minimize_function(
      new TestMinimizeFunction());
  EXPECT_TRUE(minimize_function->RunImpl());

  // All windows except window 0 should be minimized.
  EXPECT_FALSE(ash::wm::IsWindowMinimized(window0.get()));
  EXPECT_TRUE(ash::wm::IsWindowMinimized(window1.get()));
  EXPECT_TRUE(ash::wm::IsWindowMinimized(window2.get()));
  EXPECT_TRUE(ash::wm::IsWindowMinimized(window3.get()));

  // Then we destroy window 0 and call the restore function.
  window0.reset();
  scoped_refptr<TestRestoreFunction> restore_function(
      new TestRestoreFunction());
  EXPECT_TRUE(restore_function->RunImpl());

  // Windows 1 and 2 should no longer be minimized. Window 1 should again
  // be maximized. Window 3 should still be minimized.
  EXPECT_FALSE(ash::wm::IsWindowMinimized(window1.get()));
  EXPECT_TRUE(ash::wm::IsWindowMaximized(window1.get()));
  EXPECT_FALSE(ash::wm::IsWindowMinimized(window2.get()));
  EXPECT_TRUE(ash::wm::IsWindowMinimized(window3.get()));
}
