// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.downloads;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * Serves as an interface between Download Home UI and offline page related items that are to be
 * displayed in the downloads UI.
 */
@JNINamespace("offline_pages::android")
public class OfflinePageDownloadBridge {
    /**
     * Base observer class for notifications on changes to the offline page related download items.
     */
    public abstract static class Observer {
        /**
         * Indicates that the bridge is loaded and consumers can call GetXXX methods on it.
         * If the bridge is loaded at the time the observer is being added, the Loaded event will be
         * dispatched immediately.
         */
        public void onItemsLoaded() {}

        /**
         * Event fired when an new item was added.
         * @param item A newly added download item.
         */
        public void onItemAdded(OfflinePageDownloadItem item) {}

        /**
         * Event fired when an item was deleted
         * @param guid A GUID of the deleted download item.
         */
        public void onItemDeleted(String guid) {}

        /**
         * Event fired when an new item was updated.
         * @param item A newly updated download item.
         */
        public void onItemUpdated(OfflinePageDownloadItem item) {}
    }

    private static boolean sIsTesting = false;
    private final ObserverList<Observer> mObservers = new ObserverList<Observer>();
    private long mNativeOfflinePageDownloadBridge;
    private boolean mIsLoaded;

    public OfflinePageDownloadBridge(Profile profile) {
        mNativeOfflinePageDownloadBridge = sIsTesting ? 0L : nativeInit(profile);
    }

    /** Destroys the native portion of the bridge. */
    public void destroy() {
        if (mNativeOfflinePageDownloadBridge != 0) {
            nativeDestroy(mNativeOfflinePageDownloadBridge);
            mNativeOfflinePageDownloadBridge = 0;
            mIsLoaded = false;
        }
        mObservers.clear();
    }

    /**
     * Add an observer of offline download items changes.
     * @param observer The observer to be added.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
        if (mIsLoaded) {
            observer.onItemsLoaded();
        }
    }

    /**
     * Remove an observer of offline download items changes.
     * @param observer The observer to be removed.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /** @return all of the download items related to offline pages. */
    public List<OfflinePageDownloadItem> getAllItems() {
        List<OfflinePageDownloadItem> items = new ArrayList<>();
        nativeGetAllItems(mNativeOfflinePageDownloadBridge, items);
        return items;
    }

    /**
     * Gets a download item related to the provided GUID.
     * @param guid a GUID of the item to get.
     * @return download item related to the offline page identified by GUID.
     * */
    public OfflinePageDownloadItem getItem(String guid) {
        return nativeGetItemByGuid(mNativeOfflinePageDownloadBridge, guid);
    }

    /**
     * Method to ensure that the bridge is created for tests without calling the native portion of
     * initialization.
     * @param isTesting flag indicating whether the constructor will initialize native code.
     */
    static void setIsTesting(boolean isTesting) {
        sIsTesting = isTesting;
    }

    @CalledByNative
    void downloadItemsLoaded() {
        mIsLoaded = true;

        for (Observer observer : mObservers) {
            observer.onItemsLoaded();
        }
    }

    @CalledByNative
    void downloadItemAdded(OfflinePageDownloadItem item) {
        assert item != null;

        for (Observer observer : mObservers) {
            observer.onItemAdded(item);
        }
    }

    @CalledByNative
    void downloadItemDeleted(String guid) {
        for (Observer observer : mObservers) {
            observer.onItemDeleted(guid);
        }
    }

    @CalledByNative
    void downloadItemUpdated(OfflinePageDownloadItem item) {
        assert item != null;

        for (Observer observer : mObservers) {
            observer.onItemUpdated(item);
        }
    }

    @CalledByNative
    static void createDownloadItemAndAddToList(List<OfflinePageDownloadItem> list, String guid,
            String url, String targetPath, long startTimeMs, long totalBytes) {
        list.add(createDownloadItem(guid, url, targetPath, startTimeMs, totalBytes));
    }

    @CalledByNative
    static OfflinePageDownloadItem createDownloadItem(
            String guid, String url, String targetPath, long startTimeMs, long totalBytes) {
        return new OfflinePageDownloadItem(guid, url, targetPath, startTimeMs, totalBytes);
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeOfflinePageDownloadBridge);
    native void nativeGetAllItems(
            long nativeOfflinePageDownloadBridge, List<OfflinePageDownloadItem> items);
    native OfflinePageDownloadItem nativeGetItemByGuid(
            long nativeOfflinePageDownloadBridge, String guid);
}
