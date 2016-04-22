// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Binder;
import android.os.IBinder;
import android.preference.PreferenceManager;
import android.support.v4.app.NotificationCompat;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.ui.base.LocalizationUtils;

import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * Service responsible for creating and updating download notifications even after
 * Chrome gets killed.
 */
public class DownloadNotificationService extends Service {
    static final String EXTRA_DOWNLOAD_NOTIFICATION_ID = "DownloadNotificationId";
    static final String EXTRA_DOWNLOAD_GUID = "DownloadGuid";
    static final String EXTRA_DOWNLOAD_FILE_NAME = "DownloadFileName";
    static final String ACTION_DOWNLOAD_CANCEL =
            "org.chromium.chrome.browser.download.DOWNLOAD_CANCEL";
    static final String ACTION_DOWNLOAD_PAUSE =
            "org.chromium.chrome.browser.download.DOWNLOAD_PAUSE";
    static final String ACTION_DOWNLOAD_RESUME =
            "org.chromium.chrome.browser.download.DOWNLOAD_RESUME";
    static final int INVALID_DOWNLOAD_PERCENTAGE = -1;
    @VisibleForTesting
    static final String PENDING_DOWNLOAD_NOTIFICATIONS = "PendingDownloadNotifications";
    private static final String NOTIFICATION_NAMESPACE = "DownloadNotificationService";
    private static final String TAG = "DownloadNotification";
    private final IBinder mBinder = new LocalBinder();
    private final List<DownloadSharedPreferenceEntry> mDownloadSharedPreferenceEntries =
            new ArrayList<DownloadSharedPreferenceEntry>();
    private NotificationManager mNotificationManager;
    private SharedPreferences mSharedPrefs;
    private Context mContext;

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
        // This funcion is called when Chrome is swiped away from the recent apps
        // drawer. So it doesn't catch all scenarios that chrome can get killed.
        // This will only help Android 4.4.2.
        pauseAllDownloads();
        stopSelf();
    }

    @Override
    public void onCreate() {
        mContext = getApplicationContext();
        mNotificationManager = (NotificationManager) mContext.getSystemService(
                Context.NOTIFICATION_SERVICE);
        mSharedPrefs = PreferenceManager.getDefaultSharedPreferences(mContext);
        parseDownloadSharedPrefs();
        // Because this service is a started service and returns START_STICKY in
        // onStartCommand(), it will be restarted as soon as resources are available
        // after it is killed. As a result, onCreate() may be called after Chrome
        // gets killed and before user restarts chrome. In that case,
        // DownloadManagerService.hasDownloadManagerService() will return false as
        // there are no calls to initialize DownloadManagerService. Pause all the
        // download notifications as download will not progress without chrome.
        if (!DownloadManagerService.hasDownloadManagerService()) {
            pauseAllDownloads();
            stopSelf();
        }
    }

    @Override
    public int onStartCommand(final Intent intent, int flags, int startId) {
        if (isDownloadOperationIntent(intent)) {
            handleDownloadOperation(intent);
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
     * Add a in-progress download notification.
     * @param notificationId Notification ID of the download.
     * @param downloadGuid GUID of the download.
     * @param fileName File name of the download.
     * @param percentage Percentage completed. Value should be between 0 to 100 if
     *        the percentage can be determined, or -1 if it is unknown.
     * @param timeRemainingInMillis Remaining download time in milliseconds.
     * @param startTime Time when download started.
     * @param isResumable Whether the download can be resumed.
     * @param canDownloadWhileMetered Whether the download can happen in metered network.
     */
    public void notifyDownloadProgress(int notificationId, String downloadGuid, String fileName,
            int percentage, long timeRemainingInMillis, long startTime, boolean isResumable,
            boolean canDownloadWhileMetered) {
        boolean indeterminate = percentage == INVALID_DOWNLOAD_PERCENTAGE;
        NotificationCompat.Builder builder = buildNotification(
                android.R.drawable.stat_sys_download, fileName, null);
        builder.setOngoing(true).setProgress(100, percentage, indeterminate);
        builder.setPriority(Notification.PRIORITY_HIGH);
        if (!indeterminate) {
            NumberFormat formatter = NumberFormat.getPercentInstance(Locale.getDefault());
            String percentText = formatter.format(percentage / 100.0);
            String duration = getDurationString(timeRemainingInMillis);
            builder.setContentText(duration).setContentInfo(percentText);
            addOrReplaceSharedPreferenceEntry(new DownloadSharedPreferenceEntry(
                    notificationId, isResumable, canDownloadWhileMetered, downloadGuid, fileName));
        }
        if (startTime > 0) builder.setWhen(startTime);
        builder.addAction(R.drawable.bookmark_cancel_active,
                mContext.getResources().getString(R.string.download_notification_cancel_button),
                buildPendingIntent(ACTION_DOWNLOAD_CANCEL, notificationId, downloadGuid, fileName));
        if (isResumable) {
            builder.addAction(R.drawable.ic_vidcontrol_pause,
                    mContext.getResources().getString(R.string.download_notification_pause_button),
                    buildPendingIntent(ACTION_DOWNLOAD_PAUSE, notificationId, downloadGuid,
                            fileName));
        }
        updateNotification(notificationId, builder.build());
    }

    /**
     * Converts milliseconds to time remaining format.
     * @param timeRemainingInMillis Remaining download time in milliseconds.
     * @return formatted remaining time string for display.
     */
    @VisibleForTesting
    String getDurationString(long timeRemainingInMillis) {
        return LocalizationUtils.getDurationString(timeRemainingInMillis);
    }

    /**
     * Cancel a download notification.
     * @param notificationId Notification ID of the download.
     * @param downloadGuid GUID of the download.
     */
    public void cancelNotification(int notificationId, String downloadGuid) {
        mNotificationManager.cancel(NOTIFICATION_NAMESPACE, notificationId);
        removeSharedPreferenceEntry(downloadGuid);
    }

    /**
     * Change a download notification to paused state.
     * @param downloadGuid GUID of the download.
     * @param isAutoResumable whether download is can be resumed automatically.
     */
    public void notifyDownloadPaused(String downloadGuid, boolean isAutoResumable) {
        DownloadSharedPreferenceEntry entry = getDownloadSharedPreferenceEntry(downloadGuid);
        if (entry == null) return;
        NotificationCompat.Builder builder = buildNotification(
                android.R.drawable.ic_media_pause, entry.fileName,
                mContext.getResources().getString(R.string.download_notification_paused));
        PendingIntent cancelIntent = buildPendingIntent(ACTION_DOWNLOAD_CANCEL,
                entry.notificationId, entry.downloadGuid, entry.fileName);
        builder.setDeleteIntent(cancelIntent);
        builder.addAction(R.drawable.bookmark_cancel_active,
                mContext.getResources().getString(R.string.download_notification_cancel_button),
                cancelIntent);
        if (entry.isResumable) {
            builder.addAction(R.drawable.resume_download,
                    mContext.getResources().getString(
                            R.string.download_notification_resume_button),
                    buildPendingIntent(ACTION_DOWNLOAD_RESUME, entry.notificationId,
                            entry.downloadGuid, entry.fileName));
        }
        updateNotification(entry.notificationId, builder.build());
        // If download is not auto resumable, there is no need to keep it in SharedPreferences.
        if (!entry.isResumable || !isAutoResumable) {
            removeSharedPreferenceEntry(downloadGuid);
        }
    }

    /**
     * Add a download successful notification.
     * @param downloadGuid GUID of the download.
     * @param intent Intent to launch when clicking the notification.
     */
    public void notifyDownloadSuccessful(String downloadGuid, Intent intent) {
        DownloadSharedPreferenceEntry entry = getDownloadSharedPreferenceEntry(downloadGuid);
        if (entry == null) return;
        NotificationCompat.Builder builder = buildNotification(
                android.R.drawable.stat_sys_download_done, entry.fileName,
                mContext.getResources().getString(R.string.download_notification_completed));
        if (intent != null) {
            builder.setContentIntent(PendingIntent.getActivity(
                    mContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT));
        }
        updateNotification(entry.notificationId, builder.build());
        removeSharedPreferenceEntry(downloadGuid);
    }

    /**
     * Add a download failed notification.
     * @param downloadGuid GUID of the download.
     */
    public void notifyDownloadFailed(String downloadGuid) {
        DownloadSharedPreferenceEntry entry = getDownloadSharedPreferenceEntry(downloadGuid);
        if (entry == null) return;
        NotificationCompat.Builder builder = buildNotification(
                android.R.drawable.stat_sys_download_done, entry.fileName,
                mContext.getResources().getString(R.string.download_notification_failed));
        updateNotification(entry.notificationId, builder.build());
        removeSharedPreferenceEntry(downloadGuid);
    }

    /**
     * Called to pause all the download notifications.
     */
    @VisibleForTesting
    void pauseAllDownloads() {
        for (int i = 0; i < mDownloadSharedPreferenceEntries.size(); ++i) {
            DownloadSharedPreferenceEntry entry = mDownloadSharedPreferenceEntries.get(i);
            notifyDownloadPaused(entry.downloadGuid, true);
        }
    }

    private PendingIntent buildPendingIntent(
            String action, int notificationId, String downloadGuid, String fileName) {
        ComponentName component = new ComponentName(
                mContext.getPackageName(), DownloadBroadcastReceiver.class.getName());
        Intent intent = new Intent(action);
        intent.setComponent(component);
        intent.putExtra(EXTRA_DOWNLOAD_NOTIFICATION_ID, notificationId);
        intent.putExtra(EXTRA_DOWNLOAD_GUID, downloadGuid);
        intent.putExtra(EXTRA_DOWNLOAD_FILE_NAME, fileName);
        return PendingIntent.getBroadcast(
                mContext, notificationId, intent, PendingIntent.FLAG_UPDATE_CURRENT);
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
                .setContentTitle(title)
                .setSmallIcon(iconId)
                .setLocalOnly(true)
                .setAutoCancel(true)
                .setContentText(contentText);
        return builder;
    }

    /**
     * Helper method to launch the browser process and handle a download operation that is included
     * in the given intent.
     * @param intent Intent with the download operation.
     */
    private void handleDownloadOperation(final Intent intent) {
        final int notificationId = IntentUtils.safeGetIntExtra(
                intent, DownloadNotificationService.EXTRA_DOWNLOAD_NOTIFICATION_ID, -1);
        final String guid = IntentUtils.safeGetStringExtra(
                intent, DownloadNotificationService.EXTRA_DOWNLOAD_GUID);
        final String fileName = IntentUtils.safeGetStringExtra(
                intent, DownloadNotificationService.EXTRA_DOWNLOAD_FILE_NAME);
        DownloadSharedPreferenceEntry entry = null;
        if (intent.getAction() == ACTION_DOWNLOAD_PAUSE) {
            notifyDownloadPaused(guid, false);
            // If browser process already goes away, the download should have already paused. Do
            // nothing in that case.
            if (!DownloadManagerService.hasDownloadManagerService()) return;
        } else if (intent.getAction() == ACTION_DOWNLOAD_RESUME) {
            entry = getDownloadSharedPreferenceEntry(guid);
            boolean metered = DownloadManagerService.isActiveNetworkMetered(mContext);
            if (entry == null) {
                entry = new DownloadSharedPreferenceEntry(
                        notificationId, true, metered, guid, fileName);
            } else if (!entry.canDownloadWhileMetered) {
                // If user manually resumes a download, update the network type if it
                // is not metered previously.
                entry.canDownloadWhileMetered = metered;
            }
            // Update the SharedPreference entry.
            addOrReplaceSharedPreferenceEntry(entry);
        }
        final DownloadItem item = entry == null ? null : entry.buildDownloadItem();
        final boolean canDownloadWhileMetered =
                entry == null ? false : entry.canDownloadWhileMetered;
        BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                DownloadManagerService service =
                        DownloadManagerService.getDownloadManagerService(getApplicationContext());
                switch (intent.getAction()) {
                    case ACTION_DOWNLOAD_CANCEL:
                        // TODO(qinmin): Alternatively, we can delete the downloaded content on
                        // SD card, and remove the download ID from the SharedPreferences so we
                        // don't need to restart the browser process. http://crbug.com/579643.
                        cancelNotification(notificationId, guid);
                        service.cancelDownload(guid);
                        break;
                    case ACTION_DOWNLOAD_PAUSE:
                        service.pauseDownload(guid);
                        break;
                    case ACTION_DOWNLOAD_RESUME:
                        assert item != null;
                        notifyDownloadProgress(notificationId, guid, fileName,
                                INVALID_DOWNLOAD_PERCENTAGE, 0, 0, true,
                                canDownloadWhileMetered);
                        service.resumeDownload(item, true);
                        break;
                    default:
                        Log.e(TAG, "Unrecognized intent action.", intent);
                        break;
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
        if (!ACTION_DOWNLOAD_CANCEL.equals(intent.getAction())
                && !ACTION_DOWNLOAD_RESUME.equals(intent.getAction())
                && !ACTION_DOWNLOAD_PAUSE.equals(intent.getAction())) {
            return false;
        }
        if (!intent.hasExtra(EXTRA_DOWNLOAD_NOTIFICATION_ID)
                || !intent.hasExtra(EXTRA_DOWNLOAD_FILE_NAME)
                || !intent.hasExtra(EXTRA_DOWNLOAD_GUID)) {
            return false;
        }
        final int notificationId =
                IntentUtils.safeGetIntExtra(intent, EXTRA_DOWNLOAD_NOTIFICATION_ID, -1);
        if (notificationId == -1) return false;
        final String fileName = IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_FILE_NAME);
        if (fileName == null) return false;
        final String guid = IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_GUID);
        if (!DownloadSharedPreferenceEntry.isValidGUID(guid)) return false;
        return true;
    }

    /**
     * Adds a DownloadSharedPreferenceEntry to SharedPrefs. If an entry with the GUID already exists
     * in SharedPrefs, update it if it has changed.
     * @param DownloadSharedPreferenceEntry A DownloadSharedPreferenceEntry to be added.
     */
    private void addOrReplaceSharedPreferenceEntry(DownloadSharedPreferenceEntry pendingEntry) {
        Iterator<DownloadSharedPreferenceEntry> iterator =
                mDownloadSharedPreferenceEntries.iterator();
        while (iterator.hasNext()) {
            DownloadSharedPreferenceEntry entry = iterator.next();
            if (entry.downloadGuid.equals(pendingEntry.downloadGuid)) {
                if (entry.equals(pendingEntry)) return;
                iterator.remove();
                break;
            }
        }
        mDownloadSharedPreferenceEntries.add(pendingEntry);
        storeDownloadSharedPreferenceEntries();
    }

    /**
     * Removes a DownloadSharedPreferenceEntry from SharedPrefs given by the GUID.
     * @param guid Download GUID to be removed.
     */
    private void removeSharedPreferenceEntry(String guid) {
        Iterator<DownloadSharedPreferenceEntry> iterator =
                mDownloadSharedPreferenceEntries.iterator();
        boolean found = false;
        while (iterator.hasNext()) {
            DownloadSharedPreferenceEntry entry = iterator.next();
            if (entry.downloadGuid.equals(guid)) {
                iterator.remove();
                found = true;
                break;
            }
        }
        if (found) {
            storeDownloadSharedPreferenceEntries();
        }
    }

    /**
     * Resumes all pending downloads from |mDownloadSharedPreferenceEntries|.
     */
    public void resumeAllPendingDownloads() {
        boolean isNetworkMetered = DownloadManagerService.isActiveNetworkMetered(mContext);
        if (!DownloadManagerService.hasDownloadManagerService()) return;
        DownloadManagerService service =
                DownloadManagerService.getDownloadManagerService(getApplicationContext());
        for (int i = 0; i < mDownloadSharedPreferenceEntries.size(); ++i) {
            DownloadSharedPreferenceEntry entry = mDownloadSharedPreferenceEntries.get(i);
            if (!entry.canDownloadWhileMetered && isNetworkMetered) continue;
            notifyDownloadProgress(entry.notificationId, entry.downloadGuid, entry.fileName,
                    INVALID_DOWNLOAD_PERCENTAGE, 0, 0, true, entry.canDownloadWhileMetered);
            service.resumeDownload(entry.buildDownloadItem(), false);
        }
    }

    /**
     * Parse the DownloadSharedPreferenceEntry from the shared preference and return a list of them.
     * @return a list of parsed DownloadSharedPreferenceEntry.
     */
    void parseDownloadSharedPrefs() {
        if (!mSharedPrefs.contains(PENDING_DOWNLOAD_NOTIFICATIONS)) return;
        Set<String> entries = DownloadManagerService.getStoredDownloadInfo(
                mSharedPrefs, PENDING_DOWNLOAD_NOTIFICATIONS);
        for (String entryString : entries) {
            DownloadSharedPreferenceEntry entry =
                    DownloadSharedPreferenceEntry.parseFromString(entryString);
            if (entry.notificationId > 0) {
                mDownloadSharedPreferenceEntries.add(
                        DownloadSharedPreferenceEntry.parseFromString(entryString));
            }
        }
    }

    /**
     * Gets a DownloadSharedPreferenceEntry that has the given GUID.
     * @param guid GUID to query.
     * @return a DownloadSharedPreferenceEntry that has the specified GUID.
     */
    private DownloadSharedPreferenceEntry getDownloadSharedPreferenceEntry(String guid) {
        for (int i = 0; i < mDownloadSharedPreferenceEntries.size(); ++i) {
            if (mDownloadSharedPreferenceEntries.get(i).downloadGuid.equals(guid)) {
                return mDownloadSharedPreferenceEntries.get(i);
            }
        }
        return null;
    }

    /**
     * Helper method to store all the SharedPreferences entries.
     */
    private void storeDownloadSharedPreferenceEntries() {
        Set<String> entries = new HashSet<String>();
        for (int i = 0; i < mDownloadSharedPreferenceEntries.size(); ++i) {
            entries.add(mDownloadSharedPreferenceEntries.get(i).getSharedPreferenceString());
        }
        DownloadManagerService.storeDownloadInfo(
                mSharedPrefs, PENDING_DOWNLOAD_NOTIFICATIONS, entries);
    }
}
