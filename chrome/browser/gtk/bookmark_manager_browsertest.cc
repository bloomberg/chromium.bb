// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/keyboard_codes.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/bookmark_manager_gtk.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

class BookmarkManagerTest : public InProcessBrowserTest {
};

// There was once a regression where we crashed when launching the bookmark
// manager, and another regression where we crashed when calling
// RecursiveFind(). This test aims to check for these simple crashes.
IN_PROC_BROWSER_TEST_F(BookmarkManagerTest, Crash) {
  BookmarkManagerGtk::Show(browser()->profile());
  BookmarkManagerGtk* manager = BookmarkManagerGtk::GetCurrentManager();
  ASSERT_TRUE(manager);

  // Make sure RecursiveFind() is run.
  manager->SelectInTree(manager->model_->GetBookmarkBarNode(), true);

  // Close the window.
  ui_controls::SendKeyPressNotifyWhenDone(GTK_WINDOW(manager->window_),
                                          base::VKEY_W,
                                          true, false, false,
                                          new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();
  ASSERT_FALSE(BookmarkManagerGtk::GetCurrentManager());
}
