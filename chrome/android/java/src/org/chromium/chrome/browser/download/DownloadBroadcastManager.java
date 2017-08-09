// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_CANCEL;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_OPEN;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_PAUSE;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_RESUME;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_RESUME_ALL;
import static org.chromium.chrome.browser.download.DownloadNotificationService.EXTRA_IS_OFF_THE_RECORD;
import static org.chromium.chrome.browser.download.DownloadNotificationService.getContentIdFromIntent;
import static org.chromium.chrome.browser.download.DownloadNotificationService.getServiceDelegate;

import android.content.Context;
import android.content.Intent;
import android.os.Handler;

import com.google.ipc.invalidation.util.Preconditions;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorNotificationBridgeUiFactory;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;

/**
 * Class that spins up native when an interaction with a notification happens and passes the
 * relevant information on to native.
 */
public class DownloadBroadcastManager {
    private static final String TAG = "DLBroadcastManager";
    // TODO(jming): Check to see if this wait time is long enough to execute commands on native.
    private static final int WAIT_TIME_MS = 5000;

    private final DownloadSharedPreferenceHelper mDownloadSharedPreferenceHelper =
            DownloadSharedPreferenceHelper.getInstance();

    private final Handler mHandler = new Handler();
    private final Runnable mStopSelfRunnable = new Runnable() {
        @Override
        public void run() {
            // TODO(jming): Add stopSelf() call when this becomes a service.
        }
    };

    public DownloadBroadcastManager() {}

    /**
     * Passes down information about a notification interaction to native.
     * TODO(jming): Move this call to onStartCommand when this class becomes a service.
     * @param context of the interaction.
     * @param intent with information about the notification interaction (action, contentId, etc).
     */
    public void onNotificationInteraction(Context context, final Intent intent) {
        String action = intent.getAction();
        if (!isActionHandled(action)) return;

        // Remove delayed stop of service until after native library is loaded.
        mHandler.removeCallbacks(mStopSelfRunnable);

        // Handle the intent and propagate it through the native library.
        loadNativeAndPropagateInteraction(context, intent);
    }

    /**
     * Helper function that loads the native and runs given runnable.
     * @param context of the application.
     * @param intent that is propagated when the native is loaded.
     */
    @VisibleForTesting
    void loadNativeAndPropagateInteraction(Context context, final Intent intent) {
        final BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                // Delay the stop of the service by WAIT_TIME_MS after native library is loaded.
                mHandler.postDelayed(mStopSelfRunnable, WAIT_TIME_MS);

                // Make sure the OfflineContentAggregator bridge is initialized.
                OfflineContentAggregatorNotificationBridgeUiFactory.instance();
                propagateInteraction(intent);
            }
        };

        try {
            ChromeBrowserInitializer.getInstance(context).handlePreNativeStartup(parts);
            ChromeBrowserInitializer.getInstance(context).handlePostNativeStartup(true, parts);
        } catch (ProcessInitException e) {
            Log.e(TAG, "Unable to load native library.", e);
            ChromeApplication.reportStartupErrorAndExit(e);
        }
    }

    @VisibleForTesting
    void propagateInteraction(Intent intent) {
        String action = intent.getAction();
        final ContentId id = getContentIdFromIntent(intent);
        final DownloadSharedPreferenceEntry entry = getDownloadEntryFromIntent(intent);
        boolean isOffTheRecord = entry == null
                ? IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OFF_THE_RECORD, false)
                : entry.isOffTheRecord;

        DownloadServiceDelegate downloadServiceDelegate =
                ACTION_DOWNLOAD_OPEN.equals(action) ? null : getServiceDelegate(id);

        switch (action) {
            case ACTION_DOWNLOAD_CANCEL:
                Preconditions.checkNotNull(downloadServiceDelegate);
                Preconditions.checkNotNull(id);

                downloadServiceDelegate.cancelDownload(id, isOffTheRecord);
                break;

            case ACTION_DOWNLOAD_PAUSE:
                Preconditions.checkNotNull(downloadServiceDelegate);
                Preconditions.checkNotNull(id);

                downloadServiceDelegate.pauseDownload(id, isOffTheRecord);
                break;

            case ACTION_DOWNLOAD_RESUME:
                Preconditions.checkNotNull(downloadServiceDelegate);
                Preconditions.checkNotNull(id);

                DownloadInfo info = new DownloadInfo.Builder()
                                            .setDownloadGuid(id.id)
                                            .setIsOffTheRecord(isOffTheRecord)
                                            .build();

                downloadServiceDelegate.resumeDownload(
                        id, new DownloadItem(false, info), true /* hasUserGesture */);
                break;

            case ACTION_DOWNLOAD_OPEN:
                if (LegacyHelpers.isLegacyOfflinePage(id)) {
                    OfflinePageDownloadBridge.openDownloadedPage(id);
                } else if (id != null) {
                    OfflineContentAggregatorNotificationBridgeUiFactory.instance().openItem(id);
                }
                break;

            default:
                // No-op.
                break;
        }

        if (downloadServiceDelegate != null) downloadServiceDelegate.destroyServiceDelegate();
    }

    private boolean isActionHandled(String action) {
        return ACTION_DOWNLOAD_CANCEL.equals(action) || ACTION_DOWNLOAD_PAUSE.equals(action)
                || ACTION_DOWNLOAD_RESUME.equals(action) || ACTION_DOWNLOAD_OPEN.equals(action);
    }

    /**
     * Retrieves DownloadSharedPreferenceEntry from a download action intent.
     * TODO(jming): Make private when no longer needed by DownloadNotificationService.
     * TODO(jming): Instead of getting entire entry, pass only id/isOffTheRecord (crbug.com/749323).
     * @param intent Intent that contains the download action.
     */
    DownloadSharedPreferenceEntry getDownloadEntryFromIntent(Intent intent) {
        if (ACTION_DOWNLOAD_RESUME_ALL.equals(intent.getAction())) return null;
        return mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(
                getContentIdFromIntent(intent));
    }
}
