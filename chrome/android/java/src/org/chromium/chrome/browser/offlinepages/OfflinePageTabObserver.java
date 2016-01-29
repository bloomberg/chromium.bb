// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
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
    private SnackbarController mSnackbarController;
    private boolean mWasHidden = false;
    private OfflinePageConnectivityListener mListener = null;

    static final Map<Integer, OfflinePageTabObserver> sTabObservers =
            new TreeMap<Integer, OfflinePageTabObserver>();

    /**
     * Create and attach a tab observer if we don't already have one, otherwise update it.
     * @param activity The ChromeActivity we are associated with.
     * @param tab The tab we are adding an observer for.
     * @param conneted True if we were connected when this call was made.
     * @param bookmarkId The ID of the bookmark we are adding an observer for.
     */
    public static void addObserverForTab(ChromeActivity activity, Tab tab, boolean connected,
            long bookmarkId, SnackbarController snackbarController) {
        // See if we already have an observer for this tab.
        int tabId = tab.getId();
        OfflinePageTabObserver observer = sTabObservers.get(tabId);
        Log.d(TAG, "addObserver called, tabId " + tabId);

        if (observer == null) {
            // If we don't have an observer, build one and attach it to the tab.
            observer = new OfflinePageTabObserver(
                    activity, tab, connected, bookmarkId, snackbarController);
            sTabObservers.put(tabId, observer);
            tab.addObserver(observer);
            Log.d(TAG, "Added tab observer connected " + connected + ", bookmarkId " + bookmarkId
                            + ", controller " + snackbarController);
        } else {
            // If we already have an observer, update the connected state.
            observer.setConnected(connected);
            // If we already have an observer and addObserver was called, we should re-enable
            // the connectivity listener in case we go offline again.  This typically happens
            // if a background page comes back to the foreground.
            // To make testing work properly, replace the snackbar controller.  Otherwise the test
            // will use the release version of the snackbar controller instead of its mock.
            if (!connected) {
                observer.enableConnectivityListener(snackbarController);
            }
            Log.d(TAG, "Updated tab observer " + connected + ", bookmarkId " + bookmarkId
                            + ", controller " + snackbarController);
        }
    }

    /**
     * Builds a new OfflinePageTabObserver.
     * @param activity The ChromeActivity of this instance of the browser.
     * @param connected True if the phone is connected when the observer is created.
     * @param bookmarkId Id of the bookmark (offline page) that is associated with this observer.
     * @param snackbarController Controller to use to build the snackbar.
     */
    public OfflinePageTabObserver(ChromeActivity activity, Tab tab, boolean connected,
            long bookmarkId, SnackbarController snackbarController) {
        mActivity = activity;
        mConnected = connected;
        // Remember if the tab was hidden when we started, so we can show the snackbar when
        // the tab becomes visible.
        mWasHidden = tab.isHidden();

        mListener = new OfflinePageConnectivityListener(activity, tab, snackbarController);
        mSnackbarController = snackbarController;

        Log.d(TAG, "OfflinePageTabObserver built");
    }

    public void setConnected(boolean connected) {
        mConnected = connected;
    }

    public void enableConnectivityListener(SnackbarController snackbarController) {
        mListener.enable(snackbarController);
    }

    @Override
    public void onShown(Tab visibleTab) {
        if (mWasHidden) {
            if (mConnected) {
                OfflinePageUtils.showReloadSnackbar(mActivity, mSnackbarController);
            } else {
                OfflinePageUtils.showEditSnackbar(mActivity, mSnackbarController);
            }

            mWasHidden = false;
            Log.d(TAG, "onShown, showing 'delayed' snackbar");
        }
    }

    @Override
    public void onHidden(Tab hiddenTab) {
        mWasHidden = true;
        // TODO(petewil): In case any snackbars are showing, dismiss them before we switch tabs.
        // We will need a handle on the snackbar manager (likely via ChromeActivity) and the
        // snackbar controller (likely via showReload/EditSnackbar).
    }

    @Override
    public void onPageLoadStarted(Tab tab, String newUrl) {
        OfflinePageUtils.preferOnlineVersion(mActivity, tab, newUrl);
    }

    @Override
    public void onDestroyed(Tab destroyedTab) {
        // Unregister this tab for OS connectivity notifications.
        mListener.disable();
        destroyedTab.removeObserver(this);
        sTabObservers.remove(destroyedTab.getId());
        Log.d(TAG, "onDestroyed");
    }

    @Override
    public void onUrlUpdated(Tab reloadingTab) {
        // Unregister this tab for OS connectivity notifications.
        mListener.disable();
        Log.d(TAG, "onUrlUpdated");
    }

    /**
     * Attaches a connectivity listener if needed by this observer.
     * @param tabId The index of the tab that this listener is listening to.
     * @param listener The listener itself.
     */
    public void setConnectivityListener(int tabId, OfflinePageConnectivityListener listener) {
        mListener = listener;
    }
}
