// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhanced_bookmarks;

import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.util.LruCache;
import android.util.Pair;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.content_public.browser.WebContents;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Access gate to C++ side enhanced bookmarks functionalities.
 */
@JNINamespace("enhanced_bookmarks::android")
public final class EnhancedBookmarksBridge {
    private static WeakReference<LruCache<String, Pair<String, Bitmap>>> sLruCache;
    private static final int SALIENT_IMAGE_MAX_CACHE_SIZE = 32 * 1024 * 1024; // 32MB

    private long mNativeEnhancedBookmarksBridge;
    private final ObserverList<FiltersObserver> mFilterObservers =
            new ObserverList<FiltersObserver>();
    private final ObserverList<SearchServiceObserver> mSearchObservers =
            new ObserverList<SearchServiceObserver>();
    private LruCache<String, Pair<String, Bitmap>> mSalientImageCache;

    /**
     * Interface for getting result back from SalientImageForUrl function.
     */
    public interface SalientImageCallback {
        /**
         * Callback method for fetching salient image.
         * @param image Salient image. This can be null if the image cannot be found.
         * @param imageUrl Url of the image. Note this is not the same as the url of the website
         *            containing the image.
         */
        @CalledByNative("SalientImageCallback")
        void onSalientImageReady(Bitmap image, String imageUrl);
    }

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

    /**
     * Creates a new enhanced bridge using the given profile.
     */
    public EnhancedBookmarksBridge(Profile profile) {
        mNativeEnhancedBookmarksBridge = nativeInit(profile);
        mSalientImageCache = getImageCache();
    }

    public void destroy() {
        assert mNativeEnhancedBookmarksBridge != 0;
        nativeDestroy(mNativeEnhancedBookmarksBridge);
        mNativeEnhancedBookmarksBridge = 0;

        mSalientImageCache = null;
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
     * Moves a bookmark to another folder, and append it at the end of the list of all children.
     * @param bookmarkId The item to be be moved
     * @param newParentId The new parent of the item
     */
    public void moveBookmark(BookmarkId bookmarkId, BookmarkId newParentId) {
        nativeMoveBookmark(mNativeEnhancedBookmarksBridge, bookmarkId, newParentId);
    }

    /**
     * Get descriptions of a given bookmark.
     * @param id The id of the bookmark to look at.
     * @return Description of the bookmark. If given a partner bookmark, this method will return an
     *         empty list.
     */
    public String getBookmarkDescription(BookmarkId id) {
        return nativeGetBookmarkDescription(mNativeEnhancedBookmarksBridge, id.getId(),
                id.getType());
    }

    /**
     * Sets the description of the given bookmark.
     */
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
     * Request bookmark salient image for the given URL. Please refer to
     * |BookmarkImageService::SalientImageForUrl|.
     * @return True if this method is executed synchronously. False if
     *         {@link SalientImageCallback#onSalientImageReady(Bitmap, String)} is called later
     *         (asynchronously).
     */
    public boolean salientImageForUrl(final String url, final SalientImageCallback callback) {
        assert callback != null;
        SalientImageCallback callbackWrapper = callback;

        if (mSalientImageCache != null) {
            Pair<String, Bitmap> cached = mSalientImageCache.get(url);
            if (cached != null) {
                callback.onSalientImageReady(cached.second, cached.first);
                return true;
            }

            callbackWrapper = new SalientImageCallback() {
                @Override
                public void onSalientImageReady(Bitmap image, String imageUrl) {
                    if (mNativeEnhancedBookmarksBridge == 0) return;

                    if (image != null) {
                        mSalientImageCache.put(url, new Pair<String, Bitmap>(imageUrl, image));
                    }
                    callback.onSalientImageReady(image, imageUrl);
                }
            };
        }

        nativeSalientImageForUrl(mNativeEnhancedBookmarksBridge, url, callbackWrapper);
        return false;
    }

    /**
     * Parses the web content of a tab, and stores salient images to local database.
     * @param webContents Contents of the tab that the user is currently in.
     */
    public void fetchImageForTab(WebContents webContents) {
        nativeFetchImageForTab(mNativeEnhancedBookmarksBridge, webContents);
    }

    /**
     * Get all filters associated with the given bookmark.
     *
     * @param bookmark The bookmark to find filters for.
     * @return Array of Strings, each representing a filter. If given a partner bookmark, this
     *         method will return an empty array.
     */
    public String[] getFiltersForBookmark(BookmarkId bookmark) {
        return nativeGetFiltersForBookmark(mNativeEnhancedBookmarksBridge, bookmark.getId(),
                bookmark.getType());
    }

    /**
     * @return Current set of known auto-filters for bookmarks.
     */
    public List<String> getFilters() {
        List<String> list =
                Arrays.asList(nativeGetFilters(mNativeEnhancedBookmarksBridge));
        return list;
    }

    /**
     * @return Return the cache if it is stored in the static weak reference, or create a new empty
     *         one if the reference is empty.
     */
    private static LruCache<String, Pair<String, Bitmap>> getImageCache() {
        ActivityManager activityManager = ((ActivityManager) ApplicationStatus
                .getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE));
        int maxSize = Math.min(activityManager.getMemoryClass() / 4 * 1024 * 1024,
                SALIENT_IMAGE_MAX_CACHE_SIZE);
        LruCache<String, Pair<String, Bitmap>> cache = sLruCache == null ? null : sLruCache.get();
        if (cache == null) {
            cache = new LruCache<String, Pair<String, Bitmap>>(maxSize) {
                @Override
                protected int sizeOf(String key, Pair<String, Bitmap> urlImage) {
                    return urlImage.first.length() + urlImage.second.getByteCount();
                }
            };
            sLruCache = new WeakReference<LruCache<String, Pair<String, Bitmap>>>(cache);
        }
        return cache;
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
    private native String[] nativeGetFiltersForBookmark(long nativeEnhancedBookmarksBridge, long id,
            int type);
    private native BookmarkId nativeAddFolder(long nativeEnhancedBookmarksBridge, BookmarkId parent,
            int index, String title);
    private native void nativeMoveBookmark(long nativeEnhancedBookmarksBridge,
            BookmarkId bookmarkId, BookmarkId newParentId);
    private native BookmarkId nativeAddBookmark(long nativeEnhancedBookmarksBridge,
            BookmarkId parent, int index, String title, String url);
    private native void nativeSendSearchRequest(long nativeEnhancedBookmarksBridge, String query);
    private native void nativeSalientImageForUrl(long nativeEnhancedBookmarksBridge,
            String url, SalientImageCallback callback);
    private native void nativeFetchImageForTab(long nativeEnhancedBookmarksBridge,
            WebContents webContents);
}
