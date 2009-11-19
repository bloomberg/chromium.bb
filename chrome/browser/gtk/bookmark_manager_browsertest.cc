// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/keyboard_codes.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/gtk/bookmark_manager_gtk.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

class BookmarkManagerTest : public InProcessBrowserTest,
                            public BookmarkModelObserver {
 public:
  void Init() {
    model_ = browser()->profile()->GetBookmarkModel();

    if (!model_->IsLoaded()) {
      model_->AddObserver(this);
      ui_test_utils::RunMessageLoop();
      model_->RemoveObserver(this);
    }
    ASSERT_TRUE(model_->IsLoaded());

    BookmarkManagerGtk::Show(browser()->profile());
    manager_ = BookmarkManagerGtk::GetCurrentManager();
    ASSERT_TRUE(manager_);
  }

  void CleanUp() {
    // Close the window.
    if (manager_->window_) {
      ui_controls::SendKeyPressNotifyWhenDone(GTK_WINDOW(manager_->window_),
                                              base::VKEY_W,
                                              true, false, false,
                                              new MessageLoop::QuitTask());
      ui_test_utils::RunMessageLoop();
    }
    EXPECT_FALSE(BookmarkManagerGtk::GetCurrentManager());
  }

  // BookmarkModelObserver implementation. -------------------------------------
  virtual void Loaded(BookmarkModel* model) {
    MessageLoop::current()->Quit();
  }
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) {}
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) {}
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) {}
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) {}
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {}

 protected:
  BookmarkModel* model_;
  BookmarkManagerGtk* manager_;
};

// There was once a regression where we crashed when launching the bookmark
// manager, and another regression where we crashed when calling
// RecursiveFind(). This test aims to check for these simple crashes.
IN_PROC_BROWSER_TEST_F(BookmarkManagerTest, Crash) {
  Init();

  // Make sure RecursiveFind() is run.
  manager_->SelectInTree(model_->GetBookmarkBarNode(), true);

  CleanUp();
}
