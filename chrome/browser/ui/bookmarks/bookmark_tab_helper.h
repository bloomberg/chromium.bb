// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_
#pragma once

#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class BookmarkTabHelperDelegate;
class TabContentsWrapper;
struct BookmarkNodeData;

// Per-tab class to manage bookmarks.
class BookmarkTabHelper : public content::NotificationObserver,
                          public TabContentsObserver {
 public:
  // BookmarkDrag --------------------------------------------------------------
  // Interface for forwarding bookmark drag and drop to extenstions.
  class BookmarkDrag {
   public:
    virtual void OnDragEnter(const BookmarkNodeData& data) = 0;
    virtual void OnDragOver(const BookmarkNodeData& data) = 0;
    virtual void OnDragLeave(const BookmarkNodeData& data) = 0;
    virtual void OnDrop(const BookmarkNodeData& data) = 0;

   protected:
    virtual ~BookmarkDrag() {}
  };

  explicit BookmarkTabHelper(TabContentsWrapper* tab_contents);
  virtual ~BookmarkTabHelper();

  bool is_starred() const { return is_starred_; }

  BookmarkTabHelperDelegate* delegate() const { return delegate_; }
  void set_delegate(BookmarkTabHelperDelegate* d) { delegate_ = d; }

  // Returns true if the bookmark bar should be shown detached.
  bool ShouldShowBookmarkBar();

  // TabContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // It is up to callers to call SetBookmarkDragDelegate(NULL) when
  // |bookmark_drag| is deleted since this class does not take ownership of
  // |bookmark_drag|.
  void SetBookmarkDragDelegate(
      BookmarkTabHelper::BookmarkDrag* bookmark_drag);
  // The BookmarkDragDelegate is used to forward bookmark drag and drop events
  // to extensions.
  BookmarkTabHelper::BookmarkDrag* GetBookmarkDragDelegate();

 private:
  // Updates the starred state from the bookmark bar model. If the state has
  // changed, the delegate is notified.
  void UpdateStarredStateForCurrentURL();

  // Whether the current URL is starred.
  bool is_starred_;

  // Registers and unregisters us for notifications.
  content::NotificationRegistrar registrar_;

  // Owning TabContentsWrapper.
  TabContentsWrapper* tab_contents_wrapper_;

  // Delegate for notifying our owner (usually Browser) about stuff. Not owned
  // by us.
  BookmarkTabHelperDelegate* delegate_;

  // Handles drag and drop event forwarding to extensions.
  BookmarkDrag* bookmark_drag_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkTabHelper);
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_
