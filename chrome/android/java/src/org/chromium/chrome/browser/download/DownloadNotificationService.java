// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.ActivityManager;
import android.app.DownloadManager;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.shapes.OvalShape;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.support.v4.app.NotificationCompat;
import android.text.TextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.util.IntentUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;


/**
 * Service responsible for creating and updating download notifications even after
 * Chrome gets killed.
 */
public class DownloadNotificationService extends Service {
    static final String EXTRA_DOWNLOAD_GUID = "DownloadGuid";
    static final String EXTRA_DOWNLOAD_FILE_PATH = "DownloadFilePath";
    static final String EXTRA_NOTIFICATION_DISMISSED = "NotificationDismissed";
    static final String EXTRA_IS_SUPPORTED_MIME_TYPE = "IsSupportedMimeType";
    static final String EXTRA_IS_OFF_THE_RECORD =
            "org.chromium.chrome.browser.download.IS_OFF_THE_RECORD";
    static final String EXTRA_IS_OFFLINE_PAGE =
            "org.chromium.chrome.browser.download.IS_OFFLINE_PAGE";

    public static final String ACTION_DOWNLOAD_CANCEL =
            "org.chromium.chrome.browser.download.DOWNLOAD_CANCEL";
    public static final String ACTION_DOWNLOAD_PAUSE =
            "org.chromium.chrome.browser.download.DOWNLOAD_PAUSE";
    public static final String ACTION_DOWNLOAD_RESUME =
            "org.chromium.chrome.browser.download.DOWNLOAD_RESUME";
    static final String ACTION_DOWNLOAD_RESUME_ALL =
            "org.chromium.chrome.browser.download.DOWNLOAD_RESUME_ALL";
    public static final String ACTION_DOWNLOAD_OPEN =
            "org.chromium.chrome.browser.download.DOWNLOAD_OPEN";

    static final String NOTIFICATION_NAMESPACE = "DownloadNotificationService";
    private static final String TAG = "DownloadNotification";
    // Limit file name to 25 characters. TODO(qinmin): use different limit for different devices?
    private static final int MAX_FILE_NAME_LENGTH = 25;

    /** Notification Id starting value, to avoid conflicts from IDs used in prior versions. */
    private static final int STARTING_NOTIFICATION_ID = 1000000;
    private static final int MAX_RESUMPTION_ATTEMPT_LEFT = 5;
    @VisibleForTesting static final long SECONDS_PER_MINUTE = TimeUnit.MINUTES.toSeconds(1);
    @VisibleForTesting static final long SECONDS_PER_HOUR = TimeUnit.HOURS.toSeconds(1);
    @VisibleForTesting static final long SECONDS_PER_DAY = TimeUnit.DAYS.toSeconds(1);

    private static final String KEY_AUTO_RESUMPTION_ATTEMPT_LEFT = "ResumptionAttemptLeft";
    private static final String KEY_NEXT_DOWNLOAD_NOTIFICATION_ID = "NextDownloadNotificationId";

    private final IBinder mBinder = new LocalBinder();
    private final List<String> mDownloadsInProgress = new ArrayList<String>();

    private NotificationManager mNotificationManager;
    private SharedPreferences mSharedPrefs;
    private Context mContext;
    private int mNextNotificationId;
    private int mNumAutoResumptionAttemptLeft;
    private boolean mStopPostingProgressNotifications;
    private Bitmap mDownloadSuccessLargeIcon;
    private DownloadSharedPreferenceHelper mDownloadSharedPreferenceHelper;

