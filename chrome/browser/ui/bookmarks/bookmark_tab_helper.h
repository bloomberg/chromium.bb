// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_

#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

struct BookmarkNodeData;
class BookmarkTabHelperDelegate;

namespace content {
class WebContents;
}

// Per-tab class to manage bookmarks.
class BookmarkTabHelper
    : public BaseBookmarkModelObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<BookmarkTabHelper> {
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

  virtual ~BookmarkTabHelper();

  bool is_starred() const { return is_starred_; }

  BookmarkTabHelperDelegate* delegate() const { return delegate_; }
  void set_delegate(BookmarkTabHelperDelegate* d) { delegate_ = d; }

  // Returns true if the bookmark bar should be shown detached.
  bool ShouldShowBookmarkBar();

  // content::WebContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // It is up to callers to call SetBookmarkDragDelegate(NULL) when
  // |bookmark_drag| is deleted since this class does not take ownership of
  // |bookmark_drag|.
  void SetBookmarkDragDelegate(
      BookmarkTabHelper::BookmarkDrag* bookmark_drag);
  // The BookmarkDragDelegate is used to forward bookmark drag and drop events
  // to extensions.
  BookmarkTabHelper::BookmarkDrag* GetBookmarkDragDelegate();

 private:
  friend class content::WebContentsUserData<BookmarkTabHelper>;

  explicit BookmarkTabHelper(content::WebContents* web_contents);

  // Updates the starred state from the bookmark bar model. If the state has
  // changed, the delegate is notified.
  void UpdateStarredStateForCurrentURL();

  // Overridden from BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;

  // Whether the current URL is starred.
  bool is_starred_;

  BookmarkModel* bookmark_model_;

  // Delegate for notifying our owner (usually Browser) about stuff. Not owned
  // by us.
  BookmarkTabHelperDelegate* delegate_;

  // Handles drag and drop event forwarding to extensions.
  BookmarkDrag* bookmark_drag_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkTabHelper);
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_
