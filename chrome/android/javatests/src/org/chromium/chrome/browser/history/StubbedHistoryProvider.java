// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.List;

/**
 * Stubs out the backends used by the native Android browsing history manager.
 */
public class StubbedHistoryProvider implements HistoryProvider {
    public final CallbackHelper markItemForRemovalCallback = new CallbackHelper();
    public final CallbackHelper removeItemsCallback = new CallbackHelper();

    private BrowsingHistoryObserver mObserver;
    private List<HistoryItem> mItems = new ArrayList<>();
    private List<HistoryItem> mRemovedItems = new ArrayList<>();

    @Override
    public void setObserver(BrowsingHistoryObserver observer) {
        mObserver = observer;
    }

    @Override
    public void queryHistory(String query, long endQueryTime) {
        mObserver.onQueryHistoryComplete(mItems, false);
    }

    @Override
    public void markItemForRemoval(HistoryItem item) {
        mRemovedItems.add(item);
        markItemForRemovalCallback.notifyCalled();
    }

    @Override
    public void removeItems() {
        for (HistoryItem item : mRemovedItems) {
            mItems.remove(item);
        }
        mRemovedItems.clear();
        removeItemsCallback.notifyCalled();
    }

    @Override
    public void destroy() {}

    public void addItem(HistoryItem item) {
        mItems.add(item);
    }

    public static HistoryItem createHistoryItem(int which, long[] timestamps) {
        if (which == 0) {
            return new HistoryItem("http://google.com/", "www.google.com", "Google", timestamps);
        } else if (which == 1) {
            return new HistoryItem("http://foo.com/", "www.foo.com", "Foo", timestamps);
        } else {
            return null;
        }
    }

}
