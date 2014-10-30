// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Access gate to C++ side enhanced bookmarks functionalities.
 */
@JNINamespace("enhanced_bookmarks::android")
public final class EnhancedBookmarksBridge {
    private long mNativeEnhancedBookmarksBridge;
    private final ObserverList<FiltersObserver> mFilterObservers =
            new ObserverList<FiltersObserver>();
    private final ObserverList<SearchServiceObserver> mSearchObservers =
            new ObserverList<SearchServiceObserver>();

    /**
     * Interface to provide consumers notifications to changes in clusters
     */
    public interface FiltersObserver {
        /**
         * Invoked when client detects that filters have been added/removed from the server.
         */
        void onFiltersChanged();
    }

    /**
     * Interface to provide consumers notifications to changes in search service results.
     */
    public interface SearchServiceObserver {
        /**
         * Invoked when client detects that search results have been updated. This callback is
         * guaranteed to be called only once and only for the most recent query.
         */
        void onSearchResultsReturned();
    }

    public EnhancedBookmarksBridge(Profile profile) {
        mNativeEnhancedBookmarksBridge = nativeInit(profile);
    }

    public void destroy() {
        assert mNativeEnhancedBookmarksBridge != 0;
        nativeDestroy(mNativeEnhancedBookmarksBridge);
        mNativeEnhancedBookmarksBridge = 0;
    }

    /**
     * Adds a folder to the EnhancedBookmarkModel
     * @param parent The parent of this folder
     * @param index The position this folder should appear within the parent
     * @param title The title of the bookmark
     * @return The ID of the newly created folder.
     */
    public BookmarkId addFolder(BookmarkId parent, int index, String title) {
        return nativeAddFolder(mNativeEnhancedBookmarksBridge, parent, index, title);
    }

    /**
     * Adds a Bookmark to the EnhancedBookmarkModel
     * @param parent The parent of this bookmark
     * @param index The position this bookmark should appear within the parent
     * @param title The title of the bookmark
     * @param url URL of the bookmark
     * @return The ID of the newly created bookmark
     */
    public BookmarkId addBookmark(BookmarkId parent, int index, String title, String url) {
        return nativeAddBookmark(mNativeEnhancedBookmarksBridge, parent, index, title, url);
    }

    /**
     * Moves a Bookmark in the EnhancedBookmarkModel
     * @param bookmarkId The item to be be moved
     * @param newParentId The new parent of the item
     * @param index New position index of bookmark within the parent
     */
    public void moveBookmark(BookmarkId bookmarkId, BookmarkId newParentId, int index) {
        nativeMoveBookmark(mNativeEnhancedBookmarksBridge, bookmarkId, newParentId, index);
    }

    public String getBookmarkDescription(BookmarkId id) {
        return nativeGetBookmarkDescription(mNativeEnhancedBookmarksBridge, id.getId(),
                id.getType());
    }

    public void setBookmarkDescription(BookmarkId id, String description) {
        nativeSetBookmarkDescription(mNativeEnhancedBookmarksBridge, id.getId(), id.getType(),
                description);
    }

    /**
     * Registers a FiltersObserver to listen for filter change notifications.
     * @param observer Observer to add
     */
    public void addFiltersObserver(FiltersObserver observer) {
        mFilterObservers.addObserver(observer);
    }

    /**
     * Unregisters a FiltersObserver from listening to filter change notifications.
     * @param observer Observer to remove
     */
    public void removeFiltersObserver(FiltersObserver observer) {
        mFilterObservers.removeObserver(observer);
    }

    /**
     * Gets all the bookmark ids associated with a filter string.
     * @param filter The filter string
     * @return List of bookmark ids
     */
    public List<BookmarkId> getBookmarksForFilter(String filter) {
        List<BookmarkId> list = new ArrayList<BookmarkId>();
        nativeGetBookmarksForFilter(mNativeEnhancedBookmarksBridge, filter, list);
        return list;
    }

    /**
     * Sends request to search server for querying related bookmarks.
     * @param query Keyword used to find related bookmarks.
     */
    public void sendSearchRequest(String query) {
        nativeSendSearchRequest(mNativeEnhancedBookmarksBridge, query);
    }

    /**
     * Get list of bookmarks as result of a search request that was sent before in
     * {@link EnhancedBookmarksBridge#sendSearchRequest(String)}. Normally this function should be
     * called after {@link SearchServiceObserver#onSearchResultsReturned()}
     * @param query Keyword used to find related bookmarks.
     * @return List of BookmarkIds that are related to query. It will be null if the request is
     *         still on the fly, or empty list if there are no results for the query.
     */
    public List<BookmarkId> getSearchResultsForQuery(String query) {
        return nativeGetSearchResults(mNativeEnhancedBookmarksBridge, query);
    }

    /**
     * Registers a SearchObserver that listens to search request updates.
     * @param observer Observer to add
     */
    public void addSearchObserver(SearchServiceObserver observer) {
        mSearchObservers.addObserver(observer);
    }

    /**
     * Unregisters a SearchObserver that listens to search request updates.
     * @param observer Observer to remove
     */
    public void removeSearchObserver(SearchServiceObserver observer) {
        mSearchObservers.removeObserver(observer);
    }

    /**
     * @return Current set of known auto-filters for bookmarks.
     */
    public List<String> getFilters() {
        List<String> list =
                Arrays.asList(nativeGetFilters(mNativeEnhancedBookmarksBridge));
        return list;
    }

    @CalledByNative
    private void onFiltersChanged() {
        for (FiltersObserver observer : mFilterObservers) {
            observer.onFiltersChanged();
        }
    }

    @CalledByNative
    private void onSearchResultReturned() {
        for (SearchServiceObserver observer : mSearchObservers) {
            observer.onSearchResultsReturned();
        }
    }

    @CalledByNative
    private static List<BookmarkId> createBookmarkIdList() {
        return new ArrayList<BookmarkId>();
    }

    @CalledByNative
    private static void addToBookmarkIdList(List<BookmarkId> bookmarkIdList, long id, int type) {
        bookmarkIdList.add(new BookmarkId(id, type));
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeEnhancedBookmarksBridge);
    private native String nativeGetBookmarkDescription(long nativeEnhancedBookmarksBridge, long id,
            int type);
    private native void nativeSetBookmarkDescription(long nativeEnhancedBookmarksBridge, long id,
            int type, String description);
    private native void nativeGetBookmarksForFilter(long nativeEnhancedBookmarksBridge,
            String filter, List<BookmarkId> list);
    private native List<BookmarkId> nativeGetSearchResults(long nativeEnhancedBookmarksBridge,
            String query);
    private native String[] nativeGetFilters(long nativeEnhancedBookmarksBridge);
    private native BookmarkId nativeAddFolder(long nativeEnhancedBookmarksBridge, BookmarkId parent,
            int index, String title);
    private native void nativeMoveBookmark(long nativeEnhancedBookmarksBridge,
            BookmarkId bookmarkId, BookmarkId newParentId, int index);
    private native BookmarkId nativeAddBookmark(long nativeEnhancedBookmarksBridge,
            BookmarkId parent, int index, String title, String url);
    private native void nativeSendSearchRequest(long nativeEnhancedBookmarksBridge, String query);
}
