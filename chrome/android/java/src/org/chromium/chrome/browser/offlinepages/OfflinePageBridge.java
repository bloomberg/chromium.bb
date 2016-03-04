// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.os.AsyncTask;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.offlinepages.DeletePageResult;
import org.chromium.components.offlinepages.FeatureMode;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Access gate to C++ side offline pages functionalities.
 */
@JNINamespace("offline_pages::android")
public final class OfflinePageBridge {
    public static final String BOOKMARK_NAMESPACE = "bookmark";
    public static final long INVALID_OFFLINE_ID = 0;

    private long mNativeOfflinePageBridge;
    private boolean mIsNativeOfflinePageModelLoaded;
    private final ObserverList<OfflinePageModelObserver> mObservers =
            new ObserverList<OfflinePageModelObserver>();

    /** Mode of the offline pages feature */
    private static Integer sFeatureMode;

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
        void onSavePageDone(int savePageResult, String url, long offlineId);
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
         * TODO(bburns): Switch to offline id/client id
         * #checkOfflinePageMetadata().
         * @param bookmarkId A bookmark ID of the deleted offline page.
         */
        public void offlinePageDeleted(BookmarkId id) {}
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
                RecordHistogram.recordPercentageHistogram(percentageName, percentage);
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
                RecordHistogram.recordPercentageHistogram(
                        "OfflinePages.TotalPageSizePercentage", totalPageSizePercentage);
                if (totalPageSizeBefore > 0) {
                    // If the user is deleting the pages, perhaps they are running out of free
                    // space. Report the size before the operation, where a base for calculation
                    // of total free space includes space taken by offline pages.
                    int percentageOfFree = (int) (1.0 * totalPageSizeBefore
                            / (totalPageSizeBefore + OfflinePageUtils.getFreeSpaceInBytes()) * 100);
                    RecordHistogram.recordPercentageHistogram(
                            "OfflinePages.DeletePage.TotalPageSizeAsPercentageOfFreeSpace",
                            percentageOfFree);
                }
                return null;
            }
        }.execute();
    }

    /**
     * Creates offline pages bridge for a given profile.
     */
    public OfflinePageBridge(Profile profile) {
        mNativeOfflinePageBridge = nativeInit(profile);
    }

    /**
     * @return The mode of the offline pages feature. Uses
     *     {@see org.chromium.components.offlinepages.FeatureMode} enum.
     */
    public static int getFeatureMode() {
        ThreadUtils.assertOnUiThread();
        if (sFeatureMode == null) sFeatureMode = nativeGetFeatureMode();
        return sFeatureMode;
    }

    /**
     * @return True if the offline pages feature is enabled, regardless whether bookmark or saved
     *     page shown in UI strings.
     */
    public static boolean isEnabled() {
        ThreadUtils.assertOnUiThread();
        return getFeatureMode() != FeatureMode.DISABLED;
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
    public void addObserver(OfflinePageModelObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer to offline page model changes.
     * @param observer The observer to be removed.
     */
    public void removeObserver(OfflinePageModelObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @return Gets all available offline pages. Requires that the model is already loaded.
     */
    public List<OfflinePageItem> getAllPages() {
        assert mIsNativeOfflinePageModelLoaded;
        List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        nativeGetAllPages(mNativeOfflinePageBridge, result);
        return result;
    }

    /**
     * @return A list of all offline ids that match a particular
     * (namespace, client_id)
     */
    private Set<Long> getOfflineIdsForClientId(String clientIdNamespace, String clientId) {
        assert mIsNativeOfflinePageModelLoaded;
        long[] offlineIds = nativeGetOfflineIdsForClientId(
                mNativeOfflinePageBridge, clientIdNamespace, clientId);
        Set<Long> result = new HashSet<>(offlineIds.length);
        for (long id : offlineIds) {
            result.add(id);
        }
        return result;
    }

    /**
     * Gets an offline page associated with a provided bookmark ID.
     *
     * @param bookmarkId Id of the bookmark associated with an offline page.
     * @return An {@link OfflinePageItem} matching the bookmark Id or <code>null</code> if none
     * exist.
     */
    public OfflinePageItem getPageByBookmarkId(BookmarkId bookmarkId) {
        Set<Long> ids =
                getOfflineIdsForClientId(BOOKMARK_NAMESPACE, Long.toString(bookmarkId.getId()));
        if (ids.size() == 0) {
            return null;
        }
        Long offlineId = ids.iterator().next();
        // TODO(bburns): Handle multiple client ids better.
        return nativeGetPageByOfflineId(mNativeOfflinePageBridge, offlineId);
    }

    /**
     * Gets an offline page associated with a provided online URL.
     *
     * @param onlineURL URL of the page.
     * @return An {@link OfflinePageItem} matching the URL or <code>null</code> if none exist.
     */
    public OfflinePageItem getPageByOnlineURL(String onlineURL) {
        return nativeGetPageByOnlineURL(mNativeOfflinePageBridge, onlineURL);
    }

    /**
     * Gets an offline page associated with a provided offline URL.
     *
     * @param string URL pointing to the offline copy of the web page.
     * @return An {@link OfflinePageItem} matching the offline URL or <code>null</code> if not
     * found.
     */
    public OfflinePageItem getPageByOfflineUrl(String offlineUrl) {
        return nativeGetPageByOfflineUrl(mNativeOfflinePageBridge, offlineUrl);
    }

    /**
     * Saves the web page loaded into web contents offline.
     *
     * @param webContents Contents of the page to save.
     * @param bookmarkId Id of the bookmark related to the offline page.
     * @param callback Interface that contains a callback.
     * @see SavePageCallback
     */
    public void savePage(final WebContents webContents, final BookmarkId bookmarkId,
            final SavePageCallback callback) {
        assert mIsNativeOfflinePageModelLoaded;
        assert webContents != null;

        if (webContents.isDestroyed()) {
            callback.onSavePageDone(SavePageResult.CONTENT_UNAVAILABLE, null, INVALID_OFFLINE_ID);
            RecordHistogram.recordEnumeratedHistogram("OfflinePages.SavePageResult",
                    SavePageResult.CONTENT_UNAVAILABLE, SavePageResult.RESULT_COUNT);
            return;
        }

        SavePageCallback callbackWrapper = new SavePageCallback() {
            @Override
            public void onSavePageDone(int savePageResult, String url, long offlineId) {
                // TODO(fgorski): Eliminate call to getAllPages() here.
                // See http://crbug.com/566939
                if (savePageResult == SavePageResult.SUCCESS && isOfflinePageModelLoaded()) {
                    long totalPageSizeAfter = getTotalSize(getAllPages());
                    recordStorageHistograms(0, totalPageSizeAfter);
                }
                callback.onSavePageDone(savePageResult, url, offlineId);
            }
        };
        recordFreeSpaceHistograms(
                "OfflinePages.SavePage.FreeSpacePercentage", "OfflinePages.SavePage.FreeSpaceMB");
        String namespace = BOOKMARK_NAMESPACE;
        String clientId = Long.toString(bookmarkId.getId());

        nativeSavePage(mNativeOfflinePageBridge, callbackWrapper, webContents, namespace, clientId);
    }

    /**
     * Marks that an offline page related to a specified bookmark has been accessed.
     *
     * @param bookmarkId Bookmark ID for which the offline copy will be deleted.
     */
    private void markPageAccessed(BookmarkId bookmarkId) {
        assert mIsNativeOfflinePageModelLoaded;
        Set<Long> ids =
                getOfflineIdsForClientId(BOOKMARK_NAMESPACE, Long.toString(bookmarkId.getId()));
        if (ids.size() == 0) {
            return;
        }
        Long offlineId = ids.iterator().next();
        nativeMarkPageAccessed(mNativeOfflinePageBridge, offlineId);
    }

    /**
     * Deletes an offline page related to a specified bookmark.
     *
     * @param bookmarkId Bookmark ID for which the offline copy will be deleted.
     * @param callback Interface that contains a callback.
     * @see DeletePageCallback
     */
    public void deletePage(final BookmarkId bookmarkId, DeletePageCallback callback) {
        assert mIsNativeOfflinePageModelLoaded;

        recordFreeSpaceHistograms("OfflinePages.DeletePage.FreeSpacePercentage",
                "OfflinePages.DeletePage.FreeSpaceMB");

        DeletePageCallback callbackWrapper = wrapCallbackWithHistogramReporting(callback);
        Set<Long> ids =
                getOfflineIdsForClientId(BOOKMARK_NAMESPACE, Long.toString(bookmarkId.getId()));
        if (ids.size() == 0) {
            callback.onDeletePageDone(DeletePageResult.NOT_FOUND);
            return;
        }
        Long offlineId = ids.iterator().next();
        nativeDeletePage(mNativeOfflinePageBridge, callbackWrapper, offlineId);
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
        List<Long> idList = new ArrayList<>(bookmarkIds.size());
        for (int i = 0; i < bookmarkIds.size(); i++) {
            idList.addAll(getOfflineIdsForClientId(
                    BOOKMARK_NAMESPACE, Long.toString(bookmarkIds.get(i).getId())));
        }
        long[] ids = new long[idList.size()];
        for (int i = 0; i < idList.size(); i++) {
            ids[i] = idList.get(i);
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

    /**
     * Retrieves the url to launch a bookmark or saved page. If latter, also marks it as accessed
     * and reports the UMAs.
     *
     * @param onlineUrl Online url of a bookmark.
     * @return The launch URL.
     */
    public String getLaunchUrlFromOnlineUrl(String onlineUrl) {
        if (!isEnabled()) return onlineUrl;
        return getLaunchUrlAndMarkAccessed(getPageByOnlineURL(onlineUrl), onlineUrl);
    }

    /**
     * Retrieves the url to launch a bookmark or saved page. If latter, also marks it as
     * accessed and reports the UMAs.
     *
     * @param page Offline page to get the launch url for.
     * @param onlineUrl Online URL to launch if offline is not available.
     * @return The launch URL.
     */
    public String getLaunchUrlAndMarkAccessed(OfflinePageItem page, String onlineUrl) {
        if (page == null) return onlineUrl;

        boolean isOnline = OfflinePageUtils.isConnected();
        RecordHistogram.recordBooleanHistogram("OfflinePages.OnlineOnOpen", isOnline);

        // When there is a network connection, we visit original URL online.
        if (isOnline) return onlineUrl;

        // Mark that the offline page has been accessed, that will cause last access time and access
        // count being updated.
        nativeMarkPageAccessed(mNativeOfflinePageBridge, page.getBookmarkId().getId());

        // Returns the offline URL for offline access.
        return page.getOfflineUrl();
    }

    /**
     * Gets the offline URL of an offline page of that is saved for the online URL.
     * @param onlineUrl Online URL, which might have offline copy.
     * @return URL pointing to the offline copy or <code>null</code> if none exists.
     */
    public String getOfflineUrlForOnlineUrl(String onlineUrl) {
        assert mIsNativeOfflinePageModelLoaded;
        return nativeGetOfflineUrlForOnlineUrl(mNativeOfflinePageBridge, onlineUrl);
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
    private void offlinePageDeleted(long offlineId) {
        BookmarkId id = new BookmarkId(offlineId, BookmarkType.NORMAL);
        for (OfflinePageModelObserver observer : mObservers) {
            observer.offlinePageDeleted(id);
        }
    }

    @CalledByNative
    private static void createOfflinePageAndAddToList(List<OfflinePageItem> offlinePagesList,
            String url, long offlineId, String offlineUrl, long fileSize, long creationTime,
            int accessCount, long lastAccessTimeMs) {
        offlinePagesList.add(createOfflinePageItem(
                url, offlineId, offlineUrl, fileSize, creationTime, accessCount, lastAccessTimeMs));
    }

    @CalledByNative
    private static OfflinePageItem createOfflinePageItem(String url, long offlineId,
            String offlineUrl, long fileSize, long creationTime, int accessCount,
            long lastAccessTimeMs) {
        return new OfflinePageItem(
                url, offlineId, offlineUrl, fileSize, creationTime, accessCount, lastAccessTimeMs);
    }

    private static native int nativeGetFeatureMode();
    private static native boolean nativeCanSavePage(String url);

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeOfflinePageBridge);
    private native void nativeGetAllPages(
            long nativeOfflinePageBridge, List<OfflinePageItem> offlinePages);
    private native long[] nativeGetOfflineIdsForClientId(
            long nativeOfflinePageBridge, String clientNamespace, String clientId);
    private native OfflinePageItem nativeGetPageByOfflineId(
            long nativeOfflinePageBridge, long offlineId);
    private native OfflinePageItem nativeGetPageByOnlineURL(
            long nativeOfflinePageBridge, String onlineURL);
    private native OfflinePageItem nativeGetPageByOfflineUrl(
            long nativeOfflinePageBridge, String offlineUrl);
    private native void nativeSavePage(long nativeOfflinePageBridge, SavePageCallback callback,
            WebContents webContents, String clientNamespace, String clientId);
    private native void nativeMarkPageAccessed(long nativeOfflinePageBridge, long offlineId);
    private native void nativeDeletePage(
            long nativeOfflinePageBridge, DeletePageCallback callback, long offlineId);
    private native void nativeDeletePages(
            long nativeOfflinePageBridge, DeletePageCallback callback, long[] offlineIds);
    private native void nativeGetPagesToCleanUp(
            long nativeOfflinePageBridge, List<OfflinePageItem> offlinePages);
    private native void nativeCheckMetadataConsistency(long nativeOfflinePageBridge);
    private native String nativeGetOfflineUrlForOnlineUrl(
            long nativeOfflinePageBridge, String onlineUrl);
}
