// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.os.AsyncTask;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.offlinepages.DeletePageResult;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Access gate to C++ side offline pages functionalities.
 */
@JNINamespace("offline_pages::android")
public class OfflinePageBridge {
    public static final String BOOKMARK_NAMESPACE = "bookmark";
    public static final long INVALID_OFFLINE_ID = 0;

    /**
     * Retrieves the OfflinePageBridge for the given profile, creating it the first time
     * getForProfile is called for a given profile.  Must be called on the UI thread.
     *
     * @param profile The profile associated with the OfflinePageBridge to get.
     */
    public static OfflinePageBridge getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();

        return nativeGetOfflinePageBridgeForProfile(profile);
    }

    private long mNativeOfflinePageBridge;
    private boolean mIsNativeOfflinePageModelLoaded;
    private final ObserverList<OfflinePageModelObserver> mObservers =
            new ObserverList<OfflinePageModelObserver>();

    /** Whether offline pages feature is enabled or not. */
    private static Boolean sOfflinePagesEnabled;

    /** Whether an offline sub-feature is enabled or not. */
    private static Boolean sOfflineBookmarksEnabled;
    private static Boolean sBackgroundLoadingEnabled;

    /**
     * Callback used when saving an offline page.
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
     * Callback used when deleting an offline page.
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
     * Callback that delivers information about multiple offline page entries.
     *
     * The returned List will be empty (but non-null) if no items are found.
     */
    public interface MultipleOfflinePageItemCallback {
        @CalledByNative("MultipleOfflinePageItemCallback")
        void onResult(List<OfflinePageItem> items);
    }

    /**
     * Callback used when determining whether we have any offline pages.
     */
    public interface HasPagesCallback {
        @CalledByNative("HasPagesCallback")
        void onResult(boolean result);
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
         * @param offlineId The offline ID of the deleted offline page.
         * @param clientId The client supplied ID of the deleted offline page.
         */
        public void offlinePageDeleted(long offlineId, ClientId clientId) {}
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
     * @param reportingAfterDelete Indicates that reporting has been requested after deleting an
     *   offline copy.
     */
    private void recordStorageHistograms(final boolean reportingAfterDelete) {
        new AsyncTask<Void, Void, long[]>() {
            @Override
            protected long[] doInBackground(Void... params) {
                // Getting the storage numbers violates strict mode when done on UI thread.
                return new long[] { OfflinePageUtils.getTotalSpaceInBytes(),
                        OfflinePageUtils.getFreeSpaceInBytes() };
            }

            @Override
            protected void onPostExecute(long[] result) {
                if (result == null || result.length != 2) return;
                nativeRecordStorageHistograms(
                        mNativeOfflinePageBridge, result[0], result[1], reportingAfterDelete);
            }
        }.execute();
    }

    /**
     * Creates an offline page bridge for a given profile.
     * Accessible by the package for testability.
     */
    @VisibleForTesting
    OfflinePageBridge(long nativeOfflinePageBridge) {
        mNativeOfflinePageBridge = nativeOfflinePageBridge;
    }

    /**
     * Called by the native OfflinePageBridge so that it can cache the new Java OfflinePageBridge.
     */
    @CalledByNative
    private static OfflinePageBridge create(long nativeOfflinePageBridge) {
        return new OfflinePageBridge(nativeOfflinePageBridge);
    }

    /**
     * @return True if offline pages feature is enabled.
     */
    public static boolean isOfflinePagesEnabled() {
        ThreadUtils.assertOnUiThread();
        if (sOfflinePagesEnabled == null) {
            sOfflinePagesEnabled = nativeIsOfflinePagesEnabled();
        }
        return sOfflinePagesEnabled;
    }

    /**
     * @return True if saving bookmarked pages for offline viewing is enabled.
     */
    public static boolean isOfflineBookmarksEnabled() {
        ThreadUtils.assertOnUiThread();
        if (sOfflineBookmarksEnabled == null) {
            sOfflineBookmarksEnabled = nativeIsOfflineBookmarksEnabled();
        }
        return sOfflineBookmarksEnabled;
    }

    /**
     * @return True if saving offline pages in the background is enabled.
     */
    @VisibleForTesting
    public static boolean isBackgroundLoadingEnabled() {
        ThreadUtils.assertOnUiThread();
        if (sBackgroundLoadingEnabled == null) {
            sBackgroundLoadingEnabled = nativeIsBackgroundLoadingEnabled();
        }
        return sBackgroundLoadingEnabled;
    }

    /**
     * @return True if an offline copy of the given URL can be saved.
     */
    public static boolean canSavePage(String url) {
        return nativeCanSavePage(url);
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
     * Gets all available offline pages, returning results via the provided callback.
     *
     * @param callback The callback to run when the operation completes.
     */
    @VisibleForTesting
    public void getAllPages(final MultipleOfflinePageItemCallback callback) {
        List<OfflinePageItem> result = new ArrayList<>();
        nativeGetAllPages(mNativeOfflinePageBridge, result, callback);
    }

    /**
     * Returns via callback whether we have any offline pages at all.
     *
     * TODO(dewittj): Remove @VisibleForTesting when this method is called in mainline Chrome.
     */
    @VisibleForTesting
    public void hasPages(final String namespace, final HasPagesCallback callback) {
        nativeHasPages(mNativeOfflinePageBridge, namespace, callback);
    }

    /** @return A list of all offline ids that match a particular (namespace, client_id) pair. */
    Set<Long> getOfflineIdsForClientId(ClientId clientId) {
        assert mIsNativeOfflinePageModelLoaded;
        long[] offlineIds = nativeGetOfflineIdsForClientId(
                mNativeOfflinePageBridge, clientId.getNamespace(), clientId.getId());
        Set<Long> result = new HashSet<>(offlineIds.length);
        for (long id : offlineIds) {
            result.add(id);
        }
        return result;
    }

    /**
     * Gets the offline pages associated with a provided client ID.
     *
     * @param clientId Client's ID associated with an offline page.
     * @return A {@link OfflinePageItem} matching the bookmark Id or <code>null</code> if none
     * exist.
     */
    @VisibleForTesting
    public void getPagesByClientId(
            final ClientId clientId, final MultipleOfflinePageItemCallback callback) {
        runWhenLoaded(new Runnable() {
            @Override
            public void run() {
                callback.onResult(getPagesByClientIdInternal(clientId));
            }
        });
    }

    private List<OfflinePageItem> getPagesByClientIdInternal(ClientId clientId) {
        Set<Long> ids = getOfflineIdsForClientId(clientId);
        List<OfflinePageItem> result = new ArrayList<>();
        for (long offlineId : ids) {
            // TODO(dewittj): Restructure the native API to avoid this loop with a native call.
            OfflinePageItem item = nativeGetPageByOfflineId(mNativeOfflinePageBridge, offlineId);
            if (item != null) {
                result.add(item);
            }
        }
        return result;
    }

    /**
     * Gets the offline pages associated with a provided online URL.  The callback is called when
     * the results are available.
     *
     * @param onlineURL URL of the page.
     * @param callback Called with the results.
     */
    @VisibleForTesting
    public void getPagesByOnlineUrl(
            final String onlineUrl, final MultipleOfflinePageItemCallback callback) {
        runWhenLoaded(new Runnable() {
            @Override
            public void run() {
                List<OfflinePageItem> result = new ArrayList<>();

                // TODO(http://crbug.com/589526) This native API returns only one item, but in the
                // future will return a list.
                OfflinePageItem item =
                        nativeGetBestPageForOnlineURL(mNativeOfflinePageBridge, onlineUrl);
                if (item != null) {
                    result.add(item);
                }

                callback.onResult(result);
            }
        });
    }

    /**
     * Gets the offline pages associated with the provided offline URL.
     *
     * @param string URL pointing to the offline copy of the web page.
     * @return An {@link OfflinePageItem} matching the offline URL or
     * <code>null</code> if not found.
     * found.
     */
    @VisibleForTesting
    public void getPagesByOfflineUrl(
            final String offlineUrl, final MultipleOfflinePageItemCallback callback) {
        runWhenLoaded(new Runnable() {
            @Override
            public void run() {
                List<OfflinePageItem> result = new ArrayList<>();

                // TODO(http://crbug.com/589526) This native API returns only one item, but in the
                // future will return a list.
                OfflinePageItem item =
                        nativeGetPageByOfflineUrl(mNativeOfflinePageBridge, offlineUrl);
                if (item != null) {
                    result.add(item);
                }

                callback.onResult(result);
            }
        });
    }

    /**
     * Gets an offline page associated with a provided offline URL.
     * This method is deprecated. Use OfflinePageBridge#getPagesByOnlineUrl.
     *
     * @param string URL pointing to the offline copy of the web page.
     * @return An {@link OfflinePageItem} matching the offline URL or
     * <code>null</code> if not found.
     */
    public OfflinePageItem getPageByOfflineUrl(String offlineUrl) {
        return nativeGetPageByOfflineUrl(mNativeOfflinePageBridge, offlineUrl);
    }

    /**
     * Saves the web page loaded into web contents offline.
     *
     * @param webContents Contents of the page to save.
     * @param ClientId of the bookmark related to the offline page.
     * @param callback Interface that contains a callback. This may be called synchronously, e.g.
     * if the web contents is already destroyed.
     * @see SavePageCallback
     */
    public void savePage(final WebContents webContents, final ClientId clientId,
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
                if (savePageResult == SavePageResult.SUCCESS && isOfflinePageModelLoaded()) {
                    recordStorageHistograms(false /* reporting after delete */);
                }
                callback.onSavePageDone(savePageResult, url, offlineId);
            }
        };
        recordFreeSpaceHistograms(
                "OfflinePages.SavePage.FreeSpacePercentage", "OfflinePages.SavePage.FreeSpaceMB");

        nativeSavePage(mNativeOfflinePageBridge, callbackWrapper, webContents,
                clientId.getNamespace(), clientId.getId());
    }

    /**
     * Save the given URL as an offline page when the network becomes available.
     *
     * @param url The given URL to save for later
     * @param clientId The client ID for the offline page to be saved later.
     */
    public void savePageLater(final String url, final ClientId clientId) {
        nativeSavePageLater(
                mNativeOfflinePageBridge, url, clientId.getNamespace(), clientId.getId());
    }

    /**
     * Deletes an offline page related to a specified bookmark.
     *
     * @param clientId Client ID for which the offline copy will be deleted.
     * @param callback Interface that contains a callback.
     * @see DeletePageCallback
     */
    @VisibleForTesting
    public void deletePage(final ClientId clientId, DeletePageCallback callback) {
        assert mIsNativeOfflinePageModelLoaded;
        ArrayList<ClientId> ids = new ArrayList<ClientId>();
        ids.add(clientId);

        deletePagesByClientId(ids, callback);
    }

    /**
     * Deletes offline pages based on the list of provided client IDs. Calls the callback
     * when operation is complete. Requires that the model is already loaded.
     *
     * @param clientIds A list of Client IDs for which the offline pages will be deleted.
     * @param callback A callback that will be called once operation is completed.
     */
    public void deletePagesByClientId(List<ClientId> clientIds, DeletePageCallback callback) {
        assert mIsNativeOfflinePageModelLoaded;
        List<Long> idList = new ArrayList<>(clientIds.size());
        for (ClientId clientId : clientIds) {
            idList.addAll(getOfflineIdsForClientId(clientId));
        }
        deletePages(idList, callback);
    }

    void deletePages(List<Long> offlineIds, DeletePageCallback callback) {
        long[] ids = new long[offlineIds.size()];
        for (int i = 0; i < offlineIds.size(); i++) {
            ids[i] = offlineIds.get(i);
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
     * Starts a check of offline page metadata, e.g. are all offline copies present.
     */
    public void checkOfflinePageMetadata() {
        nativeCheckMetadataConsistency(mNativeOfflinePageBridge);
    }

    private static class CheckPagesExistOfflineCallbackInternal {
        private Callback<Set<String>> mCallback;

        CheckPagesExistOfflineCallbackInternal(Callback<Set<String>> callback) {
            mCallback = callback;
        }

        @CalledByNative("CheckPagesExistOfflineCallbackInternal")
        public void onResult(String[] results) {
            Set<String> resultSet = new HashSet<>();
            Collections.addAll(resultSet, results);
            mCallback.onResult(resultSet);
        }
    }

    /**
     * Returns via callback any urls in <code>urls</code> for which there exist offline pages.
     *
     * TODO(http://crbug.com/598006): Add metrics for preventing UI jank.
     */
    public void checkPagesExistOffline(Set<String> urls, Callback<Set<String>> callback) {
        String[] urlArray = urls.toArray(new String[urls.size()]);

        CheckPagesExistOfflineCallbackInternal callbackInternal =
                new CheckPagesExistOfflineCallbackInternal(callback);
        nativeCheckPagesExistOffline(mNativeOfflinePageBridge, urlArray, callbackInternal);
    }

    private DeletePageCallback wrapCallbackWithHistogramReporting(
            final DeletePageCallback callback) {
        return new DeletePageCallback() {
            @Override
            public void onDeletePageDone(int deletePageResult) {
                if (deletePageResult == DeletePageResult.SUCCESS && isOfflinePageModelLoaded()) {
                    recordStorageHistograms(true /* reporting after delete */);
                }
                callback.onDeletePageDone(deletePageResult);
            }
        };
    }

    @VisibleForTesting
    ClientId getClientIdForOfflineId(long offlineId) {
        OfflinePageItem item = nativeGetPageByOfflineId(mNativeOfflinePageBridge, offlineId);
        if (item != null) {
            return item.getClientId();
        }
        return null;
    }

    private void runWhenLoaded(final Runnable runnable) {
        if (isOfflinePageModelLoaded()) {
            ThreadUtils.postOnUiThread(runnable);
            return;
        }

        addObserver(new OfflinePageModelObserver() {
            @Override
            public void offlinePageModelLoaded() {
                removeObserver(this);
                runnable.run();
            }
        });
    }

    @CalledByNative
    void offlinePageModelLoaded() {
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

    /**
     * Removes references to the native OfflinePageBridge when it is being destroyed.
     */
    @CalledByNative
    private void offlinePageBridgeDestroyed() {
        ThreadUtils.assertOnUiThread();
        assert mNativeOfflinePageBridge != 0;

        mIsNativeOfflinePageModelLoaded = false;
        mNativeOfflinePageBridge = 0;

        // TODO(dewittj): Add a model destroyed method to the observer interface.
        mObservers.clear();
    }

    @CalledByNative
    void offlinePageDeleted(long offlineId, ClientId clientId) {
        for (OfflinePageModelObserver observer : mObservers) {
            observer.offlinePageDeleted(offlineId, clientId);
        }
    }

    @CalledByNative
    private static void createOfflinePageAndAddToList(List<OfflinePageItem> offlinePagesList,
            String url, long offlineId, String clientNamespace, String clientId, String offlineUrl,
            long fileSize, long creationTime, int accessCount, long lastAccessTimeMs) {
        offlinePagesList.add(createOfflinePageItem(url, offlineId, clientNamespace, clientId,
                offlineUrl, fileSize, creationTime, accessCount, lastAccessTimeMs));
    }

    @CalledByNative
    private static OfflinePageItem createOfflinePageItem(String url, long offlineId,
            String clientNamespace, String clientId, String offlineUrl, long fileSize,
            long creationTime, int accessCount, long lastAccessTimeMs) {
        return new OfflinePageItem(url, offlineId, clientNamespace, clientId, offlineUrl, fileSize,
                creationTime, accessCount, lastAccessTimeMs);
    }

    @CalledByNative
    private static ClientId createClientId(String clientNamespace, String id) {
        return new ClientId(clientNamespace, id);
    }

    private static native boolean nativeIsOfflinePagesEnabled();
    private static native boolean nativeIsOfflineBookmarksEnabled();
    private static native boolean nativeIsBackgroundLoadingEnabled();
    private static native boolean nativeCanSavePage(String url);
    private static native OfflinePageBridge nativeGetOfflinePageBridgeForProfile(Profile profile);

    @VisibleForTesting
    native void nativeGetAllPages(long nativeOfflinePageBridge, List<OfflinePageItem> offlinePages,
            final MultipleOfflinePageItemCallback callback);
    private native void nativeCheckPagesExistOffline(long nativeOfflinePageBridge, Object[] urls,
            CheckPagesExistOfflineCallbackInternal callback);
    native void nativeHasPages(
            long nativeOfflinePageBridge, String nameSpace, final HasPagesCallback callback);

    @VisibleForTesting
    native long[] nativeGetOfflineIdsForClientId(
            long nativeOfflinePageBridge, String clientNamespace, String clientId);

    @VisibleForTesting
    native OfflinePageItem nativeGetPageByOfflineId(long nativeOfflinePageBridge, long offlineId);
    private native OfflinePageItem nativeGetBestPageForOnlineURL(
            long nativeOfflinePageBridge, String onlineURL);
    private native OfflinePageItem nativeGetPageByOfflineUrl(
            long nativeOfflinePageBridge, String offlineUrl);
    private native void nativeSavePage(long nativeOfflinePageBridge, SavePageCallback callback,
            WebContents webContents, String clientNamespace, String clientId);
    private native void nativeSavePageLater(
            long nativeOfflinePageBridge, String url, String clientNamespace, String clientId);
    private native void nativeDeletePages(
            long nativeOfflinePageBridge, DeletePageCallback callback, long[] offlineIds);
    private native void nativeCheckMetadataConsistency(long nativeOfflinePageBridge);
    private native void nativeRecordStorageHistograms(long nativeOfflinePageBridge,
            long totalSpaceInBytes, long freeSpaceInBytes, boolean reportingAfterDelete);
}
