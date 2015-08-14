// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.BookmarksBridge;
import org.chromium.chrome.browser.offline_pages.OfflinePageBridge;
import org.chromium.chrome.browser.offline_pages.OfflinePageItem;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * A class that encapsulates {@link BookmarksBridge} and provides extra features such as undo, large
 * icon fetching, reader mode url redirecting, etc. This class should serve as the single class for
 * the UI to acquire data from the backend.
 */
public class EnhancedBookmarksModel extends BookmarksBridge {
    private static final int FAVICON_MAX_CACHE_SIZE = 10 * 1024 * 1024; // 10MB

    /**
     * Observer that listens to delete event. This interface is used by undo controllers to know
     * which bookmarks were deleted. Note this observer only listens to events that go through
     * enhanced bookmark model.
     */
    public interface EnhancedBookmarkDeleteObserver {

        /**
         * Callback being triggered immediately before bookmarks are deleted.
         * @param titles All titles of the bookmarks to be deleted.
         * @param isUndoable Whether the deletion is undoable.
         */
        void onDeleteBookmarks(String[] titles, boolean isUndoable);
    }

    private ObserverList<EnhancedBookmarkDeleteObserver> mDeleteObservers = new ObserverList<>();
    private OfflinePageBridge mOfflinePageBridge;

    /**
     * Initialize enhanced bookmark model for last used non-incognito profile.
     */
    public EnhancedBookmarksModel() {
        this(Profile.getLastUsedProfile().getOriginalProfile());
    }

    @VisibleForTesting
    public EnhancedBookmarksModel(Profile profile) {
        super(profile);

        if (OfflinePageBridge.isEnabled()) {
            // TODO(jianli): Make sure to wait until the bridge is loaded.
            mOfflinePageBridge = new OfflinePageBridge(profile);
        }
    }

    /**
     * Add an observer that listens to delete events that go through enhanced bookmark model.
     * @param observer The observer to add.
     */
    public void addDeleteObserver(EnhancedBookmarkDeleteObserver observer) {
        mDeleteObservers.addObserver(observer);
    }

    /**
     * Remove the observer from listening to bookmark deleting events.
     * @param observer The observer to remove.
     */
    public void removeDeleteObserver(EnhancedBookmarkDeleteObserver observer) {
        mDeleteObservers.removeObserver(observer);
    }

    /**
     * Delete one or multiple bookmarks from model. If more than one bookmarks are passed here, this
     * method will group these delete operations into one undo bundle so that later if the user
     * clicks undo, all bookmarks deleted here will be restored.
     * @param bookmarks Bookmarks to delete. Note this array should not contain a folder and its
     *                  children, because deleting folder will also remove all its children, and
     *                  deleting children once more will cause errors.
     */
    public void deleteBookmarks(BookmarkId... bookmarks) {
        assert bookmarks != null && bookmarks.length > 0;
        // Store all titles of bookmarks.
        String[] titles = new String[bookmarks.length];
        boolean isUndoable = true;
        for (int i = 0; i < bookmarks.length; i++) {
            titles[i] = getBookmarkTitle(bookmarks[i]);
            isUndoable &= (bookmarks[i].getType() == BookmarkType.NORMAL);
        }

        if (bookmarks.length == 1) {
            deleteBookmark(bookmarks[0]);
        } else {
            startGroupingUndos();
            for (BookmarkId bookmark : bookmarks) {
                deleteBookmark(bookmark);
            }
            endGroupingUndos();
        }

        for (EnhancedBookmarkDeleteObserver observer : mDeleteObservers) {
            observer.onDeleteBookmarks(titles, isUndoable);
        }
    }

    /**
     * Calls {@link BookmarksBridge#moveBookmark(BookmarkId, BookmarkId, int)} in a reversed
     * order of the list, in order to let the last item appear at the top.
     */
    public void moveBookmarks(List<BookmarkId> bookmarkIds, BookmarkId newParentId) {
        for (int i = bookmarkIds.size() - 1; i >= 0; i--) {
            moveBookmark(bookmarkIds.get(i), newParentId, 0);
        }
    }

    @Override
    public BookmarkId addBookmark(BookmarkId parent, int index, String title, String url) {
        url = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url);
        return super.addBookmark(parent, index, title, url);
    }

    /**
     * @see org.chromium.chrome.browser.BookmarksBridge.BookmarkItem#getTitle()
     */
    public String getBookmarkTitle(BookmarkId bookmarkId) {
        return getBookmarkById(bookmarkId).getTitle();
    }

    /**
     * @return The id of the default folder to add bookmarks/folders to.
     */
    public BookmarkId getDefaultFolder() {
        return getMobileFolderId();
    }

    /**
     * Gets a list of bookmark IDs of bookmarks that match a specified filter.
     *
     * @param filter Filter to be applied to the bookmarks.
     * @return A list of bookmark IDs of bookmarks matching the filter.
     */
    public List<BookmarkId> getBookmarkIDsByFilter(EnhancedBookmarkFilter filter) {
        assert filter == EnhancedBookmarkFilter.OFFLINE_PAGES;
        assert mOfflinePageBridge != null;

        List<BookmarkId> bookmarkIds = new ArrayList<BookmarkId>();
        for (OfflinePageItem offlinePage : mOfflinePageBridge.getAllPages()) {
            bookmarkIds.add(offlinePage.getBookmarkId());
        }
        return bookmarkIds;
    }
}
