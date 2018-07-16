// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.support.v7.content.res.AppCompatResources;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.DownloadActivity;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;

/**
 * Class that controls when to show the offline indicator.
 */
public class OfflineIndicatorController
        implements NetworkChangeNotifier.ConnectionTypeObserver, SnackbarController {
    // OfflineIndicatorCTREvent defined in tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    public static final int OFFLINE_INDICATOR_CTR_DISPLAYED = 0;
    public static final int OFFLINE_INDICATOR_CTR_CLICKED = 1;
    public static final int OFFLINE_INDICATOR_CTR_COUNT = 2;

    private static final int DURATION_MS = 10000;

    private static boolean sSkipSystemCheckForTesting = false;

    @SuppressLint("StaticFieldLeak")
    private static OfflineIndicatorController sInstance;

    private boolean mIsOffline = false;
    private boolean mIsShowingOfflineIndicator = false;

    private OfflineIndicatorController() {
        NetworkChangeNotifier.addConnectionTypeObserver(this);
        updateConnectionType();
    }

    /**
     * Initializes the singleton once.
     */
    public static void initialize() {
        // No need to create the singleton if the feature is not enabled.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_INDICATOR)) return;

        if (sInstance == null) {
            sInstance = new OfflineIndicatorController();
        }
    }

    /**
     * Returns the singleton instance.
     */
    public static OfflineIndicatorController getInstance() {
        assert sInstance != null;
        return sInstance;
    }

    /**
     * Called to update the offline indicator per current connection state when the activity is
     * running in foreground now or the tab is being shown.
     */
    public static void onUpdate() {
        if (sInstance == null) return;
        sInstance.updateConnectionType();
    }

    @Override
    public void onConnectionTypeChanged(int connectionType) {
        if (connectionType == ConnectionType.CONNECTION_NONE) {
            mIsOffline = true;
        } else {
            if (!performSystemCheckForValidatedNetwork()) {
                mIsOffline = false;
            }
        }

        updateOfflineIndicator();
    }

    @Override
    public void onAction(Object actionData) {
        mIsShowingOfflineIndicator = false;
        DownloadUtils.showDownloadManager(null, null, true /*showPrefetchedContent*/);
        RecordHistogram.recordEnumeratedHistogram(
                "OfflineIndicator.CTR", OFFLINE_INDICATOR_CTR_CLICKED, OFFLINE_INDICATOR_CTR_COUNT);
    }

    @Override
    public void onDismissNoAction(Object actionData) {
        mIsShowingOfflineIndicator = false;
    }

    /**
     * Consults with the Android connection manager to find out if there is a validated network.
     * Returns true if the check is performed. A validated network is one of the following:
     * 1) a functioning network providing Internet access
     * 2) a captive portal and the user decided to use it as is
     */
    @TargetApi(Build.VERSION_CODES.M)
    private boolean performSystemCheckForValidatedNetwork() {
        if (sSkipSystemCheckForTesting) return false;

        // NetworkCapabilities.NET_CAPABILITY_VALIDATED is only available on Marshmallow and
        // later versions.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;
        ConnectivityManager connectivityManager =
                (ConnectivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        if (connectivityManager == null) return false;

        Network[] networks = connectivityManager.getAllNetworks();
        for (Network network : networks) {
            NetworkCapabilities capabilities = connectivityManager.getNetworkCapabilities(network);
            if (capabilities != null
                    && capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)) {
                mIsOffline = false;
                return true;
            }
        }

        mIsOffline = true;
        return true;
    }

    private void updateConnectionType() {
        onConnectionTypeChanged(NetworkChangeNotifier.getInstance().getCurrentConnectionType());
    }

    private void updateOfflineIndicator() {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof SnackbarManageable)) return;
        SnackbarManager snackbarManager = ((SnackbarManageable) activity).getSnackbarManager();
        if (snackbarManager == null) return;

        if (mIsOffline) {
            showOfflineIndicator(activity, snackbarManager);
        } else {
            hideOfflineIndicator(snackbarManager);
        }
    }

    private boolean canShowOfflineIndicator(Activity activity) {
        // No need to show offline indicator since Downloads home is already home of the offline
        // content.
        if (activity instanceof DownloadActivity) return false;

        if (activity instanceof ChromeActivity) {
            ChromeActivity chromeActivity = (ChromeActivity) activity;
            Tab tab = chromeActivity.getActivityTab();
            if (tab == null) return false;
            if (tab.isShowingErrorPage()) return false;
        }

        return true;
    }

    private void showOfflineIndicator(Activity activity, SnackbarManager snackbarManager) {
        if (mIsShowingOfflineIndicator || !canShowOfflineIndicator(activity)) return;

        Drawable icon =
                AppCompatResources.getDrawable(activity, R.drawable.ic_offline_pin_blue_white);
        Snackbar snackbar =
                Snackbar.make(activity.getString(R.string.offline_indicator_offline_title), this,
                                Snackbar.TYPE_ACTION, Snackbar.UMA_OFFLINE_INDICATOR)
                        .setSingleLine(true)
                        .setProfileImage(icon)
                        .setBackgroundColor(Color.BLACK)
                        .setTextAppearance(R.style.WhiteBody)
                        .setAction(
                                activity.getString(R.string.offline_indicator_view_offline_content),
                                null);
        snackbar.setDuration(DURATION_MS);
        snackbarManager.showSnackbar(snackbar);
        RecordHistogram.recordEnumeratedHistogram("OfflineIndicator.CTR",
                OFFLINE_INDICATOR_CTR_DISPLAYED, OFFLINE_INDICATOR_CTR_COUNT);
        mIsShowingOfflineIndicator = true;
    }

    private void hideOfflineIndicator(SnackbarManager snackbarManager) {
        if (!mIsShowingOfflineIndicator) return;
        snackbarManager.dismissSnackbars(this);
    }

    @VisibleForTesting
    static void skipSystemCheckForTesting() {
        sSkipSystemCheckForTesting = true;
    }
}
