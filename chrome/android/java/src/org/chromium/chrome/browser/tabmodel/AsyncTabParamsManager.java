// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.util.SparseArray;

/**
 * Data that will be used later when a tab is opened via an intent. Often only the necessary
 * subset of the data will be set. All data is removed once the tab finishes initializing.
 */
public class AsyncTabParamsManager {
    /** A map of tab IDs to AsyncTabParams consumed by Activities started asynchronously. */
    private static final SparseArray<AsyncTabParams> sAsyncTabParams = new SparseArray<>();

    /**
     * Stores AsyncTabParams used when the tab with the given ID is launched via intent.
     * @param tabId The ID of the tab that will be launched via intent.
     * @param params The AsyncTabParams to use when creating the Tab.
     */
    public static void add(int tabId, AsyncTabParams params) {
        sAsyncTabParams.put(tabId, params);
    }

    /**
     * @return Whether there are any saved {@link AsyncTabParams} with the given tab id.
     */
    public static boolean hasParamsForTabId(int tabId) {
        return sAsyncTabParams.get(tabId) != null;
    }

    /**
     * @return Retrieves and removes AsyncTabCreationParams for a particular tab id.
     */
    public static AsyncTabParams remove(int tabId) {
        AsyncTabParams data = sAsyncTabParams.get(tabId);
        sAsyncTabParams.remove(tabId);
        return data;
    }

    private AsyncTabParamsManager() {
    }
}