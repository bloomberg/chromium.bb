// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.components.bookmarks.BookmarkId;

import java.util.List;

/**
 * Observer interface to get notification for UI mode changes, bookmark changes, and other related
 * event that affects UI. All enhanced bookmark UI components are expected to implement this and
 * update themselves correctly on each event.
 */
interface BookmarkUIObserver {
    void onBookmarkDelegateInitialized(BookmarkDelegate delegate);

    /**
     * Called when the entire UI is being destroyed and will be no longer in use.
     */
    void onDestroy();

    /**
     * @see BookmarkDelegate#openAllBookmarks()
     */
    void onAllBookmarksStateSet();

    /**
     * @see BookmarkDelegate#openFolder(BookmarkId)
     */
    void onFolderStateSet(BookmarkId folder);

    /**
     * @see BookmarkDelegate#openFilter(BookmarkFilter)
     */
    void onFilterStateSet(BookmarkFilter filter);

    /**
     * Please refer to
     * {@link BookmarkDelegate#toggleSelectionForBookmark(BookmarkId)},
     * {@link BookmarkDelegate#clearSelection()} and
     * {@link BookmarkDelegate#getSelectedBookmarks()}
     */
    void onSelectionStateChange(List<BookmarkId> selectedBookmarks);
}
