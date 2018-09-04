// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;

/**
 * Class that controls when to show the offline indicator.
 */
public class OfflineIndicatorController implements ConnectivityDetector.Observer,
                                                   SnackbarController,
                                                   ApplicationStatus.ApplicationStateListener {
    // OfflineIndicatorCTREvent defined in tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    public static final int OFFLINE_INDICATOR_CTR_DISPLAYED = 0;
    public static final int OFFLINE_INDICATOR_CTR_CLICKED = 1;
    public static final int OFFLINE_INDICATOR_CTR_COUNT = 2;

    private static final int SNACKBAR_DURATION_MS = 10000;

    // Time in milliseconds to wait until the offline state is stablized in the case of flaky
    // connections.
    private static final int TIME_TO_WAIT_FOR_STABLE_OFFLINE = 3 * 60 * 1000;

    @SuppressLint("StaticFieldLeak")
    private static OfflineIndicatorController sInstance;

    private static int sTimeToWaitForStableOffline = TIME_TO_WAIT_FOR_STABLE_OFFLINE;

    private boolean mIsShowingOfflineIndicator;
    // Set to true if the offline indicator has been shown once.
    private boolean mHasOfflineIndicatorShown;
    private ConnectivityDetector mConnectivityDetector;
    private ChromeActivity mObservedActivity = null;

    private boolean mIsOnline;
    // Last time when the online state is detected. It is recorded as milliseconds since boot.
    private long mLastOnlineTime;

    private TopSnackbarManager mTopSnackbarManager;

    private OfflineIndicatorController() {
        if (isUsingTopSnackbar()) {
            mTopSnackbarManager = new TopSnackbarManager();
        }
        mConnectivityDetector = new ConnectivityDetector(this);
        ApplicationStatus.registerApplicationStateListener(this);
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

    @Override
    public void onConnectionStateChanged(
            @ConnectivityDetector.ConnectionState int connectionState) {
        if (connectionState == ConnectivityDetector.ConnectionState.NONE) return;
        updateOfflineIndicator(connectionState == ConnectivityDetector.ConnectionState.VALIDATED);
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

    @Override
    public void onApplicationStateChange(int newState) {
        // If the application is resumed, update the connection state and show indicator if needed.
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            mConnectivityDetector.detect();
            updateOfflineIndicator(mConnectivityDetector.getConnectionState()
                    == ConnectivityDetector.ConnectionState.VALIDATED);
        }
    }

    private void updateOfflineIndicator(boolean isOnline) {
        if (isOnline != mIsOnline) {
            if (isOnline) {
                mLastOnlineTime = SystemClock.elapsedRealtime();
            }
            mIsOnline = isOnline;
        }

        if (ApplicationStatus.getStateForApplication() != ApplicationState.HAS_RUNNING_ACTIVITIES) {
            return;
        }
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null) return;

        if (isOnline) {
            hideOfflineIndicator(activity);
        } else {
            showOfflineIndicator(activity);
        }
    }

    private boolean canShowOfflineIndicator(Activity activity) {
        // For now, we only support ChromeActivity.
        if (!(activity instanceof ChromeActivity)) return false;

        ChromeActivity chromeActivity = (ChromeActivity) activity;
        Tab tab = chromeActivity.getActivityTab();
        if (tab == null) return false;
        if (tab.isShowingErrorPage()) return false;
        if (OfflinePageUtils.isOfflinePage(tab)) return false;
        if (TextUtils.equals(tab.getUrl(), ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL)) {
            return false;
        }

        return true;
    }

    /**
     * Delay showing the offline indicator UI under some circumstances, i.e. current tab is still
     * being loaded.
     * Returns true if the offline indicator UI is delayed to be shown.
     */
    private boolean delayShowingOfflineIndicatorIfNeeded(Activity activity) {
        if (!(activity instanceof ChromeActivity)) return false;

        ChromeActivity chromeActivity = (ChromeActivity) activity;
        Tab tab = chromeActivity.getActivityTab();
        if (tab == null) return false;

        WebContents webContents = tab.getWebContents();
        if (webContents != null && !webContents.isLoading()) return false;

        // If the tab is still being loaded, we should wait until it finishes.
        if (mObservedActivity == chromeActivity) return true;
        mObservedActivity = chromeActivity;
        TabObserver tabObserver = new EmptyTabObserver() {
            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                mObservedActivity = null;
                tab.removeObserver(this);
                doUpdate();
            }

            @Override
            public void onHidden(Tab tab) {
                mObservedActivity = null;
                tab.removeObserver(this);
                doUpdate();
            }

            @Override
            public void onDestroyed(Tab tab) {
                mObservedActivity = null;
                tab.removeObserver(this);
                doUpdate();
            }

            private void doUpdate() {
                updateOfflineIndicator(mConnectivityDetector.getConnectionState()
                        == ConnectivityDetector.ConnectionState.VALIDATED);
            }
        };
        tab.addObserver(tabObserver);
        return true;
    }

    private void showOfflineIndicator(Activity activity) {
        if (mIsShowingOfflineIndicator || !canShowOfflineIndicator(activity)) return;

        if (delayShowingOfflineIndicatorIfNeeded(activity)) return;

        // If this is the first time to show offline indicator, show it. Otherwise, it will only
        // be shown if the user has been continuously online for the required duration, then goes
        // back to being offline.
        // TODO(jianli): keep these values in shared prefernces. (http://crbug.com/879725)
        if (mHasOfflineIndicatorShown
                && SystemClock.elapsedRealtime() - mLastOnlineTime < sTimeToWaitForStableOffline) {
            return;
        }

        Drawable icon =
                AppCompatResources.getDrawable(activity, R.drawable.ic_offline_pin_blue_white);
        Snackbar snackbar =
                Snackbar.make(activity.getString(R.string.offline_indicator_offline_title), this,
                                Snackbar.TYPE_ACTION, Snackbar.UMA_OFFLINE_INDICATOR)
                        .setSingleLine(true)
                        .setProfileImage(icon)
                        .setBackgroundColor(Color.BLACK)
                        .setTextAppearance(R.style.WhiteBody)
                        .setDuration(SNACKBAR_DURATION_MS)
                        .setAction(
                                activity.getString(R.string.offline_indicator_view_offline_content),
                                null);
        if (isUsingTopSnackbar()) {
            mTopSnackbarManager.show(snackbar, activity);
        } else {
            // Show a bottom snackbar via SnackbarManager.
            assert activity instanceof SnackbarManageable;
            SnackbarManager snackbarManager = ((SnackbarManageable) activity).getSnackbarManager();
            snackbarManager.showSnackbar(snackbar);
        }

        RecordHistogram.recordEnumeratedHistogram("OfflineIndicator.CTR",
                OFFLINE_INDICATOR_CTR_DISPLAYED, OFFLINE_INDICATOR_CTR_COUNT);
        mIsShowingOfflineIndicator = true;
        mHasOfflineIndicatorShown = true;
    }

    private void hideOfflineIndicator(Activity activity) {
        if (!mIsShowingOfflineIndicator) return;

        if (isUsingTopSnackbar()) {
            mTopSnackbarManager.hide();
        } else {
            assert activity instanceof SnackbarManageable;
            SnackbarManager snackbarManager = ((SnackbarManageable) activity).getSnackbarManager();
            snackbarManager.dismissSnackbars(this);
        }
    }

    @VisibleForTesting
    static boolean isUsingTopSnackbar() {
        return true;
    }

    @VisibleForTesting
    ConnectivityDetector getConnectivityDetectorForTesting() {
        return mConnectivityDetector;
    }

    @VisibleForTesting
    TopSnackbarManager getTopSnackbarManagerForTesting() {
        return mTopSnackbarManager;
    }

    @VisibleForTesting
    static void overrideTimeToWaitForStableOfflineForTesting(int time) {
        sTimeToWaitForStableOffline = time;
    }
}
