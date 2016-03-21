// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;

import org.chromium.base.Log;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;

import java.util.Map;
import java.util.WeakHashMap;

/**
 * A class that observes events for a tab which has an associated offline page.  This will be
 * created when needed (for instance, we want to show a snackbar when the tab is shown or we want
 * to show a snackbar if the device connects).  It will be removed when the user navigates away
 * from the offline tab.
 */
public class OfflinePageTabObserver extends EmptyTabObserver {
    private static final String TAG = "OfflinePageTO";
    private Context mContext;
    private SnackbarManager mSnackbarManager;
    private boolean mConnected;
    private SnackbarController mSnackbarController;
    private boolean mWasHidden;
    private OfflinePageConnectivityListener mListener;

    private static final Map<Integer, OfflinePageTabObserver> sTabObservers =
            new WeakHashMap<Integer, OfflinePageTabObserver>();

    /**
     * Create and attach a tab observer if we don't already have one, otherwise update it.
     * @param context Android context.
     * @param snackbarManager The snackbar manager to show and dismiss snackbars.
     * @param tab The tab we are adding an observer for.
     * @param connected True if we were connected when this call was made.
     * @param snackbarController The snackbar controller to control snackbar behavior.
     */
    public static void addObserverForTab(Context context, SnackbarManager snackbarManager, Tab tab,
            boolean connected, SnackbarController snackbarController) {
        // See if we already have an observer for this tab.
        int tabId = tab.getId();
        OfflinePageTabObserver observer = sTabObservers.get(tabId);

        if (observer == null) {
            // If we don't have an observer, build one and attach it to the tab.
            observer = new OfflinePageTabObserver(
                    context, snackbarManager, tab, connected, snackbarController);
            sTabObservers.put(tabId, observer);
            tab.addObserver(observer);
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
        }
    }

    /**
     * Removes the observer for a tab with the specified tabId.
     * @param tab The tab to remove an observer for.
     */
    private static void removeObserverForTab(Tab tab) {
        OfflinePageTabObserver observer = sTabObservers.get(tab.getId());
        if (observer != null) {
            tab.removeObserver(observer);
        }
        sTabObservers.remove(tab.getId());
    }

    /**
     * Builds a new OfflinePageTabObserver.
     * @param context Android context.
     * @param snackbarManager The snackbar manager to show and dismiss snackbars.
     * @param tab The tab we are adding an observer for.
     * @param connected True if the phone is connected when the observer is created.
     * @param snackbarController Controller to use to build the snackbar.
     */
    OfflinePageTabObserver(Context context, SnackbarManager snackbarManager, Tab tab,
            boolean connected, SnackbarController snackbarController) {
        mContext = context;
        mSnackbarManager = snackbarManager;
        mConnected = connected;
        // Remember if the tab was hidden when we started, so we can show the snackbar when
        // the tab becomes visible.
        mWasHidden = tab.isHidden();

        mListener = new OfflinePageConnectivityListener(
                context, snackbarManager, tab, snackbarController);
        mSnackbarController = snackbarController;
    }

    private void setConnected(boolean connected) {
        mConnected = connected;
    }

    private void enableConnectivityListener(SnackbarController snackbarController) {
        mListener.enable(snackbarController);
    }

    // Methods from EmptyTabObserver
    @Override
    public void onShown(Tab visibleTab) {
        // If there is no bookmark on this tab, there is nothing to do.
        long bookmarkNum = visibleTab.getBookmarkId();
        if (bookmarkNum == Tab.INVALID_BOOKMARK_ID) return;

        if (mWasHidden) {
            if (mConnected) {
                OfflinePageUtils.showReloadSnackbar(
                        mContext, mSnackbarManager, mSnackbarController);
            }

            mWasHidden = false;
            Log.d(TAG, "onShown, showing 'delayed' snackbar");
        }
    }

    @Override
    public void onHidden(Tab hiddenTab) {
        mWasHidden = true;
        // In case any snackbars are showing, dismiss them before we switch tabs.
        mSnackbarManager.dismissSnackbars(mSnackbarController);
    }

    @Override
    public void onPageLoadStarted(Tab tab, String newUrl) {
        OfflinePageUtils.preferOnlineVersion(tab, newUrl);
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
        removeObserverForTab(reloadingTab);
        // In case any snackbars are showing, dismiss them before we navigate away.
        mSnackbarManager.dismissSnackbars(mSnackbarController);
    }

    /**
     * Attaches a connectivity listener if needed by this observer.
     * @param tabId The index of the tab that this listener is listening to.
     * @param listener The listener itself.
     */
    private void setConnectivityListener(int tabId, OfflinePageConnectivityListener listener) {
        mListener = listener;
    }
}
