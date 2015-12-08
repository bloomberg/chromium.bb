// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.os.AsyncTask;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.bookmark.BookmarksBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.offlinepages.DeletePageResult;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Access gate to C++ side offline pages functionalities.
 */
@JNINamespace("offline_pages::android")
public final class OfflinePageBridge {

    private long mNativeOfflinePageBridge;
    private boolean mIsNativeOfflinePageModelLoaded;
    private final ObserverList<OfflinePageModelObserver> mObservers =
            new ObserverList<OfflinePageModelObserver>();

    /** Whether the offline pages feature is enabled. */
    private static Boolean sIsEnabled;

    /**
     * Callback used to saving an offline page.
     */
    public interface SavePageCallback {
        /**
         * Delivers result of saving a page.
         *
         * @param savePageResult Result of the saving. Uses
         *     {@see org.chromium.components.offlinepages.SavePageResult} enum.
         * @param url URL of the saved page.
         * @see OfflinePageBridge#savePage()
         */
        @CalledByNative("SavePageCallback")
        void onSavePageDone(int savePageResult, String url);
    }

    /**
     * Callback used to deleting an offline page.
     */
    public interface DeletePageCallback {
        /**
         * Delivers result of deleting a page.
         *
         * @param deletePageResult Result of deleting the page. Uses
         *     {@see org.chromium.components.offlinepages.DeletePageResult} enum.
         * @see OfflinePageBridge#deletePage()
         */
        @CalledByNative("DeletePageCallback")
        void onDeletePageDone(int deletePageResult);
    }

    /**
     * Base observer class listeners to be notified of changes to the offline page model.
     */
    public abstract static class OfflinePageModelObserver {
        /**
         * Called when the native side of offline pages is loaded and now in usable state.
         */
        public void offlinePageModelLoaded() {}

        /**
         * Called when the native side of offline pages is changed due to adding, removing or
         * update an offline page.
         */
        public void offlinePageModelChanged() {}

        /**
         * Called when an offline page is deleted. This can be called as a result of
         * #checkOfflinePageMetadata().
         * @param bookmarkId A bookmark ID of the deleted offline page.
         */
        public void offlinePageDeleted(BookmarkId bookmarkId) {}
    }

    private static long getTotalSize(List<OfflinePageItem> offlinePages) {
        long totalSize = 0;
        for (OfflinePageItem offlinePage : offlinePages) {
            totalSize += offlinePage.getFileSize();
        }
        return totalSize;
    }

