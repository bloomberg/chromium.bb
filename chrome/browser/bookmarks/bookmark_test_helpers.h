// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_TEST_HELPERS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_TEST_HELPERS_H_

class BookmarkModel;
class Profile;

namespace test {

// Blocks until |model| finishes loading.
void WaitForBookmarkModelToLoad(BookmarkModel* model);
void WaitForBookmarkModelToLoad(Profile* profile);

}  // namespace test

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_TEST_HELPERS_H_
