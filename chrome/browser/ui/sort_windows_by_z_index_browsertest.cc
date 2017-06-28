// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sort_windows_by_z_index.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"

using SortWindowsByZIndexBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SortWindowsByZIndexBrowserTest, SortWindowsByZIndex) {
  Browser* browser1 = BrowserList::GetInstance()->GetLastActive();
  chrome::NewWindow(browser1);
  Browser* browser2 = BrowserList::GetInstance()->GetLastActive();
  EXPECT_NE(browser1, browser2);
  chrome::NewWindow(browser1);
  Browser* browser3 = BrowserList::GetInstance()->GetLastActive();
  EXPECT_NE(browser1, browser3);
  EXPECT_NE(browser2, browser3);
  chrome::NewWindow(browser1);
  Browser* browser4 = BrowserList::GetInstance()->GetLastActive();
  EXPECT_NE(browser1, browser4);
  EXPECT_NE(browser2, browser4);
  EXPECT_NE(browser3, browser4);

  gfx::NativeWindow window1 = browser1->window()->GetNativeWindow();
  gfx::NativeWindow window2 = browser2->window()->GetNativeWindow();
  gfx::NativeWindow window3 = browser3->window()->GetNativeWindow();
  gfx::NativeWindow window4 = browser4->window()->GetNativeWindow();

  std::vector<gfx::NativeWindow> expected_sorted_windows{window4, window3,
                                                         window2, window1};

  bool callback_did_run = false;
  ui::SortWindowsByZIndex(
      {window1, window3, window2, window4},
      base::BindOnce(
          [](std::vector<gfx::NativeWindow> expected_sorted_windows,
             bool* callback_did_run,
             std::vector<gfx::NativeWindow> sorted_windows) {
            EXPECT_EQ(expected_sorted_windows, sorted_windows);
            *callback_did_run = true;
          },
          std::move(expected_sorted_windows),
          base::Unretained(&callback_did_run)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_did_run);
}