    private static void recordFreeSpaceHistograms(
            final String percentageName, final String bytesName) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                int percentage = (int) (1.0 * OfflinePageUtils.getFreeSpaceInBytes()
                        / OfflinePageUtils.getTotalSpaceInBytes() * 100);
                RecordHistogram.recordEnumeratedHistogram(percentageName, percentage, 101);
                int bytesInMB = (int) (OfflinePageUtils.getFreeSpaceInBytes() / (1024 * 1024));
                RecordHistogram.recordCustomCountHistogram(bytesName, bytesInMB, 1, 500000, 50);
                return null;
            }
        }.execute();
    }

    /**
     * Records histograms related to the cost of storage. It is meant to be used after user
     * takes an action: save, delete or delete in bulk.
     *
     * @param totalPageSizeBefore Total size of the pages before the action was taken.
     * @param totalPageSizeAfter Total size of the pages after the action was taken.
     */
    private static void recordStorageHistograms(
            final long totalPageSizeBefore, final long totalPageSizeAfter) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                // Total space taken by offline pages.
                int totalPageSizeInMB = (int) (totalPageSizeAfter / (1024 * 1024));
                RecordHistogram.recordCustomCountHistogram(
                        "OfflinePages.TotalPageSize", totalPageSizeInMB, 1, 10000, 50);

                // How much of the total space the offline pages take.
                int totalPageSizePercentage = (int) (1.0 * totalPageSizeAfter
                        / OfflinePageUtils.getTotalSpaceInBytes() * 100);
                RecordHistogram.recordEnumeratedHistogram(
                        "OfflinePages.TotalPageSizePercentage", totalPageSizePercentage, 101);
                if (totalPageSizeBefore > 0) {
                    // If the user is deleting the pages, perhaps they are running out of free
                    // space. Report the size before the operation, where a base for calculation
                    // of total free space includes space taken by offline pages.
                    int percentageOfFree = (int) (1.0 * totalPageSizeBefore
                            / (totalPageSizeBefore + OfflinePageUtils.getFreeSpaceInBytes()) * 100);
                    RecordHistogram.recordEnumeratedHistogram(
                            "OfflinePages.DeletePage.TotalPageSizeAsPercentageOfFreeSpace",
                            percentageOfFree, 101);
                }
                return null;
            }
        }.execute();
    }

    /**
     * Creates offline pages bridge for a given profile.
     */
    @VisibleForTesting
    public OfflinePageBridge(Profile profile) {
        mNativeOfflinePageBridge = nativeInit(profile);
    }

    /**
     * Returns true if the offline pages feature is enabled.
     */
    public static boolean isEnabled() {
        ThreadUtils.assertOnUiThread();
        if (sIsEnabled == null) {
            // Enhanced bookmarks feature should also be enabled.
            sIsEnabled = nativeIsOfflinePagesEnabled()
                    && BookmarksBridge.isEnhancedBookmarksEnabled();
        }
        return sIsEnabled;
    }

    /**
     * @return True if an offline copy of the given URL can be saved.
     */
    public static boolean canSavePage(String url) {
        return nativeCanSavePage(url);
    }

    /**
     * Destroys native offline pages bridge. It should be called during
     * destruction to ensure proper cleanup.
     */
    public void destroy() {
        assert mNativeOfflinePageBridge != 0;
        nativeDestroy(mNativeOfflinePageBridge);
        mIsNativeOfflinePageModelLoaded = false;
        mNativeOfflinePageBridge = 0;
    }

    /**
     * Adds an observer to offline page model changes.
     * @param observer The observer to be added.
     */
    @VisibleForTesting
    public void addObserver(OfflinePageModelObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer to offline page model changes.
     * @param observer The observer to be removed.
     */
    @VisibleForTesting
    public void removeObserver(OfflinePageModelObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @return Gets all available offline pages. Requires that the model is already loaded.
     */
    @VisibleForTesting
    public List<OfflinePageItem> getAllPages() {
        assert mIsNativeOfflinePageModelLoaded;
        List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        nativeGetAllPages(mNativeOfflinePageBridge, result);
        return result;
    }

    /**
     * Gets an offline page associated with a provided bookmark ID.
     *
     * @param bookmarkId Id of the bookmark associated with an offline page.
     * @return An {@link OfflinePageItem} matching the bookmark Id or <code>null</code> if none
     * exist.
     */
    @VisibleForTesting
    public OfflinePageItem getPageByBookmarkId(BookmarkId bookmarkId) {
        return nativeGetPageByBookmarkId(mNativeOfflinePageBridge, bookmarkId.getId());
    }

    /**
     * Saves the web page loaded into web contents offline.
     *
     * @param webContents Contents of the page to save.
     * @param bookmarkId Id of the bookmark related to the offline page.
     * @param callback Interface that contains a callback.
     * @see SavePageCallback
     */
    @VisibleForTesting
    public void savePage(final WebContents webContents, final BookmarkId bookmarkId,
            final SavePageCallback callback) {
        assert mIsNativeOfflinePageModelLoaded;
        assert webContents != null;

        SavePageCallback callbackWrapper = new SavePageCallback() {
            @Override
            public void onSavePageDone(int savePageResult, String url) {
                // TODO(fgorski): Eliminate call to getAllPages() here.
                // See http://crbug.com/566939
                if (savePageResult == SavePageResult.SUCCESS && isOfflinePageModelLoaded()) {
                    long totalPageSizeAfter = getTotalSize(getAllPages());
                    recordStorageHistograms(0, totalPageSizeAfter);
                }
                callback.onSavePageDone(savePageResult, url);
            }
        };
        recordFreeSpaceHistograms(
                "OfflinePages.SavePage.FreeSpacePercentage", "OfflinePages.SavePage.FreeSpaceMB");
        nativeSavePage(mNativeOfflinePageBridge, callbackWrapper, webContents, bookmarkId.getId());
    }

    /**
     * Marks that an offline page related to a specified bookmark has been accessed.
     *
     * @param bookmarkId Bookmark ID for which the offline copy will be deleted.
     */
    public void markPageAccessed(BookmarkId bookmarkId) {
        assert mIsNativeOfflinePageModelLoaded;
        nativeMarkPageAccessed(mNativeOfflinePageBridge, bookmarkId.getId());
    }

    /**
     * Deletes an offline page related to a specified bookmark.
     *
     * @param bookmarkId Bookmark ID for which the offline copy will be deleted.
     * @param callback Interface that contains a callback.
     * @see DeletePageCallback
     */
    @VisibleForTesting
    public void deletePage(final BookmarkId bookmarkId, DeletePageCallback callback) {
        assert mIsNativeOfflinePageModelLoaded;

        recordFreeSpaceHistograms("OfflinePages.DeletePage.FreeSpacePercentage",
                "OfflinePages.DeletePage.FreeSpaceMB");

        DeletePageCallback callbackWrapper = wrapCallbackWithHistogramReporting(callback);
        nativeDeletePage(mNativeOfflinePageBridge, callbackWrapper, bookmarkId.getId());
    }

    /**
     * Deletes offline pages based on the list of provided bookamrk IDs. Calls the callback
     * when operation is complete. Requires that the model is already loaded.
     *
     * @param bookmarkIds A list of bookmark IDs for which the offline pages will be deleted.
     * @param callback A callback that will be called once operation is completed.
     */
    public void deletePages(List<BookmarkId> bookmarkIds, DeletePageCallback callback) {
        assert mIsNativeOfflinePageModelLoaded;
        long[] ids = new long[bookmarkIds.size()];
        for (int i = 0; i < ids.length; i++) {
            ids[i] = bookmarkIds.get(i).getId();
        }

        recordFreeSpaceHistograms("OfflinePages.DeletePage.FreeSpacePercentage",
                "OfflinePages.DeletePage.FreeSpaceMB");

        DeletePageCallback callbackWrapper = wrapCallbackWithHistogramReporting(callback);
        nativeDeletePages(mNativeOfflinePageBridge, callbackWrapper, ids);
    }

    /**
     * Whether or not the underlying offline page model is loaded.
     */
    public boolean isOfflinePageModelLoaded() {
        return mIsNativeOfflinePageModelLoaded;
    }

    /**
     * @return Gets a list of pages that will be removed to clean up storage.  Requires that the
     *     model is already loaded.
     */
    public List<OfflinePageItem> getPagesToCleanUp() {
        assert mIsNativeOfflinePageModelLoaded;
        List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        nativeGetPagesToCleanUp(mNativeOfflinePageBridge, result);
        return result;
    }

    /**
     * Starts a check of offline page metadata, e.g. are all offline copies present.
     */
    public void checkOfflinePageMetadata() {
        nativeCheckMetadataConsistency(mNativeOfflinePageBridge);
    }

    private DeletePageCallback wrapCallbackWithHistogramReporting(
            final DeletePageCallback callback) {
        final long totalPageSizeBefore = getTotalSize(getAllPages());
        return new DeletePageCallback() {
            @Override
            public void onDeletePageDone(int deletePageResult) {
                // TODO(fgorski): Eliminate call to getAllPages() here.
                // See http://crbug.com/566939
                if (deletePageResult == DeletePageResult.SUCCESS && isOfflinePageModelLoaded()) {
                    long totalPageSizeAfter = getTotalSize(getAllPages());
                    recordStorageHistograms(totalPageSizeBefore, totalPageSizeAfter);
                }
                callback.onDeletePageDone(deletePageResult);
            }
        };
    }

    @CalledByNative
    private void offlinePageModelLoaded() {
        mIsNativeOfflinePageModelLoaded = true;
        for (OfflinePageModelObserver observer : mObservers) {
            observer.offlinePageModelLoaded();
        }
    }

    @CalledByNative
    private void offlinePageModelChanged() {
        for (OfflinePageModelObserver observer : mObservers) {
            observer.offlinePageModelChanged();
        }
    }

    @CalledByNative
    private void offlinePageDeleted(long bookmarkId) {
        BookmarkId id = new BookmarkId(bookmarkId, BookmarkType.NORMAL);
        for (OfflinePageModelObserver observer : mObservers) {
            observer.offlinePageDeleted(id);
        }
    }

    @CalledByNative
    private static void createOfflinePageAndAddToList(List<OfflinePageItem> offlinePagesList,
            String url, long bookmarkId, String offlineUrl, long fileSize, long creationTime,
            int accessCount, long lastAccessTimeMs) {
        offlinePagesList.add(createOfflinePageItem(
                url, bookmarkId, offlineUrl, fileSize, creationTime, accessCount,
                lastAccessTimeMs));
    }

    @CalledByNative
    private static OfflinePageItem createOfflinePageItem(String url, long bookmarkId,
            String offlineUrl, long fileSize, long creationTime, int accessCount,
            long lastAccessTimeMs) {
        return new OfflinePageItem(
                url, bookmarkId, offlineUrl, fileSize, creationTime, accessCount, lastAccessTimeMs);
    }

    private static native boolean nativeIsOfflinePagesEnabled();
    private static native boolean nativeCanSavePage(String url);

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeOfflinePageBridge);
    private native void nativeGetAllPages(
            long nativeOfflinePageBridge, List<OfflinePageItem> offlinePages);
    private native OfflinePageItem nativeGetPageByBookmarkId(
            long nativeOfflinePageBridge, long bookmarkId);
    private native void nativeSavePage(long nativeOfflinePageBridge, SavePageCallback callback,
            WebContents webContents, long bookmarkId);
    private native void nativeMarkPageAccessed(long nativeOfflinePageBridge, long bookmarkId);
    private native void nativeDeletePage(long nativeOfflinePageBridge,
            DeletePageCallback callback, long bookmarkId);
    private native void nativeDeletePages(
            long nativeOfflinePageBridge, DeletePageCallback callback, long[] bookmarkIds);
    private native void nativeGetPagesToCleanUp(
            long nativeOfflinePageBridge, List<OfflinePageItem> offlinePages);
    private native void nativeCheckMetadataConsistency(long nativeOfflinePageBridge);
}
