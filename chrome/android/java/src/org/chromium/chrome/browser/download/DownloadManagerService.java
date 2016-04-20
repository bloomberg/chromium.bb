// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.DownloadManager;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.ConnectivityManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Environment;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.util.LongSparseArray;
import android.util.Pair;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.content.browser.DownloadController;
import org.chromium.content.browser.DownloadInfo;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifierAutoDetect;
import org.chromium.net.RegistrationPolicyAlwaysRegister;
import org.chromium.ui.widget.Toast;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.Vector;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Chrome implementation of the {@link DownloadController.DownloadNotificationService} interface.
 * This class is responsible for keeping track of which downloads are in progress. It generates
 * updates for progress of downloads and handles cleaning up of interrupted progress notifications.
 * TODO(qinmin): move BroadcastReceiver inheritance into DownloadManagerDelegate, as it handles all
 * Android DownloadManager interactions. And DownloadManagerService should not know download Id
 * issued by Android DownloadManager.
 */
public class DownloadManagerService extends BroadcastReceiver implements
        DownloadController.DownloadNotificationService,
        NetworkChangeNotifierAutoDetect.Observer,
        DownloadManagerDelegate.DownloadQueryCallback {
    private static final String TAG = "DownloadService";
    // Deprecated shared preference entry. Keep this for a while so that it will be removed when
    // user updates Chrome.
    // TODO(qinmin): remove this when most users have upgraded to M49 or above.
    private static final String DEPRECATED_DOWNLOAD_NOTIFICATION_IDS = "DownloadNotificationIds";
    private static final String DOWNLOAD_DIRECTORY = "Download";
    protected static final String PENDING_OMA_DOWNLOADS = "PendingOMADownloads";
    private static final String UNKNOWN_MIME_TYPE = "application/unknown";
    private static final String DOWNLOAD_UMA_ENTRY = "DownloadUmaEntry";
    private static final long UPDATE_DELAY_MILLIS = 1000;
    // Wait 10 seconds to resume all downloads, so that we won't impact tab loading.
    private static final long RESUME_DELAY_MILLIS = 10000;
    private static final int UNKNOWN_DOWNLOAD_STATUS = -1;

    // Values for the histogram MobileDownloadResumptionCount.
    private static final int UMA_DOWNLOAD_RESUMPTION_MANUAL_PAUSE = 0;
    private static final int UMA_DOWNLOAD_RESUMPTION_BROWSER_KILLED = 1;
    private static final int UMA_DOWNLOAD_RESUMPTION_CLICKED = 2;
    private static final int UMA_DOWNLOAD_RESUMPTION_FAILED = 3;
    private static final int UMA_DOWNLOAD_RESUMPTION_AUTO_STARTED = 4;
    private static final int UMA_DOWNLOAD_RESUMPTION_COUNT = 5;

    // Download status.
    static final int DOWNLOAD_STATUS_IN_PROGRESS = 0;
    static final int DOWNLOAD_STATUS_COMPLETE = 1;
    static final int DOWNLOAD_STATUS_FAILED = 2;
    static final int DOWNLOAD_STATUS_CANCELLED = 3;
    static final int DOWNLOAD_STATUS_INTERRUPTED = 4;

    // Set will be more expensive to initialize, so use an ArrayList here.
    private static final List<String> MIME_TYPES_TO_OPEN = new ArrayList<String>(Arrays.asList(
            OMADownloadHandler.OMA_DOWNLOAD_DESCRIPTOR_MIME,
            "application/pdf",
            "application/x-x509-ca-cert",
            "application/x-x509-user-cert",
            "application/x-x509-server-cert",
            "application/x-pkcs12",
            "application/application/x-pem-file",
            "application/pkix-cert",
            "application/x-wifi-config"));

    private static DownloadManagerService sDownloadManagerService;
    private static boolean sIsNetworkListenerDisabled;
    private static boolean sIsNetworkMetered;

    private final SharedPreferences mSharedPrefs;
    private final ConcurrentHashMap<String, DownloadProgress> mDownloadProgressMap =
            new ConcurrentHashMap<String, DownloadProgress>(4, 0.75f, 2);

    private final DownloadNotifier mDownloadNotifier;
    // Delay between UI updates.
    private final long mUpdateDelayInMillis;

    // Flag to track if we need to post a task to update download notifications.
    private final AtomicBoolean mIsUIUpdateScheduled;
    private final Handler mHandler;
    private final Context mContext;

    private final LongSparseArray<DownloadItem> mSystemDownloadIdMap =
            new LongSparseArray<DownloadItem>();
    // Using vector for thread safety.
    @VisibleForTesting protected final Vector<String> mAutoResumableDownloadIds =
            new Vector<String>();
    private final List<DownloadUmaStatsEntry> mUmaEntries = new ArrayList<DownloadUmaStatsEntry>();
    private OMADownloadHandler mOMADownloadHandler;
    private DownloadSnackbarController mDownloadSnackbarController;
    private long mNativeDownloadManagerService;
    private DownloadManagerDelegate mDownloadManagerDelegate;
    private NetworkChangeNotifierAutoDetect mNetworkChangeNotifier;

    /**
     * Class representing progress of a download.
     */
    private static class DownloadProgress {
        final long mStartTimeInMillis;
        boolean mCanDownloadWhileMetered;
        volatile DownloadItem mDownloadItem;
        volatile int mDownloadStatus;

        DownloadProgress(long startTimeInMillis, boolean canDownloadWhileMetered,
                DownloadItem downloadItem, int downloadStatus) {
            mStartTimeInMillis = startTimeInMillis;
            mCanDownloadWhileMetered = canDownloadWhileMetered;
            mDownloadItem = downloadItem;
            mDownloadStatus = downloadStatus;
        }
    }

    /**
     * Class representing an OMA download entry to be stored in SharedPrefs.
     * TODO(qinmin): Move all OMA related class and functions to a separate class.
     */
    @VisibleForTesting
    protected static class OMAEntry {
        final long mDownloadId;
        final String mInstallNotifyURI;

        OMAEntry(long downloadId, String installNotifyURI) {
            mDownloadId = downloadId;
            mInstallNotifyURI = installNotifyURI;
        }

        /**
         * Parse OMA entry from the SharedPrefs String
         * TODO(qinmin): use a file instead of SharedPrefs to store the OMA entry.
         *
         * @param entry String contains the OMA information.
         * @return an OMAEntry object.
         */
        @VisibleForTesting
        static OMAEntry parseOMAEntry(String entry) {
            int index = entry.indexOf(",");
            long downloadId = Long.parseLong(entry.substring(0, index));
            return new OMAEntry(downloadId, entry.substring(index + 1));
        }

        /**
         * Generates a string for an OMA entry to be inserted into the SharedPrefs.
         * TODO(qinmin): use a file instead of SharedPrefs to store the OMA entry.
         *
         * @return a String representing the download entry.
         */
        String generateSharedPrefsString() {
            return String.valueOf(mDownloadId) + "," + mInstallNotifyURI;
        }
    }

    /**
     * Creates DownloadManagerService.
     */
    @SuppressFBWarnings("LI_LAZY_INIT") // Findbugs doesn't see this is only UI thread.
    public static DownloadManagerService getDownloadManagerService(final Context context) {
        ThreadUtils.assertOnUiThread();
        assert context == context.getApplicationContext();
        if (sDownloadManagerService == null) {
            sDownloadManagerService = new DownloadManagerService(context,
                    new SystemDownloadNotifier(context),  new Handler(), UPDATE_DELAY_MILLIS);
        }
        return sDownloadManagerService;
    }

    public static boolean hasDownloadManagerService() {
        ThreadUtils.assertOnUiThread();
        return sDownloadManagerService != null;
    }

    /**
     * For tests only: sets the DownloadManagerService.
     * @param service An instance of DownloadManagerService.
     * @return Null or a currently set instance of DownloadManagerService.
     */
    @VisibleForTesting
    public static DownloadManagerService setDownloadManagerService(DownloadManagerService service) {
        ThreadUtils.assertOnUiThread();
        DownloadManagerService prev = sDownloadManagerService;
        sDownloadManagerService = service;
        return prev;
    }

    @VisibleForTesting
    protected DownloadManagerService(Context context,
            DownloadNotifier downloadNotifier,
            Handler handler,
            long updateDelayInMillis) {
        mContext = context;
        mSharedPrefs = PreferenceManager.getDefaultSharedPreferences(context
                .getApplicationContext());
        mDownloadNotifier = downloadNotifier;
        mUpdateDelayInMillis = updateDelayInMillis;
        mHandler = handler;
        mIsUIUpdateScheduled = new AtomicBoolean(false);
        mOMADownloadHandler = new OMADownloadHandler(context);
        mDownloadSnackbarController = new DownloadSnackbarController(context);
        mDownloadManagerDelegate = new DownloadManagerDelegate(mContext);
        if (mSharedPrefs.contains(DEPRECATED_DOWNLOAD_NOTIFICATION_IDS)) {
            mSharedPrefs.edit().remove(DEPRECATED_DOWNLOAD_NOTIFICATION_IDS).apply();
        }
        // Note that this technically leaks the native object, however, DownloadManagerService
        // is a singleton that lives forever and there's no clean shutdown of Chrome on Android.
        init();
        clearPendingOMADownloads();
    }

    @VisibleForTesting
    protected void init() {
        DownloadController.setDownloadNotificationService(this);
        // Post a delayed task to resume all pending downloads.
        mHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                mDownloadNotifier.resumePendingDownloads();
            }
        }, RESUME_DELAY_MILLIS);
        parseUMAStatsEntriesFromSharedPrefs();
        Iterator<DownloadUmaStatsEntry> iterator = mUmaEntries.iterator();
        boolean hasChanges = false;
        while (iterator.hasNext()) {
            DownloadUmaStatsEntry entry = iterator.next();
            if (entry.useDownloadManager) {
                mDownloadManagerDelegate.queryDownloadResult(
                        entry.buildDownloadItem(), false, this);
            } else if (!entry.isPaused) {
                entry.isPaused = true;
                entry.numInterruptions++;
                hasChanges = true;
            }
        }
        if (hasChanges) {
            storeUmaEntries();
        }
    }

    public void setDownloadManagerDelegate(DownloadManagerDelegate downloadManagerDelegate) {
        mDownloadManagerDelegate = downloadManagerDelegate;
    }

    @Override
    public void onDownloadCompleted(final DownloadInfo downloadInfo) {
        int status = DOWNLOAD_STATUS_COMPLETE;
        if (downloadInfo.getContentLength() == 0) {
            status = DOWNLOAD_STATUS_FAILED;
        }
        updateDownloadProgress(new DownloadItem(false, downloadInfo), status);
        scheduleUpdateIfNeeded();
    }

    @Override
    public void onDownloadUpdated(final DownloadInfo downloadInfo) {
        DownloadItem item = new DownloadItem(false, downloadInfo);
        updateDownloadProgress(item, DOWNLOAD_STATUS_IN_PROGRESS);
        // If user manually paused a download, this download is no longer auto resumable.
        if (downloadInfo.isPaused()) {
            removeAutoResumableDownload(item.getId());
        }
        scheduleUpdateIfNeeded();
    }

    @Override
    public void onDownloadCancelled(final DownloadInfo downloadInfo) {
        DownloadItem item = new DownloadItem(false, downloadInfo);
        updateDownloadProgress(new DownloadItem(false, downloadInfo), DOWNLOAD_STATUS_CANCELLED);
        removeAutoResumableDownload(item.getId());
        scheduleUpdateIfNeeded();
    }

    @Override
    public void onDownloadInterrupted(final DownloadInfo downloadInfo, boolean isAutoResumable) {
        int status = DOWNLOAD_STATUS_INTERRUPTED;
        DownloadItem item = new DownloadItem(false, downloadInfo);
        if (!downloadInfo.isResumable()) {
            status = DOWNLOAD_STATUS_FAILED;
        } else if (isAutoResumable) {
            addAutoResumableDownload(item.getId());
        }
        updateDownloadProgress(item, status);
        scheduleUpdateIfNeeded();
    }

    /**
     * Clear any pending OMA downloads by reading them from shared prefs.
     * TODO(qinmin): move this to a separate class.
     */
    public void clearPendingOMADownloads() {
        if (mSharedPrefs.contains(PENDING_OMA_DOWNLOADS)) {
            Set<String> omaDownloads = getStoredDownloadInfo(mSharedPrefs, PENDING_OMA_DOWNLOADS);
            for (String omaDownload : omaDownloads) {
                OMAEntry entry = OMAEntry.parseOMAEntry(omaDownload);
                clearPendingOMADownload(entry.mDownloadId, entry.mInstallNotifyURI);
            }
        }
    }

    /**
     * Async task to clear the pending OMA download from SharedPrefs and inform
     * the OMADownloadHandler about download status.
     * TODO(qinmin): move this to a separate file.
     */
    private class ClearPendingOMADownloadTask extends
            AsyncTask<Void, Void, Pair<Integer, Boolean>> {
        private final DownloadItem mDownloadItem;
        private final String mInstallNotifyURI;
        private DownloadInfo mDownloadInfo;
        private int mFailureReason;

        public ClearPendingOMADownloadTask(DownloadItem downloadItem, String installNotifyURI) {
            mDownloadItem = downloadItem;
            mInstallNotifyURI = installNotifyURI;
            mDownloadInfo = downloadItem.getDownloadInfo();
        }

        @Override
        public Pair<Integer, Boolean> doInBackground(Void...voids) {
            DownloadManager manager =
                    (DownloadManager) mContext.getSystemService(Context.DOWNLOAD_SERVICE);
            Cursor c = manager.query(new DownloadManager.Query().setFilterById(
                    mDownloadItem.getSystemDownloadId()));
            int statusIndex = c.getColumnIndex(DownloadManager.COLUMN_STATUS);
            int reasonIndex = c.getColumnIndex(DownloadManager.COLUMN_REASON);
            int titleIndex = c.getColumnIndex(DownloadManager.COLUMN_TITLE);
            int status = DownloadManager.STATUS_FAILED;
            Boolean canResolve = false;
            if (c.moveToNext()) {
                status = c.getInt(statusIndex);
                String title = c.getString(titleIndex);
                if (mDownloadInfo == null) {
                    // Chrome has been killed, reconstruct a DownloadInfo.
                    mDownloadInfo = new DownloadInfo.Builder()
                            .setFileName(title)
                            .setDescription(c.getString(
                                    c.getColumnIndex(DownloadManager.COLUMN_DESCRIPTION)))
                            .setMimeType(c.getString(
                                    c.getColumnIndex(DownloadManager.COLUMN_MEDIA_TYPE)))
                            .setContentLength(Long.parseLong(c.getString(
                                    c.getColumnIndex(DownloadManager.COLUMN_TOTAL_SIZE_BYTES))))
                            .build();
                }
                if (status == DownloadManager.STATUS_SUCCESSFUL) {
                    mDownloadInfo = DownloadInfo.Builder.fromDownloadInfo(mDownloadInfo)
                            .setFileName(title)
                            .build();
                    mDownloadItem.setDownloadInfo(mDownloadInfo);
                    canResolve = canResolveDownloadItem(mContext, mDownloadItem);
                } else if (status == DownloadManager.STATUS_FAILED) {
                    mFailureReason = c.getInt(reasonIndex);
                    manager.remove(mDownloadItem.getSystemDownloadId());
                }
            }
            c.close();
            return Pair.create(status, canResolve);
        }

        @Override
        protected void onPostExecute(Pair<Integer, Boolean> result) {
            long downloadId = mDownloadItem.getSystemDownloadId();
            if (result.first == DownloadManager.STATUS_SUCCESSFUL) {
                mOMADownloadHandler.onDownloadCompleted(
                        mDownloadInfo, downloadId, mInstallNotifyURI);
                removeOMADownloadFromSharedPrefs(downloadId);
                mDownloadSnackbarController.onDownloadSucceeded(
                        mDownloadInfo, downloadId, result.second);
            } else if (result.first == DownloadManager.STATUS_FAILED) {
                mOMADownloadHandler.onDownloadFailed(
                        mDownloadInfo, downloadId, mFailureReason, mInstallNotifyURI);
                removeOMADownloadFromSharedPrefs(downloadId);
                String fileName = mDownloadInfo.getFileName();
                onDownloadFailed(fileName, mFailureReason);
            }
        }
    }

    /**
     * Clear pending OMA downloads for a particular download ID.
     *
     * @param downloadId Download identifier from Android DownloadManager.
     * @param installNotifyURI URI to notify after installation.
     */
    private void clearPendingOMADownload(long downloadId, String installNotifyURI) {
        DownloadItem item = mSystemDownloadIdMap.get(downloadId);
        if (item == null) {
            item = new DownloadItem(true, null);
            item.setSystemDownloadId(downloadId);
        }
        ClearPendingOMADownloadTask task = new ClearPendingOMADownloadTask(item, installNotifyURI);
        task.execute();
    }

    /**
     * Broadcast that a download was successful.
     * @param downloadInfo info about the download.
     */
    protected void broadcastDownloadSuccessful(DownloadInfo downloadInfo) {}

    /**
     * Gets download information from SharedPreferences.
     * @param sharedPrefs The SharedPreferences object to parse.
     * @param type Type of the information to retrieve.
     * @return download information saved to the SharedPrefs for the given type.
     */
    @VisibleForTesting
    protected static Set<String> getStoredDownloadInfo(SharedPreferences sharedPrefs, String type) {
        return new HashSet<String>(sharedPrefs.getStringSet(type, new HashSet<String>()));
    }

    /**
     * Add OMA download info to SharedPrefs.
     * @param omaInfo OMA download information to save.
     */
    @VisibleForTesting
    protected void addOMADownloadToSharedPrefs(String omaInfo) {
        Set<String> omaDownloads = getStoredDownloadInfo(mSharedPrefs, PENDING_OMA_DOWNLOADS);
        omaDownloads.add(omaInfo);
        storeDownloadInfo(mSharedPrefs, PENDING_OMA_DOWNLOADS, omaDownloads);
    }

    /**
     * Remove OMA download info from SharedPrefs.
     * @param downloadId ID to be removed.
     */
    private void removeOMADownloadFromSharedPrefs(long downloadId) {
        Set<String> omaDownloads = getStoredDownloadInfo(mSharedPrefs, PENDING_OMA_DOWNLOADS);
        for (String omaDownload : omaDownloads) {
            OMAEntry entry = OMAEntry.parseOMAEntry(omaDownload);
            if (entry.mDownloadId == downloadId) {
                omaDownloads.remove(omaDownload);
                storeDownloadInfo(mSharedPrefs, PENDING_OMA_DOWNLOADS, omaDownloads);
                return;
            }
        }
    }

    /**
     * Check if a download ID is in OMA SharedPrefs.
     * @param downloadId Download identifier to check.
     * @param true if it is in the SharedPrefs, or false otherwise.
     */
    private boolean isDownloadIdInOMASharedPrefs(long downloadId) {
        Set<String> omaDownloads = getStoredDownloadInfo(mSharedPrefs, PENDING_OMA_DOWNLOADS);
        for (String omaDownload : omaDownloads) {
            OMAEntry entry = OMAEntry.parseOMAEntry(omaDownload);
            if (entry.mDownloadId == downloadId) {
                return true;
            }
        }
        return false;
    }

    /**
     * Stores download information to shared preferences. The information can be
     * either pending download IDs, or pending OMA downloads.
     *
     * @param sharedPrefs SharedPreferences to update.
     * @param type Type of the information.
     * @param downloadInfo Information to be saved.
     */
    static void storeDownloadInfo(
            SharedPreferences sharedPrefs, String type, Set<String> downloadInfo) {
        SharedPreferences.Editor editor = sharedPrefs.edit();
        if (downloadInfo.isEmpty()) {
            editor.remove(type);
        } else {
            editor.putStringSet(type, downloadInfo);
        }
        editor.apply();
    }

    /**
     * Updates notifications for all current downloads. Should not be called from UI thread.
     *
     * @return A map that maps all completed download to whether the download can be resolved.
     *         If a download fails, its system download ID is DownloadItem.INVALID_DOWNLOAD_ID. If
     *         a download is cancelled, return an empty map so that no action needs to be taken.
     */
    private Map<DownloadItem, Boolean> updateAllNotifications() {
        assert !ThreadUtils.runningOnUiThread();
        Map<DownloadItem, Boolean> downloadItems = new HashMap<DownloadItem, Boolean>();
        for (DownloadProgress progress : mDownloadProgressMap.values()) {
            if (progress != null) {
                DownloadItem item = progress.mDownloadItem;
                DownloadInfo info = item.getDownloadInfo();
                switch (progress.mDownloadStatus) {
                    case DOWNLOAD_STATUS_COMPLETE:
                        mDownloadProgressMap.remove(item.getId());
                        boolean success = addCompletedDownload(item);
                        if (success) {
                            boolean canResolve = isOMADownloadDescription(info)
                                    || canResolveDownloadItem(mContext, item);
                            downloadItems.put(item, canResolve);
                            mDownloadNotifier.notifyDownloadSuccessful(
                                    info, getLaunchIntentFromDownloadId(
                                            mContext, item.getSystemDownloadId()));
                            broadcastDownloadSuccessful(info);
                        } else {
                            downloadItems.put(item, false);
                            mDownloadNotifier.notifyDownloadFailed(info);
                        }
                        break;
                    case DOWNLOAD_STATUS_FAILED:
                        mDownloadProgressMap.remove(item.getId());
                        mDownloadNotifier.notifyDownloadFailed(info);
                        downloadItems.put(item, false);
                        Log.w(TAG, "Download failed: " + info.getFilePath());
                        break;
                    case DOWNLOAD_STATUS_IN_PROGRESS:
                        if (info.isPaused()) {
                            mDownloadProgressMap.remove(item.getId());
                            mDownloadNotifier.notifyDownloadPaused(info, false);
                            if (info.isResumable()) {
                                recordDownloadResumption(UMA_DOWNLOAD_RESUMPTION_MANUAL_PAUSE);
                            }
                        } else {
                            mDownloadNotifier.notifyDownloadProgress(info,
                                    progress.mStartTimeInMillis, progress.mCanDownloadWhileMetered);
                        }
                        break;
                    case DOWNLOAD_STATUS_CANCELLED:
                        mDownloadProgressMap.remove(item.getId());
                        mDownloadNotifier.cancelNotification(
                                item.getNotificationId(), item.getId());
                        break;
                    case DOWNLOAD_STATUS_INTERRUPTED:
                        // If the download can be auto resumed, keep it in the progress map so we
                        // can resume it once network becomes available.
                        boolean isAutoResumable =
                                mAutoResumableDownloadIds.contains(item.getId());
                        if (!isAutoResumable) {
                            mDownloadProgressMap.remove(item.getId());
                        }
                        mDownloadNotifier.notifyDownloadPaused(info, isAutoResumable);
                        break;
                    default:
                        assert false;
                        break;
                }
            }
        }
        return downloadItems;
    }

    /**
     * Adds a completed download into Android DownloadManager.
     *
     * @param downloadItem Information of the downloaded.
     * @return true if the download is added to the DownloadManager, or false otherwise.
     */
    protected boolean addCompletedDownload(DownloadItem downloadItem) {
        DownloadInfo downloadInfo = downloadItem.getDownloadInfo();
        String mimeType = downloadInfo.getMimeType();
        if (TextUtils.isEmpty(mimeType)) mimeType = UNKNOWN_MIME_TYPE;
        String description = downloadInfo.getDescription();
        if (TextUtils.isEmpty(description)) description = downloadInfo.getFileName();
        try {
            // Exceptions can be thrown when calling this, although it is not
            // documented on Android SDK page.
            long downloadId = mDownloadManagerDelegate.addCompletedDownload(
                    downloadInfo.getFileName(), description, mimeType, downloadInfo.getFilePath(),
                    downloadInfo.getContentLength(), downloadInfo.getOriginalUrl(),
                    downloadInfo.getReferer());
            downloadItem.setSystemDownloadId(downloadId);
            return true;
        } catch (RuntimeException e) {
            Log.w(TAG, "Failed to add the download item to DownloadManager: ", e);
            if (downloadInfo.getFilePath() != null) {
                File file = new File(downloadInfo.getFilePath());
                if (!file.delete()) {
                    Log.w(TAG, "Failed to remove the unsuccessful download");
                }
            }
        }
        return false;
    }

    /**
     * Handle auto opennable files after download completes.
     * TODO(qinmin): move this to DownloadManagerDelegate.
     *
     * @param download A download item.
     */
    private void handleAutoOpenAfterDownload(DownloadItem download) {
        if (isOMADownloadDescription(download.getDownloadInfo())) {
            mOMADownloadHandler.handleOMADownload(
                    download.getDownloadInfo(), download.getSystemDownloadId());
            return;
        }
        openDownloadedContent(download.getSystemDownloadId());
    }

    /**
     * Schedule an update if there is no update scheduled.
     */
    private void scheduleUpdateIfNeeded() {
        if (mIsUIUpdateScheduled.compareAndSet(false, true)) {
            Runnable updateTask = new Runnable() {
                @Override
                public void run() {
                    new AsyncTask<Void, Void, Map<DownloadItem, Boolean>>() {
                        @Override
                        public Map<DownloadItem, Boolean> doInBackground(Void... params) {
                            return updateAllNotifications();
                        }

                        @Override
                        protected void onPostExecute(
                                Map<DownloadItem, Boolean> result) {
                            for (Map.Entry<DownloadItem, Boolean> entry : result.entrySet()) {
                                DownloadItem download = entry.getKey();
                                if (!download.hasSystemDownloadId()) {
                                    // TODO(qinmin): get the failure message from native.
                                    onDownloadFailed(download.getDownloadInfo().getFileName(),
                                            DownloadManager.ERROR_UNKNOWN);
                                    return;
                                }
                                boolean canResolve = entry.getValue();
                                if (canResolve && shouldOpenAfterDownload(
                                        download.getDownloadInfo())) {
                                    handleAutoOpenAfterDownload(download);
                                } else {
                                    mDownloadSnackbarController.onDownloadSucceeded(
                                            download.getDownloadInfo(),
                                            download.getSystemDownloadId(),
                                            canResolve);
                                }
                            }
                        }
                    }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
                    mIsUIUpdateScheduled.set(false);
                }
            };
            mHandler.postDelayed(updateTask, mUpdateDelayInMillis);
        }
    }

    /**
     * Updates the progress of a download.
     *
     * @param downloadItem Information about the download.
     * @param downloadStatus Status of the download.
     */
    private void updateDownloadProgress(DownloadItem downloadItem, int downloadStatus) {
        String id = downloadItem.getId();
        DownloadProgress progress = mDownloadProgressMap.get(id);
        if (progress == null) {
            if (!downloadItem.getDownloadInfo().isPaused()) {
                long startTime = System.currentTimeMillis();
                progress = new DownloadProgress(
                        startTime, isActiveNetworkMetered(mContext), downloadItem, downloadStatus);
                mDownloadProgressMap.putIfAbsent(id, progress);
                if (getUmaStatsEntry(downloadItem.getId()) == null) {
                    addUmaStatsEntry(new DownloadUmaStatsEntry(
                            downloadItem.getId(), startTime, 0, false, false));
                }
            }
            return;
        }

        progress.mDownloadStatus = downloadStatus;
        progress.mDownloadItem = downloadItem;
        DownloadUmaStatsEntry entry;
        switch (downloadStatus) {
            case DOWNLOAD_STATUS_COMPLETE:
            case DOWNLOAD_STATUS_FAILED:
            case DOWNLOAD_STATUS_CANCELLED:
                recordDownloadFinishedUMA(downloadStatus, downloadItem.getId(),
                        downloadItem.getDownloadInfo().getContentLength());
                break;
            case DOWNLOAD_STATUS_INTERRUPTED:
                entry = getUmaStatsEntry(downloadItem.getId());
                entry.numInterruptions++;
                storeUmaEntries();
                break;
            case DOWNLOAD_STATUS_IN_PROGRESS:
                entry = getUmaStatsEntry(downloadItem.getId());
                if (entry.isPaused != downloadItem.getDownloadInfo().isPaused()) {
                    entry.isPaused = downloadItem.getDownloadInfo().isPaused();
                    storeUmaEntries();
                }
                break;
            default:
                assert false;
        }
    }

    /**
     * Sets the download handler for OMA downloads, for testing purpose.
     *
     * @param omaDownloadHandler Download handler for OMA contents.
     */
    @VisibleForTesting
    protected void setOMADownloadHandler(OMADownloadHandler omaDownloadHandler) {
        mOMADownloadHandler = omaDownloadHandler;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        if (!DownloadManager.ACTION_DOWNLOAD_COMPLETE.equals(action)) return;
        long downloadId = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID,
                DownloadItem.INVALID_DOWNLOAD_ID);
        if (downloadId == DownloadItem.INVALID_DOWNLOAD_ID) return;
        boolean isPendingOMADownload = mOMADownloadHandler.isPendingOMADownload(downloadId);
        boolean isInOMASharedPrefs = isDownloadIdInOMASharedPrefs(downloadId);
        if (isPendingOMADownload || isInOMASharedPrefs) {
            clearPendingOMADownload(downloadId, null);
            mSystemDownloadIdMap.remove(downloadId);
            return;
        }
        DownloadItem downloadItem = mSystemDownloadIdMap.get(downloadId);
        if (downloadItem != null) {
            mDownloadManagerDelegate.queryDownloadResult(downloadItem, true, this);
            mSystemDownloadIdMap.remove(downloadId);
            if (mSystemDownloadIdMap.size() == 0) {
                mContext.unregisterReceiver(this);
            }
        }
    }

    /**
     * Sends the download request to Android download manager. If |notifyCompleted| is true,
     * a notification will be sent to the user once download is complete and the downloaded
     * content will be saved to the public directory on external storage. Otherwise, the
     * download will be saved in the app directory and user will not get any notifications
     * after download completion.
     * TODO(qinmin): move this to DownloadManagerDelegate.
     *
     * @param info Download information about the download.
     * @param notifyCompleted Whether to notify the user after Downloadmanager completes the
     *                        download.
     */
    public void enqueueDownloadManagerRequest(
            final DownloadItem item, boolean notifyCompleted) {
        EnqueueDownloadRequestTask task = new EnqueueDownloadRequestTask(item);
        task.execute(notifyCompleted);
    }

    /**
     * Async task to enqueue a download request into DownloadManager.
     */
    private class EnqueueDownloadRequestTask extends AsyncTask<Boolean, Void, Boolean> {
        private long mDownloadId;
        private final DownloadItem mDownloadItem;
        private int mFailureReason;
        private long mStartTime;

        public EnqueueDownloadRequestTask(DownloadItem downloadItem) {
            mDownloadItem = downloadItem;
        }

        @Override
        public Boolean doInBackground(Boolean... booleans) {
            boolean notifyCompleted = booleans[0];
            Uri uri = Uri.parse(mDownloadItem.getDownloadInfo().getUrl());
            DownloadManager.Request request;
            DownloadInfo info = mDownloadItem.getDownloadInfo();
            try {
                request = new DownloadManager.Request(uri);
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Cannot download non http or https scheme");
                // Use ERROR_UNHANDLED_HTTP_CODE so that it will be treated as
                // a server error.
                mFailureReason = DownloadManager.ERROR_UNHANDLED_HTTP_CODE;
                return false;
            }

            request.setMimeType(info.getMimeType());
            try {
                if (notifyCompleted) {
                    // Set downloaded file destination to /sdcard/Download or, should it be
                    // set to one of several Environment.DIRECTORY* dirs depending on mimetype?
                    request.setDestinationInExternalPublicDir(
                            Environment.DIRECTORY_DOWNLOADS, info.getFileName());
                } else {
                    File dir = new File(mContext.getExternalFilesDir(null), DOWNLOAD_DIRECTORY);
                    if (dir.mkdir() || dir.isDirectory()) {
                        File file = new File(dir, info.getFileName());
                        request.setDestinationUri(Uri.fromFile(file));
                    } else {
                        Log.e(TAG, "Cannot create download directory");
                        mFailureReason = DownloadManager.ERROR_FILE_ERROR;
                        return false;
                    }
                }
            } catch (IllegalStateException e) {
                Log.e(TAG, "Cannot create download directory");
                mFailureReason = DownloadManager.ERROR_FILE_ERROR;
                return false;
            }

            if (notifyCompleted) {
                // Let this downloaded file be scanned by MediaScanner - so that it can
                // show up in Gallery app, for example.
                request.allowScanningByMediaScanner();
                request.setNotificationVisibility(
                        DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED);
            } else {
                request.setNotificationVisibility(
                        DownloadManager.Request.VISIBILITY_VISIBLE);
            }
            String description = info.getDescription();
            if (TextUtils.isEmpty(description)) {
                description = info.getFileName();
            }
            request.setDescription(description);
            request.setTitle(info.getFileName());
            request.addRequestHeader("Cookie", info.getCookie());
            request.addRequestHeader("Referer", info.getReferer());
            request.addRequestHeader("User-Agent", info.getUserAgent());

            DownloadManager manager =
                    (DownloadManager) mContext.getSystemService(Context.DOWNLOAD_SERVICE);
            try {
                mStartTime = System.currentTimeMillis();
                mDownloadId = manager.enqueue(request);
            } catch (IllegalArgumentException e) {
                // See crbug.com/143499 for more details.
                Log.e(TAG, "Download failed: " + e);
                mFailureReason = DownloadManager.ERROR_UNKNOWN;
                return false;
            } catch (RuntimeException e) {
                // See crbug.com/490442 for more details.
                Log.e(TAG, "Failed to create target file on the external storage: " + e);
                mFailureReason = DownloadManager.ERROR_FILE_ERROR;
                return false;
            }
            return true;
        }

        @Override
        protected void onPostExecute(Boolean result) {
            boolean isPendingOMADownload = mOMADownloadHandler.isPendingOMADownload(
                    mDownloadItem.getSystemDownloadId());
            if (!result) {
                onDownloadFailed(mDownloadItem.getDownloadInfo().getFileName(), mFailureReason);
                recordDownloadCompletionStats(true, DOWNLOAD_STATUS_FAILED, 0, 0, 0);
                if (isPendingOMADownload) {
                    mOMADownloadHandler.onDownloadFailed(
                            mDownloadItem.getDownloadInfo(), mDownloadItem.getSystemDownloadId(),
                            DownloadManager.ERROR_UNKNOWN, null);
                }
                return;
            }
            Toast.makeText(mContext, R.string.download_pending, Toast.LENGTH_SHORT).show();
            if (isPendingOMADownload) {
                // A new downloadId is generated, needs to update the OMADownloadHandler
                // about this.
                mOMADownloadHandler.updateDownloadInfo(
                        mDownloadItem.getSystemDownloadId(), mDownloadId);
                // TODO(qinmin): use a file instead of shared prefs to save the
                // OMA information in case chrome is killed. This will allow us to
                // save more information like cookies and user agent.
                String notifyUri = mOMADownloadHandler.getInstallNotifyInfo(mDownloadId);
                if (!TextUtils.isEmpty(notifyUri)) {
                    OMAEntry entry = new OMAEntry(mDownloadId, notifyUri);
                    addOMADownloadToSharedPrefs(entry.generateSharedPrefsString());
                }
            }
            if (mSystemDownloadIdMap.size() == 0) {
                mContext.registerReceiver(DownloadManagerService.this,
                        new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE));
            }
            addUmaStatsEntry(new DownloadUmaStatsEntry(
                    String.valueOf(mDownloadId), mStartTime, 0, false, true));
            mDownloadItem.setSystemDownloadId(mDownloadId);
            mDownloadItem.setStartTime(mStartTime);
            mSystemDownloadIdMap.put(mDownloadId, mDownloadItem);
        }
    }

    /**
     * Determines if the download should be immediately opened after
     * downloading.
     *
     * @param downloadInfo Information about the download.
     * @return true if the downloaded content should be opened, or false otherwise.
     */
    @VisibleForTesting
    static boolean shouldOpenAfterDownload(DownloadInfo downloadInfo) {
        String type = downloadInfo.getMimeType();
        return downloadInfo.hasUserGesture() && MIME_TYPES_TO_OPEN.contains(type);
    }

    /**
     * Returns true if the download is for OMA download description file.
     *
     * @param downloadInfo Information about the download.
     * @return true if the downloaded is OMA download description, or false otherwise.
     */
    static boolean isOMADownloadDescription(DownloadInfo downloadInfo) {
        return OMADownloadHandler.OMA_DOWNLOAD_DESCRIPTOR_MIME.equalsIgnoreCase(
                downloadInfo.getMimeType());
    }

    /**
     * Launch the best activity for the given intent. If a default activity is provided,
     * choose the default one. Otherwise, return the Intent picker if there are more than one
     * capable activities. If the intent is pdf type, return the platform pdf viewer if
     * it is available so user don't need to choose it from Intent picker.
     *
     * @param context Context of the app.
     * @param intent Intent to open.
     * @param allowSelfOpen Whether chrome itself is allowed to open the intent.
     * @return true if an Intent is launched, or false otherwise.
     */
    public static boolean openIntent(Context context, Intent intent, boolean allowSelfOpen) {
        boolean activityResolved = ExternalNavigationDelegateImpl.resolveIntent(
                context, intent, allowSelfOpen);
        if (activityResolved) {
            try {
                context.startActivity(intent);
                return true;
            } catch (ActivityNotFoundException ex) {
                Log.d(TAG, "activity not found for " + intent.getType()
                        + " over " + intent.getData().getScheme(), ex);
            } catch (SecurityException ex) {
                Log.d(TAG, "cannot open intent: " + intent, ex);
            }
        }
        return false;
    }

    /**
     * Return the intent to launch for a given download item.
     *
     * @param context Context of the app.
     * @param downloadId ID of the download item in DownloadManager.
     * @return the intent to launch for the given download item.
     */
    private static Intent getLaunchIntentFromDownloadId(Context context, long downloadId) {
        assert !ThreadUtils.runningOnUiThread();
        DownloadManager manager =
                (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
        Uri uri = manager.getUriForDownloadedFile(downloadId);
        if (uri == null) return null;
        Intent launchIntent = new Intent(Intent.ACTION_VIEW);
        launchIntent.setDataAndType(uri, manager.getMimeTypeForDownloadedFile(downloadId));
        launchIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_GRANT_READ_URI_PERMISSION);
        return launchIntent;
    }

    /**
     * Return whether a download item can be resolved to any activity.
     *
     * @param context Context of the app.
     * @param download A download item.
     * @return true if the download item can be resolved, or false otherwise.
     */
    static boolean canResolveDownloadItem(Context context, DownloadItem download) {
        assert !ThreadUtils.runningOnUiThread();
        Intent intent = getLaunchIntentFromDownloadId(context, download.getSystemDownloadId());
        return (intent == null) ? false : ExternalNavigationDelegateImpl.resolveIntent(
                context, intent, true);
    }

    /**
     * Launch the intent for a given download item.
     * TODO(qinmin): Move this to DownloadManagerDelegate.
     *
     * @param downloadId ID of the download item in DownloadManager.
     */
    protected void openDownloadedContent(final long downloadId) {
        new AsyncTask<Void, Void, Intent>() {
            @Override
            public Intent doInBackground(Void... params) {
                return getLaunchIntentFromDownloadId(mContext, downloadId);
            }

            @Override
            protected void onPostExecute(Intent intent) {
                if (intent != null) {
                    openIntent(mContext, intent, true);
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Called when a download fails.
     *
     * @param fileName Name of the download file.
     * @param reason Reason of failure reported by android DownloadManager
     */
    protected void onDownloadFailed(String fileName, int reason) {
        String reasonString = mContext.getString(
                R.string.download_failed_reason_unknown_error, fileName);
        switch (reason) {
            case DownloadManager.ERROR_FILE_ALREADY_EXISTS:
                reasonString = mContext.getString(
                        R.string.download_failed_reason_file_already_exists, fileName);
                break;
            case DownloadManager.ERROR_FILE_ERROR:
                reasonString = mContext.getString(
                        R.string.download_failed_reason_file_system_error, fileName);
                break;
            case DownloadManager.ERROR_INSUFFICIENT_SPACE:
                reasonString = mContext.getString(
                        R.string.download_failed_reason_insufficient_space, fileName);
                break;
            case DownloadManager.ERROR_CANNOT_RESUME:
            case DownloadManager.ERROR_HTTP_DATA_ERROR:
                reasonString = mContext.getString(
                        R.string.download_failed_reason_network_failures, fileName);
                break;
            case DownloadManager.ERROR_TOO_MANY_REDIRECTS:
            case DownloadManager.ERROR_UNHANDLED_HTTP_CODE:
                reasonString = mContext.getString(
                        R.string.download_failed_reason_server_issues, fileName);
                break;
            case DownloadManager.ERROR_DEVICE_NOT_FOUND:
                reasonString = mContext.getString(
                        R.string.download_failed_reason_storage_not_found, fileName);
                break;
            case DownloadManager.ERROR_UNKNOWN:
            default:
                break;
        }
        mDownloadSnackbarController.onDownloadFailed(
                reasonString, reason == DownloadManager.ERROR_FILE_ALREADY_EXISTS);
    }

    /**
     * Set the DownloadSnackbarController for testing purpose.
     */
    @VisibleForTesting
    protected void setDownloadSnackbarController(
            DownloadSnackbarController downloadSnackbarController) {
        mDownloadSnackbarController = downloadSnackbarController;
    }

    /**
     * Open the Activity which shows a list of all downloads.
     * @param context Application context
     */
    protected static void openDownloadsPage(Context context) {
        Intent pageView = new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS);
        pageView.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            context.startActivity(pageView);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Cannot find Downloads app", e);
        }
    }

    /**
     * Called to cancel a download notification.
     * @param notificationId Notification Id of the download.
     * @param downloadGuid GUID of the download.
     */
    void cancelNotification(int notificationId, String downloadGuid) {
        mDownloadNotifier.cancelNotification(notificationId, downloadGuid);
    }

    /**
     * Called to resume a paused download.
     * @param item Download item to resume.
     * @param hasUserGesture Whether the resumption is triggered by user gesture.
     */
    @VisibleForTesting
    protected void resumeDownload(DownloadItem item, boolean hasUserGesture) {
        DownloadProgress progress = mDownloadProgressMap.get(item.getId());
        if (progress != null && progress.mDownloadStatus == DOWNLOAD_STATUS_IN_PROGRESS
                && !progress.mDownloadItem.getDownloadInfo().isPaused()) {
            // Download already in progress, do nothing
            return;
        }
        int uma = hasUserGesture ? UMA_DOWNLOAD_RESUMPTION_CLICKED
                : UMA_DOWNLOAD_RESUMPTION_AUTO_STARTED;
        recordDownloadResumption(uma);
        if (progress == null) {
            assert !item.getDownloadInfo().isPaused();
            updateDownloadProgress(item, DOWNLOAD_STATUS_IN_PROGRESS);
            progress = mDownloadProgressMap.get(item.getId());
            // If progress is null, the browser must have been killed while the download is active.
            recordDownloadResumption(UMA_DOWNLOAD_RESUMPTION_BROWSER_KILLED);
        }
        if (hasUserGesture) {
            // If user manually resumes a download, update the connection type that the download
            // can start. If the previous connection type is metered, manually resuming on an
            // unmetered network should not affect the original connection type.
            if (!progress.mCanDownloadWhileMetered) {
                progress.mCanDownloadWhileMetered = isActiveNetworkMetered(mContext);
            }
        }
        nativeResumeDownload(getNativeDownloadManagerService(), item.getId());
    }

    /**
     * Called to cancel a download.
     * @param downloadGuid GUID of the download.
     */
    void cancelDownload(String downloadGuid) {
        DownloadProgress progress = mDownloadProgressMap.get(downloadGuid);
        boolean isOffTheRecord = progress == null
                ? false : progress.mDownloadItem.getDownloadInfo().isOffTheRecord();
        nativeCancelDownload(getNativeDownloadManagerService(), downloadGuid, isOffTheRecord);
        recordDownloadFinishedUMA(DOWNLOAD_STATUS_CANCELLED, downloadGuid, 0);
    }

    /**
     * Called to pause a download.
     * @param downloadGuid GUID of the download.
     */
    void pauseDownload(String downloadGuid) {
        nativePauseDownload(getNativeDownloadManagerService(), downloadGuid);
        DownloadProgress progress = mDownloadProgressMap.get(downloadGuid);
        // Calling pause will stop listening to the download item. Update its progress now.
        // If download is already completed, canceled or failed, there is no need to update the
        // download notification.
        if (progress != null && (progress.mDownloadStatus == DOWNLOAD_STATUS_INTERRUPTED
                || progress.mDownloadStatus == DOWNLOAD_STATUS_IN_PROGRESS)) {
            DownloadInfo info = DownloadInfo.Builder.fromDownloadInfo(
                    progress.mDownloadItem.getDownloadInfo()).setIsPaused(true).build();
            onDownloadUpdated(info);
        }
    }

    /**
     * Helper method to create and retrieve the native DownloadManagerService when needed.
     * @return pointer to native DownloadManagerService.
     */
    private long getNativeDownloadManagerService() {
        if (mNativeDownloadManagerService == 0) {
            mNativeDownloadManagerService = nativeInit();
        }
        return mNativeDownloadManagerService;
    }

    @CalledByNative
    void onResumptionFailed(String downloadGuid) {
        mDownloadNotifier.notifyDownloadFailed(
                new DownloadInfo.Builder().setDownloadGuid(downloadGuid).build());
        mDownloadProgressMap.remove(downloadGuid);
        recordDownloadResumption(UMA_DOWNLOAD_RESUMPTION_FAILED);
        recordDownloadFinishedUMA(DOWNLOAD_STATUS_FAILED, downloadGuid, 0);
    }

    /**
     * Helper method to record the download resumption UMA.
     * @param type UMA type to be recorded.
     */
    private void recordDownloadResumption(int type) {
        assert type < UMA_DOWNLOAD_RESUMPTION_COUNT && type >= 0;
        RecordHistogram.recordEnumeratedHistogram(
                "MobileDownload.DownloadResumption", type, UMA_DOWNLOAD_RESUMPTION_COUNT);
    }

    /**
     * Helper method to record the metrics when a download completes.
     * @param useDownloadManager Whether the download goes through Android DownloadManager.
     * @param status Download completion status.
     * @param totalDuration Total time in milliseconds to download the file.
     * @param bytesDownloaded Total bytes downloaded.
     * @param numInterruptions Number of interruptions during the download.
     */
    private void recordDownloadCompletionStats(boolean useDownloadManager, int status,
            long totalDuration, long bytesDownloaded, int numInterruptions) {
        switch (status) {
            case DOWNLOAD_STATUS_COMPLETE:
                if (useDownloadManager) {
                    RecordHistogram.recordLongTimesHistogram(
                            "MobileDownload.DownloadTime.DownloadManager.Success",
                            totalDuration, TimeUnit.MILLISECONDS);
                    RecordHistogram.recordCount1000Histogram(
                            "MobileDownload.BytesDownloaded.DownloadManager.Success",
                            (int) (bytesDownloaded / 1024));
                } else {
                    RecordHistogram.recordLongTimesHistogram(
                            "MobileDownload.DownloadTime.ChromeNetworkStack.Success",
                            totalDuration, TimeUnit.MILLISECONDS);
                    RecordHistogram.recordCount1000Histogram(
                            "MobileDownload.BytesDownloaded.ChromeNetworkStack.Success",
                            (int) (bytesDownloaded / 1024));
                    RecordHistogram.recordCountHistogram(
                            "MobileDownload.InterruptionsCount.ChromeNetworkStack.Success",
                            numInterruptions);
                }
                break;
            case DOWNLOAD_STATUS_FAILED:
                if (useDownloadManager) {
                    RecordHistogram.recordLongTimesHistogram(
                            "MobileDownload.DownloadTime.DownloadManager.Failure",
                            totalDuration, TimeUnit.MILLISECONDS);
                    RecordHistogram.recordCount1000Histogram(
                            "MobileDownload.BytesDownloaded.DownloadManager.Failure",
                            (int) (bytesDownloaded / 1024));
                } else {
                    RecordHistogram.recordLongTimesHistogram(
                            "MobileDownload.DownloadTime.ChromeNetworkStack.Failure",
                            totalDuration, TimeUnit.MILLISECONDS);
                    RecordHistogram.recordCount1000Histogram(
                            "MobileDownload.BytesDownloaded.ChromeNetworkStack.Failure",
                            (int) (bytesDownloaded / 1024));
                    RecordHistogram.recordCountHistogram(
                            "MobileDownload.InterruptionsCount.ChromeNetworkStack.Failure",
                            numInterruptions);
                }
                break;
            case DOWNLOAD_STATUS_CANCELLED:
                if (!useDownloadManager) {
                    RecordHistogram.recordLongTimesHistogram(
                            "MobileDownload.DownloadTime.ChromeNetworkStack.Cancel",
                            totalDuration, TimeUnit.MILLISECONDS);
                    RecordHistogram.recordCountHistogram(
                            "MobileDownload.InterruptionsCount.ChromeNetworkStack.Cancel",
                            numInterruptions);
                }
                break;
            default:
                break;
        }
    }

    @Override
    public void onQueryCompleted(
            DownloadManagerDelegate.DownloadQueryResult result, boolean showNotification) {
        if (result.downloadStatus == DOWNLOAD_STATUS_IN_PROGRESS) return;
        if (showNotification) {
            switch (result.downloadStatus) {
                case DOWNLOAD_STATUS_COMPLETE:
                    if (shouldOpenAfterDownload(result.item.getDownloadInfo())
                            && result.canResolve) {
                        handleAutoOpenAfterDownload(result.item);
                    } else {
                        mDownloadSnackbarController.onDownloadSucceeded(
                                result.item.getDownloadInfo(), result.item.getSystemDownloadId(),
                                result.canResolve);
                    }
                    break;
                case DOWNLOAD_STATUS_FAILED:
                    onDownloadFailed(
                            result.item.getDownloadInfo().getFileName(), result.failureReason);
                    break;
                default:
                    break;
            }
        }
        recordDownloadCompletionStats(true, result.downloadStatus,
                result.downloadTimeInMilliseconds, result.bytesDownloaded, 0);
        removeUmaStatsEntry(result.item.getId());
    }

    /**
     * Called by tests to disable listening to network connection changes.
     */
    @VisibleForTesting
    static void disableNetworkListenerForTest() {
        sIsNetworkListenerDisabled = true;
    }

    /**
     * Called by tests to set the network type.
     * @isNetworkMetered Whether the network should appear to be metered.
     */
    @VisibleForTesting
    static void setIsNetworkMeteredForTest(boolean isNetworkMetered) {
        sIsNetworkMetered = isNetworkMetered;
    }

    /**
     * Helper method to add an auto resumable download.
     * @param guid Id of the download item.
     */
    private void addAutoResumableDownload(String guid) {
        if (mAutoResumableDownloadIds.isEmpty() && !sIsNetworkListenerDisabled) {
            mNetworkChangeNotifier = new NetworkChangeNotifierAutoDetect(this, mContext,
                    new RegistrationPolicyAlwaysRegister());
        }
        if (!mAutoResumableDownloadIds.contains(guid)) {
            mAutoResumableDownloadIds.add(guid);
        }
    }

    /**
     * Helper method to remove an auto resumable download.
     * @param guid Id of the download item.
     */
    private void removeAutoResumableDownload(String guid) {
        if (mAutoResumableDownloadIds.isEmpty()) return;
        mAutoResumableDownloadIds.remove(guid);
        stopListenToConnectionChangeIfNotNeeded();
    }

    @Override
    public void onConnectionTypeChanged(int connectionType) {
        if (mAutoResumableDownloadIds.isEmpty()) return;
        if (connectionType == ConnectionType.CONNECTION_NONE) return;
        boolean isMetered = isActiveNetworkMetered(mContext);
        Iterator<String> iterator = mAutoResumableDownloadIds.iterator();
        while (iterator.hasNext()) {
            final String id = iterator.next();
            final DownloadProgress progress = mDownloadProgressMap.get(id);
            // Introduce some delay in each resumption so we don't start all of them immediately.
            if (progress != null && (progress.mCanDownloadWhileMetered || !isMetered)) {
                // Remove the pending resumable item so that the task won't be posted again on the
                // next connectivity change.
                iterator.remove();
                // Post a delayed task to avoid an issue that when connectivity status just changed
                // to CONNECTED, immediately establishing a connection will sometimes fail.
                mHandler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        resumeDownload(progress.mDownloadItem, false);
                    }
                }, mUpdateDelayInMillis);
            }
        }
        stopListenToConnectionChangeIfNotNeeded();
    }

    /**
     * Helper method to stop listening to the connection type change
     * if it is no longer needed.
     */
    private void stopListenToConnectionChangeIfNotNeeded() {
        if (mAutoResumableDownloadIds.isEmpty() && mNetworkChangeNotifier != null) {
            mNetworkChangeNotifier.destroy();
            mNetworkChangeNotifier = null;
        }
    }

    static boolean isActiveNetworkMetered(Context context) {
        if (sIsNetworkListenerDisabled) return sIsNetworkMetered;
        ConnectivityManager cm =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        return cm.isActiveNetworkMetered();
    }

    /**
     * Adds a DownloadUmaStatsEntry to |mUmaEntries| and SharedPrefs.
     * @param umaEntry A DownloadUmaStatsEntry to be added.
     */
    private void addUmaStatsEntry(DownloadUmaStatsEntry umaEntry) {
        mUmaEntries.add(umaEntry);
        storeUmaEntries();
    }

    /**
     * Gets a DownloadUmaStatsEntry from |mUmaEntries| given by its ID.
     * @param id ID of the UMA entry.
     */
    private DownloadUmaStatsEntry getUmaStatsEntry(String id) {
        Iterator<DownloadUmaStatsEntry> iterator = mUmaEntries.iterator();
        while (iterator.hasNext()) {
            DownloadUmaStatsEntry entry = iterator.next();
            if (entry.id.equals(id)) {
                return entry;
            }
        }
        return null;
    }

    /**
     * Removes a DownloadUmaStatsEntry from SharedPrefs given by the id.
     * @param id ID to be removed.
     */
    private void removeUmaStatsEntry(String id) {
        Iterator<DownloadUmaStatsEntry> iterator = mUmaEntries.iterator();
        boolean found = false;
        while (iterator.hasNext()) {
            DownloadUmaStatsEntry entry = iterator.next();
            if (entry.id.equals(id)) {
                iterator.remove();
                found = true;
                break;
            }
        }
        if (found) {
            storeUmaEntries();
        }
    }

    /**
     * Helper method to store all the DownloadUmaStatsEntry into SharedPreferences.
     */
    private void storeUmaEntries() {
        Set<String> entries = new HashSet<String>();
        for (int i = 0; i < mUmaEntries.size(); ++i) {
            entries.add(mUmaEntries.get(i).getSharedPreferenceString());
        }
        storeDownloadInfo(mSharedPrefs, DOWNLOAD_UMA_ENTRY, entries);
    }

    /**
     * Helper method to record the download completion UMA and remove the SharedPreferences entry.
     */
    private void recordDownloadFinishedUMA(
            int downloadStatus, String entryId, long bytesDownloaded) {
        DownloadUmaStatsEntry entry = getUmaStatsEntry(entryId);
        if (entry == null) return;
        long currentTime = System.currentTimeMillis();
        long totalTime = Math.max(0, currentTime - entry.downloadStartTime);
        recordDownloadCompletionStats(
                false, downloadStatus, totalTime, bytesDownloaded, entry.numInterruptions);
        removeUmaStatsEntry(entryId);
    }

    /**
     * Parse the DownloadUmaStatsEntry from the shared preference.
     */
    private void parseUMAStatsEntriesFromSharedPrefs() {
        if (mSharedPrefs.contains(DOWNLOAD_UMA_ENTRY)) {
            Set<String> entries =
                    DownloadManagerService.getStoredDownloadInfo(mSharedPrefs, DOWNLOAD_UMA_ENTRY);
            for (String entryString : entries) {
                mUmaEntries.add(DownloadUmaStatsEntry.parseFromString(entryString));
            }
        }
    }

    @Override
    public void onMaxBandwidthChanged(double maxBandwidthMbps) {}

    @Override
    public void onNetworkConnect(int netId, int connectionType) {}

    @Override
    public void onNetworkSoonToDisconnect(int netId) {}

    @Override
    public void onNetworkDisconnect(int netId) {}

    @Override
    public void updateActiveNetworkList(int[] activeNetIds) {}

    private native long nativeInit();
    private native void nativeResumeDownload(
            long nativeDownloadManagerService, String downloadGuid);
    private native void nativeCancelDownload(
            long nativeDownloadManagerService, String downloadGuid, boolean isOffTheRecord);
    private native void nativePauseDownload(long nativeDownloadManagerService, String downloadGuid);
}
