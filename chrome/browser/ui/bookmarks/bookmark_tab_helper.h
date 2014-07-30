// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_

#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class BookmarkTabHelperDelegate;

namespace bookmarks {
struct BookmarkNodeData;
}

namespace content {
class WebContents;
}

// Per-tab class to manage bookmarks.
class BookmarkTabHelper
    : public BaseBookmarkModelObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<BookmarkTabHelper> {
 public:
  // Interface for forwarding bookmark drag and drop to extenstions.
  class BookmarkDrag {
   public:
    virtual void OnDragEnter(const bookmarks::BookmarkNodeData& data) = 0;
    virtual void OnDragOver(const bookmarks::BookmarkNodeData& data) = 0;
    virtual void OnDragLeave(const bookmarks::BookmarkNodeData& data) = 0;
    virtual void OnDrop(const bookmarks::BookmarkNodeData& data) = 0;

   protected:
    virtual ~BookmarkDrag() {}
  };

  virtual ~BookmarkTabHelper();

  void set_delegate(BookmarkTabHelperDelegate* delegate) {
    delegate_ = delegate;
  }

  // It is up to callers to call set_bookmark_drag_delegate(NULL) when
  // |bookmark_drag| is deleted since this class does not take ownership of
  // |bookmark_drag|.
  void set_bookmark_drag_delegate(BookmarkDrag* bookmark_drag) {
    bookmark_drag_ = bookmark_drag;
  }
  BookmarkDrag* bookmark_drag_delegate() { return bookmark_drag_; }

  bool is_starred() const { return is_starred_; }

  // Returns true if the bookmark bar should be shown detached.
  bool ShouldShowBookmarkBar() const;

 private:
  friend class content::WebContentsUserData<BookmarkTabHelper>;

  explicit BookmarkTabHelper(content::WebContents* web_contents);

  // Updates the starred state from the BookmarkModel. If the state has changed,
  // the delegate is notified.
  void UpdateStarredStateForCurrentURL();

  // Overridden from BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void BookmarkModelLoaded(BookmarkModel* model,
                                   bool ids_reassigned) OVERRIDE;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node,
                                   const std::set<GURL>& removed_urls) OVERRIDE;
  virtual void BookmarkAllUserNodesRemoved(
      BookmarkModel* model,
      const std::set<GURL>& removed_urls) OVERRIDE;

  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;

  // Overridden from content::WebContentsObserver:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // Whether the current URL is starred.
  bool is_starred_;

  BookmarkModel* bookmark_model_;

  // Our delegate, to notify when the url starred changed.
  BookmarkTabHelperDelegate* delegate_;

  // The BookmarkDrag is used to forward bookmark drag and drop events to
  // extensions.
  BookmarkDrag* bookmark_drag_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkTabHelper);
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_
