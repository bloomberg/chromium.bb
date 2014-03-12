// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_SCOPED_GROUP_BOOKMARK_ACTIONS_H_
#define CHROME_BROWSER_BOOKMARKS_SCOPED_GROUP_BOOKMARK_ACTIONS_H_

#include "base/macros.h"

class BookmarkModel;
class Profile;

// Scopes the grouping of a set of changes into one undoable action.
class ScopedGroupBookmarkActions {
 public:
  explicit ScopedGroupBookmarkActions(Profile* profile);
  explicit ScopedGroupBookmarkActions(BookmarkModel* model);
  ~ScopedGroupBookmarkActions();

 private:
  BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(ScopedGroupBookmarkActions);
};

#endif  // CHROME_BROWSER_BOOKMARKS_SCOPED_GROUP_BOOKMARK_ACTIONS_H_
