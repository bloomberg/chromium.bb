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
    private long mBookmarkId;
    private boolean mWasHidden;
    private OfflinePageConnectivityListener mListener;

    private static final Map<Integer, OfflinePageConnectivityListener> sConnectivityListeners =
            new TreeMap<Integer, OfflinePageConnectivityListener>();

    /**
     * Builds a new OfflinePageTabObserver.
     * @param activity The ChromeActivity of this instance of the browser.
     * @param connected True if the phone is connected when the observer is created.
     * @param bookmarkId Id of the bookmark (offline page) that is associated with this observer.
     */
    public OfflinePageTabObserver(
            ChromeActivity activity, Tab tab, boolean connected, long bookmarkId) {
        mActivity = activity;
        mConnected = connected;
        mBookmarkId = bookmarkId;
        // Remember if the tab was hidden when we started, so we can show the snackbar when
        // the tab becomes visible.
        mWasHidden = tab.isHidden();

        mListener = new OfflinePageConnectivityListener(activity, tab);
        sConnectivityListeners.put(tab.getId(), mListener);
        Log.d(TAG, "OfflinePageTabObserver built");
    }

    @Override
    public void onShown(Tab visibleTab) {
        if (mWasHidden) {
            // TODO(petewil): Connectivity listener should channel NCN.isOnline().
            if (mConnected) {
                OfflinePageUtils.showReloadSnackbar(mActivity, visibleTab.getId(), mBookmarkId);
            } else {
                OfflinePageUtils.showEditSnackbar(mActivity, visibleTab.getId(), mBookmarkId);
            }

            mWasHidden = false;
            Log.d(TAG, "onShown, showing 'delayed' snackbar");
        }
    }

    @Override
    public void onHidden(Tab hiddenTab) {
        mWasHidden = true;
        // TODO(petewil): In case any snackbars are showing, dismiss them before we switch tabs.
    }

    @Override
    public void onPageLoadStarted(Tab tab, String newUrl) {
        OfflinePageUtils.preferOnlineVersion(mActivity, tab, newUrl);
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
}
