// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_REMOVER_HELPER_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_REMOVER_HELPER_H_

#include "base/callback.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"

class ChromeBrowserState;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

// Helper class to asynchronously remove bookmarks.
class BookmarkRemoverHelper : public bookmarks::BaseBookmarkModelObserver {
 public:
  using Callback = base::OnceCallback<void(bool)>;

  explicit BookmarkRemoverHelper(ChromeBrowserState* browser_state);
  ~BookmarkRemoverHelper() override;

  // Removes all bookmarks and asynchronously invoke |completion| with
  // boolean indicating success or failure.
  void RemoveAllUserBookmarksIOS(Callback completion);

  // BaseBookmarkModelObserver implementation.
  void BookmarkModelChanged() override;

  // BookmarkModelObserver implementation.
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;

 private:
  // Invoked when the bookmark entries have been deleted. Invoke the
  // completion callback with |success| (invocation is asynchronous so
  // the object won't be deleted immediately).
  void BookmarksRemoved(bool success);

  Callback completion_;
  ChromeBrowserState* browser_state_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BookmarkRemoverHelper);
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_REMOVER_HELPER_H_
