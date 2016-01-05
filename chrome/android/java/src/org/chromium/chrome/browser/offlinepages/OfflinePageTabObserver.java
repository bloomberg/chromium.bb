// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;

import java.util.Map;
import java.util.TreeMap;

/**
 * A class that observes events for a tab which has an associated offline page.  This will be
 * created when needed (for instance, we want to show a snackbar when the tab is shown or we want
 * to show a snackbar if the device connects).  It will be removed when the user navigates away
 * from the offline tab.
 */
public class OfflinePageTabObserver extends EmptyTabObserver {
    private static final String TAG = "OfflinePageTO";
    private ChromeActivity mActivity;
    private boolean mConnected;
    private boolean mShouldSave;
    private long mBookmarkId;
    private boolean mWasHidden = false;
    private OfflinePageConnectivityListener mListener = null;

    private static final Map<Integer, OfflinePageConnectivityListener> sConnectivityListeners =
            new TreeMap<Integer, OfflinePageConnectivityListener>();

    /**
     * Builds a new OfflinePageTabObserver.
     * @param activity The ChromeActivity of this instance of the browser.
     * @param connected True if the phone is connected when the observer is created.
     * @param shouldSave True if we should show a snackbar offering to save the page offline.
     * @param bookmarkId Id of the bookmark (offline page) that is associated with this observer.
     */
    public OfflinePageTabObserver(
            ChromeActivity activity, boolean connected, boolean shouldSave, long bookmarkId) {
        mActivity = activity;
        mConnected = connected;
        mShouldSave = shouldSave;
        mBookmarkId = bookmarkId;
        Log.d(TAG, "OfflinePageTabObserver built");
    }

    @Override
    public void onShown(Tab visibleTab) {
        if (mWasHidden) {
            OfflinePageUtils.showOfflineSnackbar(
                    mActivity, visibleTab.getId(), mShouldSave, mConnected, mBookmarkId);
            mWasHidden = false;
            Log.d(TAG, "onShown, showing 'delayed' snackbar");
        }
    }

    @Override
    public void onDestroyed(Tab destroyedTab) {
        // Unregister this tab for OS connectivity notifications.
        if (mListener != null) {
            mListener.disable();
            destroyedTab.removeObserver(this);
            sConnectivityListeners.remove(destroyedTab.getId());
            Log.d(TAG, "onDestroyed");
        }
    }

    @Override
    public void onUrlUpdated(Tab reloadingTab) {
        // Unregister this tab for OS connectivity notifications.
        if (mListener != null) {
            mListener.disable();
            reloadingTab.removeObserver(this);
            sConnectivityListeners.remove(reloadingTab.getId());
            Log.d(TAG, "onUrlUpdated");
        }
    }

    /**
     * Attaches a connectivity listener if needed by this observer.
     * @param tabId The index of the tab that this listener is listening to.
     * @param listener The listener itself.
     */
    public void setConnectivityListener(int tabId, OfflinePageConnectivityListener listener) {
        mListener = listener;
        sConnectivityListeners.put(tabId, mListener);
    }

    /**
     * Remembers if the page started hidden, so we know to show it if we get 'onShown'
     * @param wasHidden True if the tab was hidden when the listener was set up.
     */
    public void setWasHidden(boolean wasHidden) {
        mWasHidden = wasHidden;
    }
}
