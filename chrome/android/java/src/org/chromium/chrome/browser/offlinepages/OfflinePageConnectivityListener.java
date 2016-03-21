// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;

import org.chromium.base.Log;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.net.NetworkChangeNotifier;

/**
 * A class to listen and respond to network connectivity events while offline pages are active.
 */
public class OfflinePageConnectivityListener
        implements NetworkChangeNotifier.ConnectionTypeObserver {
    private static final String TAG = "OfflinePageCL";
    private Context mContext;
    private SnackbarManager mSnackbarManager;
    private Tab mTab;
    private boolean mSeen;
    private boolean mEnabled;
    private SnackbarController mSnackbarController;

    /**
     * Builds an offline page connectivity listener.
     * @param context android context
     * @param snackbarManager The snackbar manager to show and dismiss snackbars.
     * @param tab The current tab that we are setting up a listener for.
     * @param snackbarController The snackbar controller to control snackbar behavior.
     */
    public OfflinePageConnectivityListener(Context context, SnackbarManager snackbarManager,
            Tab tab, SnackbarController snackbarController) {
        this.mContext = context;
        this.mSnackbarManager = snackbarManager;
        this.mTab = tab;
        this.mSeen = false;
        this.mEnabled = false;
        enable(snackbarController);
    }

    @Override
    public void onConnectionTypeChanged(int connectionType) {
        Log.d(TAG, "Got connectivity event, connectionType: " + connectionType + ", controller "
                        + mSnackbarController);

        boolean connected = NetworkChangeNotifier.isOnline();
        Log.d(TAG, "Connection changed, connected " + connected);

        // TODO(petewil): We should consider using the connection quality monitor instead
        // of the connection event here - don't offer to reload if connection quality is bad.
        // Maybe the NetworkConnectivityListener is the right place to use the quality monitor.

        // Shows or hides the snackbar as needed.  This also adds some hysterisis - if we keep
        // connecting and disconnecting, we don't want to flash the snackbar.  It will timeout after
        // several seconds.
        if (connected && mTab != null && !mSeen) {
            Log.d(TAG, "Connection became available, show reload snackbar.");
            OfflinePageUtils.showOfflineSnackbarIfNecessary(
                    mContext, mSnackbarManager, mTab, mSnackbarController);
            // We can stop listening for online transitions once we go online.
            disable();
            mSeen = true;
        }
    }

    /**
     * Enable the listener when we have an offline page showing.
     * @param snackbarController The snackbar controller to control snackbar behavior.
     */
    public void enable(SnackbarController snackbarController) {
        if (!mEnabled) {
            Log.d(TAG, "enabled");
            NetworkChangeNotifier.addConnectionTypeObserver(this);
            mEnabled = true;
            mSeen = false;
        }
        mSnackbarController = snackbarController;
    }

    /**
     * Disable the listener when we are done with the offline page.
     */
    public void disable() {
        if (mEnabled) {
            Log.d(TAG, "disabled");
            NetworkChangeNotifier.removeConnectionTypeObserver(this);
        }
        mEnabled = false;
    }
}
