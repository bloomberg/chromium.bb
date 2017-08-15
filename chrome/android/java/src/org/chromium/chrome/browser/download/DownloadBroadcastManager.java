// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static android.app.DownloadManager.ACTION_NOTIFICATION_CLICKED;

import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_CANCEL;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_OPEN;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_PAUSE;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_RESUME;
import static org.chromium.chrome.browser.download.DownloadNotificationService.EXTRA_DOWNLOAD_CONTENTID_ID;
import static org.chromium.chrome.browser.download.DownloadNotificationService.EXTRA_DOWNLOAD_CONTENTID_NAMESPACE;
import static org.chromium.chrome.browser.download.DownloadNotificationService.EXTRA_IS_OFF_THE_RECORD;
import static org.chromium.chrome.browser.download.DownloadNotificationService.MAX_RESUMPTION_ATTEMPT_LEFT;
import static org.chromium.chrome.browser.download.DownloadNotificationService.getServiceDelegate;
import static org.chromium.chrome.browser.download.DownloadNotificationService.updateResumptionAttemptLeft;

import android.app.DownloadManager;
import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.IBinder;
import android.support.annotation.Nullable;

import com.google.ipc.invalidation.util.Preconditions;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorNotificationBridgeUiFactory;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;

/**
 * Class that spins up native when an interaction with a notification happens and passes the
 * relevant information on to native.
 */
public class DownloadBroadcastManager extends Service {
    private static final String TAG = "DLBroadcastManager";
    // TODO(jming): Check to see if this wait time is long enough to execute commands on native.
    private static final int WAIT_TIME_MS = 5000;

    private final DownloadSharedPreferenceHelper mDownloadSharedPreferenceHelper =
            DownloadSharedPreferenceHelper.getInstance();

    private final Context mApplicationContext;
    private final Handler mHandler = new Handler();
    private final Runnable mStopSelfRunnable = new Runnable() {
        @Override
        public void run() {
            stopSelf();
        }
    };

    public DownloadBroadcastManager() {
        mApplicationContext = ContextUtils.getApplicationContext();
    }

    public static void startDownloadBroadcastManager(Context context, Intent source) {
        Intent intent = source != null ? new Intent(source) : new Intent();
        intent.setComponent(new ComponentName(context, DownloadBroadcastManager.class));
        context.startService(intent);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Handle the download operation.
        onNotificationInteraction(intent);

        // Handle resumption tracking logic.
        DownloadResumptionScheduler.getDownloadResumptionScheduler(mApplicationContext)
                .cancelTask();
        updateResumptionAttemptLeft(MAX_RESUMPTION_ATTEMPT_LEFT);

        // If Chrome gets killed, do not restart the service.
        return START_NOT_STICKY;
    }

    /**
     * Passes down information about a notification interaction to native.
     * @param intent with information about the notification interaction (action, contentId, etc).
     */
    public void onNotificationInteraction(final Intent intent) {
        if (!isActionHandled(intent)) return;

        // Remove delayed stop of service until after native library is loaded.
        mHandler.removeCallbacks(mStopSelfRunnable);

        // TODO(jming): When DownloadNotificationService is no longer a service, invoke it to
        // propagate immediate notification changes (ie. when pause is clicked, have progress bar be
        // paused even before there is an update in native). http://crbug.com/755588

        // Handle the intent and propagate it through the native library.
        loadNativeAndPropagateInteraction(intent);
    }

