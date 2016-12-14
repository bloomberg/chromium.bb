// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/** The JNI bridge for Android to fetch and manipulate browsing history. */
public class BrowsingHistoryBridge {

    /**
     * Observer to be notified of browsing history events.
     */
    public interface BrowsingHistoryObserver {
        /**
         * Called after {@link BrowsingHistoryBridge#queryHistory(String, long)} is complete.
         * @param items The items that matched the #queryHistory() parameters.
         * @param hasMorePotentialMatches Whether there are more items that match the query text.
         *                                This will be false once the entire local history database
         *                                has been searched.
         */
        public void onQueryHistoryComplete(List<HistoryItem> items,
                boolean hasMorePotentialMatches);

        /**
         * Called when history has been deleted through something other than a call to
         * BrowsingHistoryBridge#removeItems(). For example, if two instances of the history page
         * are open and the user removes items in one instance, the other instance will be notified
         * via this method.
         */
        public void onHistoryDeleted();

        /**
         * Called after querying history to indicate whether other forms of browsing history were
         * found.
         * @param hasOtherForms Whether other forms of browsing history were found.
         * @param hasSyncedResults Whether synced results were found.
         */
        public void hasOtherFormsOfBrowsingData(boolean hasOtherForms, boolean hasSyncedResults);
    }

    private final BrowsingHistoryObserver mObserver;

    private long mNativeHistoryBridge;
    private boolean mRemovingItems;
    private boolean mHasPendingRemoveRequest;

    public BrowsingHistoryBridge(BrowsingHistoryObserver observer) {
        mNativeHistoryBridge = nativeInit(Profile.getLastUsedProfile());
        mObserver = observer;
    }

    public void destroy() {
        if (mNativeHistoryBridge != 0) {
            nativeDestroy(mNativeHistoryBridge);
            mNativeHistoryBridge = 0;
        }
    }

    /**
     * Query browsing history. Only one query may be in-flight at any time. See
     * BrowsingHistoryService::QueryHistory.
     * @param query The query search text. May be empty.
     * @param endQueryTime The end of the time range to search. A value of 0 indicates that there
     *                     is no limit on the end time. See the native QueryOptions.
     */
    public void queryHistory(String query, long endQueryTime) {
        nativeQueryHistory(mNativeHistoryBridge, new ArrayList<HistoryItem>(), query, endQueryTime);
    }

    /**
     * Adds the HistoryItem to the list of items being removed. The removal will not be committed
     * until {@link #removeItems()} is called.
     * @param item The item to mark for removal.
     */
    public void markItemForRemoval(HistoryItem item) {
        nativeMarkItemForRemoval(mNativeHistoryBridge, item.getUrl(), item.getTimestamps());
    }

    /**
     * Removes all items that have been marked for removal through #markItemForRemoval().
     */
    public void removeItems() {
        // Only one remove request may be in-flight at any given time. If items are currently being
        // removed, queue the new request and return early.
        if (mRemovingItems) {
            mHasPendingRemoveRequest = true;
            return;
        }
        mRemovingItems = true;
        mHasPendingRemoveRequest = false;
        nativeRemoveItems(mNativeHistoryBridge);
    }

    @CalledByNative
    public static void createHistoryItemAndAddToList(
            List<HistoryItem> items, String url, String domain, String title, long[] timestamps) {
        items.add(new HistoryItem(url, domain, title, timestamps));
    }

    @CalledByNative
    public void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches) {
        mObserver.onQueryHistoryComplete(items, hasMorePotentialMatches);
    }

    @CalledByNative
    public void onRemoveComplete() {
        mRemovingItems = false;
        if (mHasPendingRemoveRequest) removeItems();
    }

    @CalledByNative
    public void onRemoveFailed() {
        mRemovingItems = false;
        if (mHasPendingRemoveRequest) removeItems();
        // TODO(twellington): handle remove failures.
    }

    @CalledByNative
    public void onHistoryDeleted() {
        mObserver.onHistoryDeleted();
    }

    @CalledByNative
    public void hasOtherFormsOfBrowsingData(boolean hasOtherForms, boolean hasSyncedResults) {
        mObserver.hasOtherFormsOfBrowsingData(hasOtherForms, hasSyncedResults);
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeBrowsingHistoryBridge);
    private native void nativeQueryHistory(long nativeBrowsingHistoryBridge,
            List<HistoryItem> historyItems, String query, long queryEndTime);
    private native void nativeMarkItemForRemoval(long nativeBrowsingHistoryBridge,
            String url, long[] timestamps);
    private native void nativeRemoveItems(long nativeBrowsingHistoryBridge);
}
