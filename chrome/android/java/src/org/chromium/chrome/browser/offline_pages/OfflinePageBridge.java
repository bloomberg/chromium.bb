// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offline_pages;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
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
     * Interface with callbacks to public calls on OfflinePageBrdige.
     */
    public interface OfflinePageCallback {
        /**
         * Delivers result of saving a page.
         *
         * @param savePageResult Result of the saving. Uses
         *     {@see org.chromium.components.offline_pages.SavePageResult} enum.
         * @param url URL of the saved page.
         * @see OfflinePageBridge#savePage()
         */
        @CalledByNative("OfflinePageCallback")
        void onSavePageDone(int savePageResult, String url);
    }

    /**
     * Interface that provides listeners to be notified of changes to the offline page model.
     */
    public interface OfflinePageModelObserver {
        /**
         * Called when the native side of offline pages is loaded and now in usable state.
         */
        void offlinePageModelLoaded();
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
            sIsEnabled = nativeIsOfflinePagesEnabled();
        }
        return sIsEnabled;
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
     * Loads a list of all available offline pages. Results are returned through
     * provided callback interface.
     *
     * @param callback Interface that contains a callback.
     * @see OfflinePageCallback
     */
    @VisibleForTesting
    public List<OfflinePageItem> getAllPages() {
        assert mIsNativeOfflinePageModelLoaded;
        List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        nativeGetAllPages(mNativeOfflinePageBridge, result);
        return result;
    }

    /**
     * Saves the web page loaded into web contents offline.
     *
     * @param webContents Contents of the page to save.
     * @param bookmarkId Id of the bookmark related to the offline page.
     * @param callback Interface that contains a callback.
     * @see OfflinePageCallback
     */
    @VisibleForTesting
    public void savePage(
            WebContents webContents, BookmarkId bookmarkId, OfflinePageCallback callback) {
        assert mIsNativeOfflinePageModelLoaded;
        nativeSavePage(mNativeOfflinePageBridge, callback, webContents, bookmarkId.getId());
    }

    /**
     * Whether or not the underlying offline page model is loaded.
     */
    public boolean isOfflinePageModelLoaded() {
        return mIsNativeOfflinePageModelLoaded;
    }

    @CalledByNative
    private void offlinePageModelLoaded() {
        mIsNativeOfflinePageModelLoaded = true;
        for (OfflinePageModelObserver observer : mObservers) {
            observer.offlinePageModelLoaded();
        }
    }

    @CalledByNative
    private static void createOfflinePageAndAddToList(List<OfflinePageItem> offlinePagesList,
            String url, long bookmarkId, String offlineUrl, long fileSize) {
        offlinePagesList.add(new OfflinePageItem(url, bookmarkId, offlineUrl, fileSize));
    }

    private static native boolean nativeIsOfflinePagesEnabled();

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeOfflinePageBridge);
    private native void nativeGetAllPages(
            long nativeOfflinePageBridge, List<OfflinePageItem> offlinePages);
    private native void nativeSavePage(long nativeOfflinePageBridge, OfflinePageCallback callback,
            WebContents webContents, long bookmarkId);
}