    /**
     * Helper function that loads the native and runs given runnable.
     * @param intent that is propagated when the native is loaded.
     */
    @VisibleForTesting
    void loadNativeAndPropagateInteraction(final Intent intent) {
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
            ChromeBrowserInitializer.getInstance(mApplicationContext).handlePreNativeStartup(parts);
            ChromeBrowserInitializer.getInstance(mApplicationContext)
                    .handlePostNativeStartup(true, parts);
        } catch (ProcessInitException e) {
            Log.e(TAG, "Unable to load native library.", e);
            ChromeApplication.reportStartupErrorAndExit(e);
        }
    }

    @VisibleForTesting
    void propagateInteraction(Intent intent) {
        String action = intent.getAction();
        final ContentId id = getContentIdFromIntent(intent);

        switch (action) {
            case ACTION_NOTIFICATION_CLICKED:
                openDownload(mApplicationContext, intent, id);
                return;

            case ACTION_DOWNLOAD_OPEN:
                if (LegacyHelpers.isLegacyOfflinePage(id)) {
                    OfflinePageDownloadBridge.openDownloadedPage(id);
                } else if (id != null) {
                    OfflineContentAggregatorNotificationBridgeUiFactory.instance().openItem(id);
                }
                return;
        }

        final DownloadSharedPreferenceEntry entry = getDownloadEntryFromIntent(intent);
        boolean isOffTheRecord = entry == null
                ? IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OFF_THE_RECORD, false)
                : entry.isOffTheRecord;
        DownloadServiceDelegate downloadServiceDelegate = getServiceDelegate(id);

        Preconditions.checkNotNull(downloadServiceDelegate);
        Preconditions.checkNotNull(id);

        switch (action) {
            case ACTION_DOWNLOAD_CANCEL:
                downloadServiceDelegate.cancelDownload(id, isOffTheRecord);
                break;

            case ACTION_DOWNLOAD_PAUSE:
                downloadServiceDelegate.pauseDownload(id, isOffTheRecord);
                break;

            case ACTION_DOWNLOAD_RESUME:
                DownloadInfo info = new DownloadInfo.Builder()
                                            .setDownloadGuid(id.id)
                                            .setIsOffTheRecord(isOffTheRecord)
                                            .build();

                downloadServiceDelegate.resumeDownload(
                        id, new DownloadItem(false, info), true /* hasUserGesture */);
                break;

            default:
                // No-op.
                break;
        }

        downloadServiceDelegate.destroyServiceDelegate();
    }

    static boolean isActionHandled(Intent intent) {
        if (intent == null) return false;
        String action = intent.getAction();
        return ACTION_DOWNLOAD_CANCEL.equals(action) || ACTION_DOWNLOAD_PAUSE.equals(action)
                || ACTION_DOWNLOAD_RESUME.equals(action) || ACTION_DOWNLOAD_OPEN.equals(action)
                || ACTION_NOTIFICATION_CLICKED.equals(action);
    }

    /**
     * Retrieves DownloadSharedPreferenceEntry from a download action intent.
     * TODO(jming): Instead of getting entire entry, pass only id/isOffTheRecord (crbug.com/749323).
     * @param intent Intent that contains the download action.
     */
    private DownloadSharedPreferenceEntry getDownloadEntryFromIntent(Intent intent) {
        return mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(
                getContentIdFromIntent(intent));
    }

    /**
     * @param intent The {@link Intent} to pull from and build a {@link ContentId}.
     * @return A {@link ContentId} built by pulling extras from {@code intent}.  This will be
     *         {@code null} if {@code intent} is missing any required extras.
     */
    private static ContentId getContentIdFromIntent(Intent intent) {
        if (!intent.hasExtra(EXTRA_DOWNLOAD_CONTENTID_ID)
                || !intent.hasExtra(EXTRA_DOWNLOAD_CONTENTID_NAMESPACE)) {
            return null;
        }

        return new ContentId(
                IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_CONTENTID_NAMESPACE),
                IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_CONTENTID_ID));
    }

    /**
     * Called to open a particular download item.  Falls back to opening Download Home.
     * @param context Context of the receiver.
     * @param intent Intent from the android DownloadManager.
     */
    private void openDownload(Context context, Intent intent, ContentId contentId) {
        int notificationId = IntentUtils.safeGetIntExtra(
                intent, NotificationConstants.EXTRA_NOTIFICATION_ID, -1);
        DownloadNotificationService.hideDanglingSummaryNotification(context, notificationId);

        long ids[] =
                intent.getLongArrayExtra(DownloadManager.EXTRA_NOTIFICATION_CLICK_DOWNLOAD_IDS);
        if (ids == null || ids.length == 0) {
            DownloadManagerService.openDownloadsPage(context);
            return;
        }

        long id = ids[0];
        Uri uri = DownloadManagerDelegate.getContentUriFromDownloadManager(context, id);
        if (uri == null) {
            DownloadManagerService.openDownloadsPage(context);
            return;
        }

        String downloadFilename = IntentUtils.safeGetStringExtra(
                intent, DownloadNotificationService.EXTRA_DOWNLOAD_FILE_PATH);
        boolean isSupportedMimeType = IntentUtils.safeGetBooleanExtra(
                intent, DownloadNotificationService.EXTRA_IS_SUPPORTED_MIME_TYPE, false);
        boolean isOffTheRecord = IntentUtils.safeGetBooleanExtra(
                intent, DownloadNotificationService.EXTRA_IS_OFF_THE_RECORD, false);
        String originalUrl = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_ORIGINATING_URI);
        String referrer = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_REFERRER);
        DownloadManagerService.openDownloadedContent(context, downloadFilename, isSupportedMimeType,
                isOffTheRecord, contentId.id, id, originalUrl, referrer);
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        // Since this service does not need to be bound, just return null.
        return null;
    }
}
