// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offline_pages;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Access gate to C++ side offline pages functionalities.
 */
@JNINamespace("offline_pages::android")
public final class OfflinePageBridge {

    private long mNativeOfflinePageBridge;

    /**
     * Interface with callbacks to public calls on OfflinePageBrdige.
     */
    public interface OfflinePageCallback {
        /**
         * Delivers result of loading all pages.
         *
         * @param loadResult Result of the loading the page. Uses
         *     {@see org.chromium.components.offline_pages.LoadResult} enum.
         * @param offlinePages A list of loaded offline pages.
         * @see OfflinePageBridge#loadAllPages()
         */
        @CalledByNative("OfflinePageCallback")
        void onLoadAllPagesDone(int loadResult, List<OfflinePageItem> offlinePages);

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
     * Creates offline pages bridge for a given profile.
     */
    @VisibleForTesting
    public OfflinePageBridge(Profile profile) {
        mNativeOfflinePageBridge = nativeInit(profile);
    }

    /**
     * Destroys native offline pages bridge. It should be called during
     * destruction to ensure proper cleanup.
     */
    public void destroy() {
        assert mNativeOfflinePageBridge != 0;
        nativeDestroy(mNativeOfflinePageBridge);
        mNativeOfflinePageBridge = 0;
    }

    /**
     * Loads a list of all available offline pages. Results are returned through
     * provided callback interface.
     *
     * @param callback Interface that contains a callback.
     * @see OfflinePageCallback
     */
    @VisibleForTesting
    public void loadAllPages(OfflinePageCallback callback) {
        nativeLoadAllPages(mNativeOfflinePageBridge, callback, new ArrayList<OfflinePageItem>());
    }

    /**
     * Saves the web page loaded into web contents offline.
     *
     * @param webContents Contents of the page to save.
     * @param callback Interface that contains a callback.
     * @see OfflinePageCallback
     */
    @VisibleForTesting
    public void savePage(WebContents webContents, OfflinePageCallback callback) {
        nativeSavePage(mNativeOfflinePageBridge, callback, webContents);
    }

    @CalledByNative
    private static void createOfflinePageAndAddToList(List<OfflinePageItem> offlinePagesList,
            String url, String title, String offlineUrl, long fileSize) {
        offlinePagesList.add(new OfflinePageItem(url, title, offlineUrl, fileSize));
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeOfflinePageBridge);
    private native void nativeLoadAllPages(long nativeOfflinePageBridge,
            OfflinePageCallback callback, List<OfflinePageItem> offlinePages);
    private native void nativeSavePage(
            long nativeOfflinePageBridge, OfflinePageCallback callback, WebContents webContents);
}