   /**
     * Class for clients to access.
     */
    public class LocalBinder extends Binder {
        DownloadNotificationService getService() {
            return DownloadNotificationService.this;
        }
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            ActivityManager am = (ActivityManager) mContext.getSystemService(
                    Context.ACTIVITY_SERVICE);
            List<ActivityManager.AppTask> tasks = am.getAppTasks();
            // In multi-window case, there could be multiple tasks. Only swiping away the last
            // activity should be pause the notification.
            if (tasks.size() > 0) return;
        }
        // This funcion is called when Chrome is swiped away from the recent apps
        // drawer. So it doesn't catch all scenarios that chrome can get killed.
        // This will only help Android 4.4.2.
        onBrowserKilled();
        mStopPostingProgressNotifications = true;
    }

    @Override
    public void onCreate() {
        mStopPostingProgressNotifications = false;
        mContext = getApplicationContext();
        mNotificationManager = (NotificationManager) mContext.getSystemService(
                Context.NOTIFICATION_SERVICE);
        mSharedPrefs = ContextUtils.getAppSharedPreferences();
        mNumAutoResumptionAttemptLeft = mSharedPrefs.getInt(KEY_AUTO_RESUMPTION_ATTEMPT_LEFT,
                MAX_RESUMPTION_ATTEMPT_LEFT);
        mDownloadSharedPreferenceHelper = DownloadSharedPreferenceHelper.getInstance();
        // Because this service is a started service and returns START_STICKY in
        // onStartCommand(), it will be restarted as soon as resources are available
        // after it is killed. As a result, onCreate() may be called after Chrome
        // gets killed and before user restarts chrome. In that case,
        // DownloadManagerService.hasDownloadManagerService() will return false as
        // there are no calls to initialize DownloadManagerService. Pause all the
        // download notifications as download will not progress without chrome.
        if (!DownloadManagerService.hasDownloadManagerService()) {
            onBrowserKilled();
        }
        mNextNotificationId = mSharedPrefs.getInt(
                KEY_NEXT_DOWNLOAD_NOTIFICATION_ID, STARTING_NOTIFICATION_ID);
    }

    /**
     * Called when browser is killed. Schedule a resumption task and pause all the download
     * notifications.
     */
    private void onBrowserKilled() {
        cancelOffTheRecordNotifications();
        pauseAllDownloads();
        List<DownloadSharedPreferenceEntry> entries = mDownloadSharedPreferenceHelper.getEntries();
        if (!entries.isEmpty()) {
            boolean scheduleAutoResumption = false;
            boolean allowMeteredConnection = false;
            for (int i = 0; i < entries.size(); ++i) {
                DownloadSharedPreferenceEntry entry = entries.get(i);
                if (entry.isAutoResumable) {
                    scheduleAutoResumption = true;
                    if (entry.canDownloadWhileMetered) {
                        allowMeteredConnection = true;
                        break;
                    }
                }
            }
            if (scheduleAutoResumption && mNumAutoResumptionAttemptLeft > 0) {
                DownloadResumptionScheduler.getDownloadResumptionScheduler(mContext).schedule(
                        allowMeteredConnection);
            }
        }
        stopSelf();
    }

    @Override
    public int onStartCommand(final Intent intent, int flags, int startId) {
        if (isDownloadOperationIntent(intent)) {
            handleDownloadOperation(intent);
            DownloadResumptionScheduler.getDownloadResumptionScheduler(mContext).cancelTask();
            // Limit the number of auto resumption attempts in case Chrome falls into a vicious
            // cycle.
            if (intent.getAction() == ACTION_DOWNLOAD_RESUME_ALL) {
                if (mNumAutoResumptionAttemptLeft > 0) {
                    mNumAutoResumptionAttemptLeft--;
                    updateResumptionAttemptLeft();
                }
            } else {
                // Reset number of attempts left if the action is triggered by user.
                mNumAutoResumptionAttemptLeft = MAX_RESUMPTION_ATTEMPT_LEFT;
                clearResumptionAttemptLeft();
            }
        }
        // This should restart the service after Chrome gets killed. However, this
        // doesn't work on Android 4.4.2.
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    /**
     * Helper method to update the remaining number of background resumption attempts left.
     * @param attamptLeft Number of attempt left.
     */
    private void updateResumptionAttemptLeft() {
        SharedPreferences.Editor editor = mSharedPrefs.edit();
        editor.putInt(KEY_AUTO_RESUMPTION_ATTEMPT_LEFT, mNumAutoResumptionAttemptLeft);
        editor.apply();
    }

    /**
     * Helper method to clear the remaining number of background resumption attempts left.
     */
    static void clearResumptionAttemptLeft() {
        SharedPreferences SharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = SharedPrefs.edit();
        editor.remove(KEY_AUTO_RESUMPTION_ATTEMPT_LEFT);
        editor.apply();
    }

    /**
     * Adds or updates an in-progress download notification.
     * @param downloadGuid GUID of the download.
     * @param fileName File name of the download.
     * @param percentage Percentage completed. Value should be between 0 to 100 if
     *        the percentage can be determined, or -1 if it is unknown.
     * @param bytesReceived Total number of bytes received.
     * @param timeRemainingInMillis Remaining download time in milliseconds.
     * @param startTime Time when download started.
     * @param isOffTheRecord Whether the download is off the record.
     * @param canDownloadWhileMetered Whether the download can happen in metered network.
     * @param isOfflinePage Whether the download is for offline page.
     */
    @VisibleForTesting
    public void notifyDownloadProgress(String downloadGuid, String fileName, int percentage,
            long bytesReceived, long timeRemainingInMillis, long startTime, boolean isOffTheRecord,
            boolean canDownloadWhileMetered, boolean isOfflinePage) {
        updateActiveDownloadNotification(downloadGuid, fileName, percentage, bytesReceived,
                timeRemainingInMillis, startTime, isOffTheRecord, canDownloadWhileMetered,
                isOfflinePage, false);
    }

    /**
     * Adds or updates a pending download notification.
     * @param downloadGuid GUID of the download.
     * @param fileName File name of the download.
     * @param isOffTheRecord Whether the download is off the record.
     * @param canDownloadWhileMetered Whether the download can happen in metered network.
     * @param isOfflinePage Whether the download is for offline page.
     */
    private void notifyDownloadPending(String downloadGuid, String fileName, boolean isOffTheRecord,
            boolean canDownloadWhileMetered, boolean isOfflinePage) {
        updateActiveDownloadNotification(downloadGuid, fileName,
                DownloadItem.INVALID_DOWNLOAD_PERCENTAGE, 0, 0, 0, isOffTheRecord,
                canDownloadWhileMetered, isOfflinePage, true);
    }

    /**
     * Helper method to update the notification for an active download, the download is either in
     * progress or pending.
     * @param downloadGuid GUID of the download.
     * @param fileName File name of the download.
     * @param percentage Percentage completed. Value should be between 0 to 100 if
     *        the percentage can be determined, or -1 if it is unknown.
     * @param bytesReceived Total number of bytes received.
     * @param timeRemainingInMillis Remaining download time in milliseconds.
     * @param startTime Time when download started.
     * @param isOffTheRecord Whether the download is off the record.
     * @param canDownloadWhileMetered Whether the download can happen in metered network.
     * @param isOfflinePage Whether the download is for offline page.
     * @param isDownloadPending Whether the download is pending.
     */
    private void updateActiveDownloadNotification(String downloadGuid, String fileName,
            int percentage, long bytesReceived, long timeRemainingInMillis, long startTime,
            boolean isOffTheRecord, boolean canDownloadWhileMetered, boolean isOfflinePage,
            boolean isDownloadPending) {
        if (mStopPostingProgressNotifications) return;

        boolean indeterminate =
                (percentage == DownloadItem.INVALID_DOWNLOAD_PERCENTAGE) || isDownloadPending;
        String contentText = null;
        if (isDownloadPending) {
            contentText = mContext.getResources().getString(R.string.download_notification_pending);
        } else if (indeterminate) {
            contentText = DownloadUtils.getStringForBytes(
                    mContext, DownloadUtils.BYTES_DOWNLOADED_STRINGS, bytesReceived);
        } else {
            contentText = formatRemainingTime(mContext, timeRemainingInMillis);
        }
        int resId = isDownloadPending ? R.drawable.ic_download_pending
                : android.R.drawable.stat_sys_download;
        NotificationCompat.Builder builder = buildNotification(resId, fileName, contentText);
        builder.setOngoing(true);
        builder.setPriority(Notification.PRIORITY_HIGH);

        // Avoid animations while the download isn't progressing.
        if (!isDownloadPending) {
            builder.setProgress(100, percentage, indeterminate);
        }

        if (!indeterminate && !isOfflinePage) {
            String percentText = DownloadUtils.getPercentageString(percentage);
            if (Build.VERSION.CODENAME.equals("N")
                    || Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
                builder.setSubText(percentText);
            } else {
                builder.setContentInfo(percentText);
            }
        }
        int notificationId = getNotificationId(downloadGuid);
        if (startTime > 0) builder.setWhen(startTime);

        // Clicking on an in-progress download sends the user to see all their downloads.
        Intent downloadHomeIntent = buildActionIntent(mContext,
                DownloadManager.ACTION_NOTIFICATION_CLICKED, null, isOffTheRecord, isOfflinePage);
        builder.setContentIntent(PendingIntent.getBroadcast(
                mContext, notificationId, downloadHomeIntent, PendingIntent.FLAG_UPDATE_CURRENT));
        builder.setAutoCancel(false);

        Intent pauseIntent = buildActionIntent(
                mContext, ACTION_DOWNLOAD_PAUSE, downloadGuid, isOffTheRecord, isOfflinePage);
        builder.addAction(R.drawable.ic_media_control_pause,
                mContext.getResources().getString(R.string.download_notification_pause_button),
                buildPendingIntent(pauseIntent, notificationId));

        Intent cancelIntent = buildActionIntent(
                mContext, ACTION_DOWNLOAD_CANCEL, downloadGuid, isOffTheRecord, isOfflinePage);
        builder.addAction(R.drawable.btn_close_white,
                mContext.getResources().getString(R.string.download_notification_cancel_button),
                buildPendingIntent(cancelIntent, notificationId));

        updateNotification(notificationId, builder.build());

        int itemType = isOfflinePage ? DownloadSharedPreferenceEntry.ITEM_TYPE_OFFLINE_PAGE
                                     : DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD;
        mDownloadSharedPreferenceHelper.addOrReplaceSharedPreferenceEntry(
                new DownloadSharedPreferenceEntry(notificationId, isOffTheRecord,
                        canDownloadWhileMetered, downloadGuid, fileName, itemType, true));
        if (!mDownloadsInProgress.contains(downloadGuid)) {
            mDownloadsInProgress.add(downloadGuid);
        }
    }

    /**
     * Cancel a download notification.
     * @param notificationId Notification ID of the download
     * @param downloadGuid GUID of the download.
     */
    @VisibleForTesting
    void cancelNotification(int notificationId, String downloadGuid) {
        mNotificationManager.cancel(NOTIFICATION_NAMESPACE, notificationId);
        mDownloadSharedPreferenceHelper.removeSharedPreferenceEntry(downloadGuid);
        mDownloadsInProgress.remove(downloadGuid);
    }

    /**
     * Called when a download is canceled.
     * @param downloadGuid GUID of the download.
     */
    @VisibleForTesting
    public void notifyDownloadCanceled(String downloadGuid) {
        DownloadSharedPreferenceEntry entry =
                mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(downloadGuid);
        if (entry == null) return;
        cancelNotification(entry.notificationId, downloadGuid);
    }

    /**
     * Change a download notification to paused state.
     * @param downloadGuid GUID of the download.
     * @param isResumable Whether download can be resumed.
     * @param isAutoResumable whether download is can be resumed automatically.
     */
    public void notifyDownloadPaused(String downloadGuid, boolean isResumable,
            boolean isAutoResumable) {
        DownloadSharedPreferenceEntry entry =
                mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(downloadGuid);
        if (entry == null) return;
        if (!isResumable) {
            notifyDownloadFailed(downloadGuid, entry.fileName);
            return;
        }
        // If download is already paused, do nothing.
        if (!entry.isAutoResumable) return;
        // If download is interrupted due to network disconnection, show download pending state.
        if (isAutoResumable) {
            notifyDownloadPending(entry.downloadGuid, entry.fileName, entry.isOffTheRecord,
                    entry.canDownloadWhileMetered, entry.isOfflinePage());
            mDownloadsInProgress.remove(downloadGuid);
            return;
        }

        String contentText = mContext.getResources().getString(
                R.string.download_notification_paused);
        NotificationCompat.Builder builder = buildNotification(
                R.drawable.ic_download_pause, entry.fileName, contentText);

        // Clicking on an in-progress download sends the user to see all their downloads.
        Intent downloadHomeIntent = buildActionIntent(
                mContext, DownloadManager.ACTION_NOTIFICATION_CLICKED, null, false, false);
        builder.setContentIntent(PendingIntent.getBroadcast(mContext, entry.notificationId,
                downloadHomeIntent, PendingIntent.FLAG_UPDATE_CURRENT));
        builder.setAutoCancel(false);

        Intent resumeIntent = buildActionIntent(mContext, ACTION_DOWNLOAD_RESUME,
                entry.downloadGuid, entry.isOffTheRecord, entry.isOfflinePage());
        builder.addAction(R.drawable.ic_get_app_white_24dp,
                mContext.getResources().getString(R.string.download_notification_resume_button),
                buildPendingIntent(resumeIntent, entry.notificationId));

        Intent cancelIntent = buildActionIntent(mContext, ACTION_DOWNLOAD_CANCEL,
                entry.downloadGuid, entry.isOffTheRecord, entry.isOfflinePage());
        builder.addAction(R.drawable.btn_close_white,
                mContext.getResources().getString(R.string.download_notification_cancel_button),
                buildPendingIntent(cancelIntent, entry.notificationId));

        Intent dismissIntent = new Intent(cancelIntent);
        dismissIntent.putExtra(EXTRA_NOTIFICATION_DISMISSED, true);
        builder.setDeleteIntent(buildPendingIntent(dismissIntent, entry.notificationId));

        updateNotification(entry.notificationId, builder.build());
        // Update the SharedPreference entry with the new isAutoResumable value.
        mDownloadSharedPreferenceHelper.addOrReplaceSharedPreferenceEntry(
                new DownloadSharedPreferenceEntry(
                        entry.notificationId, entry.isOffTheRecord,
                        entry.canDownloadWhileMetered, entry.downloadGuid, entry.fileName,
                        entry.itemType, isAutoResumable));
        mDownloadsInProgress.remove(downloadGuid);
    }

    /**
     * Add a download successful notification.
     * @param downloadGuid GUID of the download.
     * @param filePath Full path to the download.
     * @param fileName Filename of the download.
     * @param systemDownloadId Download ID assigned by system DownloadManager.
     * @param isOfflinePage Whether the download is for offline page.
     * @param isSupportedMimeType Whether the MIME type can be viewed inside browser.
     * @return ID of the successful download notification. Used for removing the notification when
     *         user click on the snackbar.
     */
    @VisibleForTesting
    public int notifyDownloadSuccessful(
            String downloadGuid, String filePath, String fileName, long systemDownloadId,
            boolean isOfflinePage, boolean isSupportedMimeType) {
        int notificationId = getNotificationId(downloadGuid);
        NotificationCompat.Builder builder = buildNotification(
                R.drawable.offline_pin, fileName,
                mContext.getResources().getString(R.string.download_notification_completed));
        ComponentName component = new ComponentName(
                mContext.getPackageName(), DownloadBroadcastReceiver.class.getName());
        Intent intent;
        if (isOfflinePage) {
            intent = buildActionIntent(mContext, ACTION_DOWNLOAD_OPEN, downloadGuid, false, true);
        } else {
            intent = new Intent(DownloadManager.ACTION_NOTIFICATION_CLICKED);
            long[] idArray = {systemDownloadId};
            intent.putExtra(DownloadManager.EXTRA_NOTIFICATION_CLICK_DOWNLOAD_IDS, idArray);
            intent.putExtra(EXTRA_DOWNLOAD_FILE_PATH, filePath);
            intent.putExtra(EXTRA_IS_SUPPORTED_MIME_TYPE, isSupportedMimeType);
        }
        intent.setComponent(component);
        builder.setContentIntent(PendingIntent.getBroadcast(
                mContext, notificationId, intent, PendingIntent.FLAG_UPDATE_CURRENT));
        if (mDownloadSuccessLargeIcon == null) {
            Bitmap bitmap = BitmapFactory.decodeResource(
                    mContext.getResources(), R.drawable.offline_pin);
            mDownloadSuccessLargeIcon = getLargeNotificationIcon(bitmap);
        }
        builder.setLargeIcon(mDownloadSuccessLargeIcon);
        updateNotification(notificationId, builder.build());
        mDownloadSharedPreferenceHelper.removeSharedPreferenceEntry(downloadGuid);
        mDownloadsInProgress.remove(downloadGuid);
        return notificationId;
    }

    /**
     * Add a download failed notification.
     * @param downloadGuid GUID of the download.
     * @param fileName GUID of the download.
     */
    @VisibleForTesting
    public void notifyDownloadFailed(String downloadGuid, String fileName) {
        // If the download is not in history db, fileName could be empty. Get it from
        // SharedPreferences.
        if (TextUtils.isEmpty(fileName)) {
            DownloadSharedPreferenceEntry entry =
                    mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(downloadGuid);
            if (entry == null) return;
            fileName = entry.fileName;
        }

        int notificationId = getNotificationId(downloadGuid);
        NotificationCompat.Builder builder = buildNotification(
                android.R.drawable.stat_sys_download_done, fileName,
                mContext.getResources().getString(R.string.download_notification_failed));
        updateNotification(notificationId, builder.build());
        mDownloadSharedPreferenceHelper.removeSharedPreferenceEntry(downloadGuid);
        mDownloadsInProgress.remove(downloadGuid);
    }

    /**
     * Called to pause all the download notifications.
     */
    @VisibleForTesting
    void pauseAllDownloads() {
        List<DownloadSharedPreferenceEntry> entries = mDownloadSharedPreferenceHelper.getEntries();
        for (int i = entries.size() - 1; i >= 0; --i) {
            DownloadSharedPreferenceEntry entry = entries.get(i);
            notifyDownloadPaused(entry.downloadGuid, !entry.isOffTheRecord, true);
        }
    }

    /**
     * Cancels all off the record download notifications.
     */
    void cancelOffTheRecordNotifications() {
        List<DownloadSharedPreferenceEntry> entries = mDownloadSharedPreferenceHelper.getEntries();
        for (int i = entries.size() - 1; i >= 0; --i) {
            DownloadSharedPreferenceEntry entry = entries.get(i);
            if (entry.isOffTheRecord) {
                notifyDownloadCanceled(entry.downloadGuid);
            }
        }
    }

    /**
     * Helper method to build a PendingIntent from the provided intent.
     * @param intent Intent to broadcast.
     * @param notificationId ID of the notification.
     */
    private PendingIntent buildPendingIntent(Intent intent, int notificationId) {
        return PendingIntent.getBroadcast(
                mContext, notificationId, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    /**
     * Helper method to build an download action Intent from the provided information.
     * @param context {@link Context} to pull resources from.
     * @param action Download action to perform.
     * @param downloadGuid GUID of the download.
     * @param isOffTheRecord Whether the download is incognito.
     * @param isOfflinePage Whether the download represents an Offline Page.
     */
    static Intent buildActionIntent(Context context, String action, String downloadGuid,
            boolean isOffTheRecord, boolean isOfflinePage) {
        ComponentName component = new ComponentName(
                context.getPackageName(), DownloadBroadcastReceiver.class.getName());
        Intent intent = new Intent(action);
        intent.setComponent(component);
        intent.putExtra(EXTRA_DOWNLOAD_GUID, downloadGuid);
        intent.putExtra(EXTRA_IS_OFF_THE_RECORD, isOffTheRecord);
        intent.putExtra(EXTRA_IS_OFFLINE_PAGE, isOfflinePage);
        return intent;
    }

    /**
     * Builds a notification to be displayed.
     * @param iconId Id of the notification icon.
     * @param title Title of the notification.
     * @param contentText Notification content text to be displayed.
     * @return notification builder that builds the notification to be displayed
     */
    private NotificationCompat.Builder buildNotification(
            int iconId, String title, String contentText) {
        NotificationCompat.Builder builder = new NotificationCompat.Builder(mContext)
                .setContentTitle(DownloadUtils.getAbbreviatedFileName(title, MAX_FILE_NAME_LENGTH))
                .setSmallIcon(iconId)
                .setLocalOnly(true)
                .setAutoCancel(true)
                .setContentText(contentText)
                .setGroup(NotificationConstants.GROUP_DOWNLOADS);
        return builder;
    }

    private Bitmap getLargeNotificationIcon(Bitmap bitmap) {
        Resources resources = mContext.getResources();
        int height = (int) resources.getDimension(android.R.dimen.notification_large_icon_height);
        int width = (int) resources.getDimension(android.R.dimen.notification_large_icon_width);
        final OvalShape circle = new OvalShape();
        circle.resize(width, height);
        final Paint paint = new Paint();
        paint.setColor(ApiCompatibilityUtils.getColor(resources, R.color.google_blue_grey_500));

        final Bitmap result = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(result);
        circle.draw(canvas, paint);
        float leftOffset = (width - bitmap.getWidth()) / 2f;
        float topOffset = (height - bitmap.getHeight()) / 2f;
        if (leftOffset >= 0 && topOffset >= 0) {
            canvas.drawBitmap(bitmap, leftOffset, topOffset, null);
        } else {
            // Scale down the icon into the notification icon dimensions
            canvas.drawBitmap(bitmap,
                    new Rect(0, 0, bitmap.getWidth(), bitmap.getHeight()),
                    new Rect(0, 0, width, height),
                    null);
        }
        return result;
    }

    /**
     * Retrieves DownloadSharedPreferenceEntry from a download action intent.
     * @param intent Intent that contains the download action.
     */
    private DownloadSharedPreferenceEntry getDownloadEntryFromIntent(Intent intent) {
        if (intent.getAction() == ACTION_DOWNLOAD_RESUME_ALL) return null;
        String guid = IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_GUID);
        return mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(guid);
    }

    /**
     * Helper method to launch the browser process and handle a download operation that is included
     * in the given intent.
     * @param intent Intent with the download operation.
     */
    private void handleDownloadOperation(final Intent intent) {
        // TODO(qinmin): Figure out how to properly handle this case.
        boolean isOfflinePage =
                IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OFFLINE_PAGE, false);
        final DownloadSharedPreferenceEntry entry = getDownloadEntryFromIntent(intent);
        if (entry == null
                && !(isOfflinePage && TextUtils.equals(intent.getAction(), ACTION_DOWNLOAD_OPEN))) {
            handleDownloadOperationForMissingNotification(intent);
            return;
        }

        if (intent.getAction() == ACTION_DOWNLOAD_PAUSE) {
            // If browser process already goes away, the download should have already paused. Do
            // nothing in that case.
            if (!DownloadManagerService.hasDownloadManagerService()) {
                notifyDownloadPaused(entry.downloadGuid, !entry.isOffTheRecord, false);
                return;
            }
        } else if (intent.getAction() == ACTION_DOWNLOAD_RESUME) {
            boolean metered = DownloadManagerService.isActiveNetworkMetered(mContext);
            if (!entry.canDownloadWhileMetered) {
                // If user manually resumes a download, update the network type if it
                // is not metered previously.
                entry.canDownloadWhileMetered = metered;
            }
            entry.isAutoResumable = true;
            // Update the SharedPreference entry.
            mDownloadSharedPreferenceHelper.addOrReplaceSharedPreferenceEntry(entry);
        } else if (intent.getAction() == ACTION_DOWNLOAD_RESUME_ALL
                && (mDownloadSharedPreferenceHelper.getEntries().isEmpty()
                        || DownloadManagerService.hasDownloadManagerService())) {
            return;
        } else if (intent.getAction() == ACTION_DOWNLOAD_OPEN) {
            // TODO(fgorski): Do we even need to do anything special here, before we launch Chrome?
        }

        BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public boolean shouldStartGpuProcess() {
                return false;
            }

            @Override
            public void finishNativeInitialization() {
                int itemType = entry != null ? entry.itemType
                        : (intent.getAction() == ACTION_DOWNLOAD_OPEN
                                ? DownloadSharedPreferenceEntry.ITEM_TYPE_OFFLINE_PAGE
                                : DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD);
                DownloadServiceDelegate downloadServiceDelegate =
                        intent.getAction() == ACTION_DOWNLOAD_OPEN ? null
                                : getServiceDelegate(itemType);
                switch (intent.getAction()) {
                    case ACTION_DOWNLOAD_CANCEL:
                        // TODO(qinmin): Alternatively, we can delete the downloaded content on
                        // SD card, and remove the download ID from the SharedPreferences so we
                        // don't need to restart the browser process. http://crbug.com/579643.
                        cancelNotification(entry.notificationId, entry.downloadGuid);
                        downloadServiceDelegate.cancelDownload(entry.downloadGuid,
                                entry.isOffTheRecord, IntentUtils.safeGetBooleanExtra(
                                        intent, EXTRA_NOTIFICATION_DISMISSED, false));
                        break;
                    case ACTION_DOWNLOAD_PAUSE:
                        downloadServiceDelegate.pauseDownload(entry.downloadGuid,
                                entry.isOffTheRecord);
                        break;
                    case ACTION_DOWNLOAD_RESUME:
                        notifyDownloadPending(entry.downloadGuid, entry.fileName,
                                entry.isOffTheRecord, entry.canDownloadWhileMetered,
                                entry.isOfflinePage());
                        downloadServiceDelegate.resumeDownload(entry.buildDownloadItem(), true);
                        break;
                    case ACTION_DOWNLOAD_RESUME_ALL:
                        assert entry == null;
                        resumeAllPendingDownloads();
                        break;
                    case ACTION_DOWNLOAD_OPEN:
                        OfflinePageDownloadBridge.openDownloadedPage(
                                IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_GUID));
                        break;
                    default:
                        Log.e(TAG, "Unrecognized intent action.", intent);
                        break;
                }
                if (intent.getAction() != ACTION_DOWNLOAD_OPEN) {
                    downloadServiceDelegate.destroyServiceDelegate();
                }
            }
        };
        try {
            ChromeBrowserInitializer.getInstance(mContext).handlePreNativeStartup(parts);
            ChromeBrowserInitializer.getInstance(mContext).handlePostNativeStartup(true, parts);
        } catch (ProcessInitException e) {
            Log.e(TAG, "Unable to load native library.", e);
            ChromeApplication.reportStartupErrorAndExit(e);
        }
    }

    /**
     * Handles operations for downloads that the DownloadNotificationService is unaware of.
     *
     * This can happen because the DownloadNotificationService learn about downloads later than
     * Download Home does, and may not yet have a DownloadSharedPreferenceEntry for the item.
     *
     * TODO(qinmin): Figure out how to fix the SharedPreferences so that it properly tracks entries.
     */
    private void handleDownloadOperationForMissingNotification(Intent intent) {
        // This function should only be called via Download Home, but catch this case to be safe.
        if (!DownloadManagerService.hasDownloadManagerService()) return;

        String action = intent.getAction();
        String downloadGuid = IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_GUID);
        boolean isOffTheRecord =
                IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OFF_THE_RECORD, false);
        int itemType = IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OFFLINE_PAGE, false)
                ? DownloadSharedPreferenceEntry.ITEM_TYPE_OFFLINE_PAGE
                : DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD;
        if (itemType != DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD) return;

        // Pass information directly to the DownloadManagerService.
        if (TextUtils.equals(action, ACTION_DOWNLOAD_CANCEL)) {
            getServiceDelegate(itemType).cancelDownload(downloadGuid, isOffTheRecord, false);
        } else if (TextUtils.equals(action, ACTION_DOWNLOAD_PAUSE)) {
            getServiceDelegate(itemType).pauseDownload(downloadGuid, isOffTheRecord);
        } else if (TextUtils.equals(action, ACTION_DOWNLOAD_RESUME)) {
            DownloadInfo info = new DownloadInfo.Builder()
                                        .setDownloadGuid(downloadGuid)
                                        .setIsOffTheRecord(isOffTheRecord)
                                        .build();
            getServiceDelegate(itemType).resumeDownload(new DownloadItem(false, info), true);
        }
    }

    /**
     * Gets appropriate download delegate that can handle interactions with download item referred
     * to by the entry.
     * @param forOfflinePage Whether the service should deal with offline pages or downloads.
     * @return delegate for interactions with the entry
     */
    DownloadServiceDelegate getServiceDelegate(int downloadItemType) {
        if (downloadItemType == DownloadSharedPreferenceEntry.ITEM_TYPE_OFFLINE_PAGE) {
            return OfflinePageDownloadBridge.getDownloadServiceDelegate();
        }
        if (downloadItemType != DownloadSharedPreferenceEntry.ITEM_TYPE_DOWNLOAD) {
            Log.e(TAG, "Unrecognized intent type.", downloadItemType);
        }
        return DownloadManagerService.getDownloadManagerService(getApplicationContext());
    }

    /**
     * Update the notification with id.
     * @param id Id of the notification that has to be updated.
     * @param notification the notification object that needs to be updated.
     */
    @VisibleForTesting
    void updateNotification(int id, Notification notification) {
        mNotificationManager.notify(NOTIFICATION_NAMESPACE, id, notification);
    }

    /**
     * Checks if an intent requires operations on a download.
     * @param intent An intent to validate.
     * @return true if the intent requires actions, or false otherwise.
     */
    static boolean isDownloadOperationIntent(Intent intent) {
        if (intent == null) return false;
        if (ACTION_DOWNLOAD_RESUME_ALL.equals(intent.getAction())) return true;
        if (!ACTION_DOWNLOAD_CANCEL.equals(intent.getAction())
                && !ACTION_DOWNLOAD_RESUME.equals(intent.getAction())
                && !ACTION_DOWNLOAD_PAUSE.equals(intent.getAction())
                && !ACTION_DOWNLOAD_OPEN.equals(intent.getAction())) {
            return false;
        }
        if (!intent.hasExtra(EXTRA_DOWNLOAD_GUID)) return false;
        final String guid = IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_GUID);
        if (!DownloadSharedPreferenceEntry.isValidGUID(guid)) return false;
        return true;
    }

    /**
     * Resumes all pending downloads from SharedPreferences. If a download is
     * already in progress, do nothing.
     */
    public void resumeAllPendingDownloads() {
        boolean isNetworkMetered = DownloadManagerService.isActiveNetworkMetered(mContext);
        if (!DownloadManagerService.hasDownloadManagerService()) return;
        List<DownloadSharedPreferenceEntry> entries = mDownloadSharedPreferenceHelper.getEntries();
        for (int i = 0; i < entries.size(); ++i) {
            DownloadSharedPreferenceEntry entry = entries.get(i);
            if (!entry.isAutoResumable) continue;
            if (mDownloadsInProgress.contains(entry.downloadGuid)) continue;
            if (!entry.canDownloadWhileMetered && isNetworkMetered) continue;
            notifyDownloadPending(entry.downloadGuid, entry.fileName, false,
                    entry.canDownloadWhileMetered, entry.isOfflinePage());
            DownloadServiceDelegate downloadServiceDelegate = getServiceDelegate(entry.itemType);
            downloadServiceDelegate.resumeDownload(entry.buildDownloadItem(), false);
            downloadServiceDelegate.destroyServiceDelegate();
        }
    }

    /**
     * Return the notification ID for the given download GUID.
     * @return notification ID to be used.
     */
    private int getNotificationId(String downloadGuid) {
        DownloadSharedPreferenceEntry entry =
                mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(downloadGuid);
        if (entry != null) return entry.notificationId;
        int notificationId = mNextNotificationId;
        mNextNotificationId = mNextNotificationId == Integer.MAX_VALUE
                ? STARTING_NOTIFICATION_ID : mNextNotificationId + 1;
        SharedPreferences.Editor editor = mSharedPrefs.edit();
        editor.putInt(KEY_NEXT_DOWNLOAD_NOTIFICATION_ID, mNextNotificationId);
        editor.apply();
        return notificationId;
    }

    /**
     * Format remaining time for the given millis, in the following format:
     * 5 hours; will include 1 unit, can go down to seconds precision.
     * This is similar to what android.java.text.Formatter.formatShortElapsedTime() does. Don't use
     * ui::TimeFormat::Simple() as it is very expensive.
     *
     * @param context the application context.
     * @param millis the remaining time in milli seconds.
     * @return the formatted remaining time.
     */
    public static String formatRemainingTime(Context context, long millis) {
        long secondsLong = millis / 1000;

        int days = 0;
        int hours = 0;
        int minutes = 0;
        if (secondsLong >= SECONDS_PER_DAY) {
            days = (int) (secondsLong / SECONDS_PER_DAY);
            secondsLong -= days * SECONDS_PER_DAY;
        }
        if (secondsLong >= SECONDS_PER_HOUR) {
            hours = (int) (secondsLong / SECONDS_PER_HOUR);
            secondsLong -= hours * SECONDS_PER_HOUR;
        }
        if (secondsLong >= SECONDS_PER_MINUTE) {
            minutes = (int) (secondsLong / SECONDS_PER_MINUTE);
            secondsLong -= minutes * SECONDS_PER_MINUTE;
        }
        int seconds = (int) secondsLong;

        if (days >= 2) {
            days += (hours + 12) / 24;
            return context.getString(R.string.remaining_duration_days, days);
        } else if (days > 0) {
            return context.getString(R.string.remaining_duration_one_day);
        } else if (hours >= 2) {
            hours += (minutes + 30) / 60;
            return context.getString(R.string.remaining_duration_hours, hours);
        } else if (hours > 0) {
            return context.getString(R.string.remaining_duration_one_hour);
        } else if (minutes >= 2) {
            minutes += (seconds + 30) / 60;
            return context.getString(R.string.remaining_duration_minutes, minutes);
        } else if (minutes > 0) {
            return context.getString(R.string.remaining_duration_one_minute);
        } else if (seconds == 1) {
            return context.getString(R.string.remaining_duration_one_second);
        } else {
            return context.getString(R.string.remaining_duration_seconds, seconds);
        }
    }
}
