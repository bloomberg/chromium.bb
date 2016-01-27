// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;

/**
 * A class to listen and respond to network connectivity events while offline pages are active.
 */
public class OfflinePageConnectivityListener
        implements NetworkChangeNotifier.ConnectionTypeObserver {
    private static final String TAG = "OfflinePageCL";
    private Tab mTab;
    private ChromeActivity mActivity;
    private boolean mSeen;
    private boolean mEnabled;

    /**
     * Builds an offline page connectivity listener.
     * @param activity The ChromeActivity that we are listening for.
     * @param tab The current tab that we are setting up a listener for.
     */
    public OfflinePageConnectivityListener(ChromeActivity activity, Tab tab) {
        Log.d(TAG, "OfflinePageConnectivityListener constructor, this: " + this);
        this.mActivity = activity;
        this.mTab = tab;
        this.mSeen = false;
        this.mEnabled = false;
        enable();
    }

    @Override
    public void onConnectionTypeChanged(int connectionType) {
        Log.d(TAG, "Got connectivity event, connectionType: " + connectionType);

        boolean connected = (connectionType != ConnectionType.CONNECTION_NONE
                && connectionType != ConnectionType.CONNECTION_BLUETOOTH);

        // TODO(petewil): We should consider using the connection quality monitor instead
        // of the connection event here - don't offer to reload if connection quality is bad.
        // Maybe the NetworkConnectivityListener is the right place to use the quality monitor.

        // Shows or hides the snackbar as needed.  This also adds some hysterisis - if we keep
        // connecting and disconnecting, we don't want to flash the snackbar.  It will timeout at 3
        // seconds.
        if (connected && mTab != null && !mSeen) {
            Log.d(TAG, "Showing reload snackbar");
            OfflinePageUtils.showOfflineSnackbarIfNecessary(mActivity, mTab);
            // We can stop listening for online transitions once we go online.
            disable();
            mSeen = true;
        }
    }

    /**
     * Enable the listener when we have an offline page showing.
     */
    public void enable() {
        if (!mEnabled) {
            Log.d(TAG, "enabled");
            NetworkChangeNotifier.addConnectionTypeObserver(this);
            mEnabled = true;
            mSeen = false;
        }
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
