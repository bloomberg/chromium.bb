// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/** The JNI bridge for Android to fetch and manipulate browsing history. */
public class BrowsingHistoryBridge {

    private long mNativeHistoryBridge;
    private Callback<List<HistoryItem>> mCallback;

    public BrowsingHistoryBridge() {
        mNativeHistoryBridge = nativeInit(Profile.getLastUsedProfile());
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
     * @param callback The callback that will receive query results.
     * @param query The query search text. May be empty.
     * @param endQueryTime The end of the time range to search. A value of 0 indicates that there
     *                     is no limit on the end time. See the native QueryOptions.
     */
    public void queryHistory(Callback<List<HistoryItem>> callback, String query,
            long endQueryTime) {
        mCallback = callback;
        nativeQueryHistory(mNativeHistoryBridge, new ArrayList<HistoryItem>(), query, endQueryTime);
    }

    @CalledByNative
    public static void createHistoryItemAndAddToList(
            List<HistoryItem> items, String url, String domain, String title, long timestamp) {
        items.add(new HistoryItem(url, domain, title, timestamp));
    }

    @CalledByNative
    public void onQueryHistoryComplete(List<HistoryItem> items) {
        mCallback.onResult(items);
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeBrowsingHistoryBridge);
    private native void nativeQueryHistory(long nativeBrowsingHistoryBridge,
            List<HistoryItem> historyItems, String query, long queryEndTime);
}
