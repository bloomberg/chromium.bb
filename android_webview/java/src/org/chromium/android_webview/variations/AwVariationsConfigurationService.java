// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.variations;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Service;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.util.Base64;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.components.background_task_scheduler.TaskIds;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * AwVariationsConfigurationService is a bound service that shares the Variations seed with all the
 * WebViews on the system. When a WebView binds to the service, it will immediately return the local
 * seed if it is not expired. Otherwise it will schedule a seed fetch job to a job service and
 * return the empty message just to make the WebViews unbind from the service. When multiple
 * WebViews ask for the seed, the service will schedule the first one. The mechanism is ensured by
 * checking if there is a current seed fetch job running, if there is pending job and if the seed is
 * expired.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP) // JobService requires API level 21.
public class AwVariationsConfigurationService extends Service {
    private static final String TAG = "AwVariatnsConfigSvc";

    // Messenger to handle seed requests from the WebView.
    private final Messenger mWebViewAppMessenger = new Messenger(new OnBindHandler());

    // TODO(paulmiller): Remove this when messengers are replaced with AIDL.
    static Message sMsgFromSeedFetchService;

    @Override
    public void onCreate() {
        super.onCreate();
        // Ensure that ContextUtils.getApplicationContext() will return the current context in the
        // following code for the function won't work when we use the standalone WebView package.
        ContextUtils.initApplicationContext(this.getApplicationContext());
        // Ensure that PathUtils.getDataDirectory() will return the app_webview directory in the
        // following code.
        PathUtils.setPrivateDataDirectorySuffix(AwBrowserProcess.PRIVATE_DATA_DIRECTORY_SUFFIX);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mWebViewAppMessenger.getBinder();
    }

    private static class OnBindHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            if (msg.what == AwVariationsUtils.MSG_SEED_REQUEST) {
                AwVariationsConfigurationService.handleSeedRequest(msg);
            } else if (msg.what == AwVariationsUtils.MSG_SEED_TRANSFER) {
                if (sMsgFromSeedFetchService != null) {
                    AwVariationsConfigurationService.handleSeedTransfer();
                } else {
                    assert false;
                }
            } else {
                assert false;
            }
        }
    };

    // Handle the seed transfer call from the Seed Fetch Service. It will store the fetched seed
    // into separate two files in the service directory.
    private static void handleSeedTransfer() {
        Bundle data = sMsgFromSeedFetchService.getData();
        sMsgFromSeedFetchService = null;
        byte[] seedData = data.getByteArray(AwVariationsUtils.KEY_SEED_DATA);
        ArrayList<String> seedPrefAsList = data.getStringArrayList(AwVariationsUtils.KEY_SEED_PREF);
        try {
            File webViewVariationsDir = getOrCreateVariationsDirectory();
            if (seedData == null) {
                Log.e(TAG, "Variations seed data is null.");
            } else {
                storeSeedDataToFile(seedData, webViewVariationsDir);
            }
            AwVariationsUtils.SeedPreference seedPref =
                    AwVariationsUtils.SeedPreference.fromList(seedPrefAsList);
            if (seedPref == null) {
                Log.e(TAG, "Variations seed preference is null.");
            } else {
                storeSeedPrefToFile(seedPref, webViewVariationsDir);
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to write seed to the service directory. " + e);
        }
    }

    // Handle the seed request call from the WebView. It will decide whether to schedule a new seed
    // fetch job and return the seed in the WebView package data directory.
    private static void handleSeedRequest(Message msg) {
        // TODO: add tests for this logic:
        // 1. There are only one seed fetch job once per time
        // 2. Send back empty message if the seed is expired
        // 3. Send back message with data if the seed is not expired
        try {
            File webViewVariationsDir = getOrCreateVariationsDirectory();
            AwVariationsUtils.SeedPreference seedPref =
                    AwVariationsUtils.readSeedPreference(webViewVariationsDir);

            // If the seed needs a refresh schedule one now. Then send the current seed back to the
            // application. If this seed happens to be very old, it will not be used to instantiate
            // field trials.
            if (seedPref.seedNeedsUpdate() && !pendingJobExists()) {
                scheduleSeedFetchJob();
            }
            sendMessageToWebView(msg.replyTo, AwVariationsUtils.MSG_WITH_SEED_DATA);
        } catch (IOException e) {
            Log.e(TAG, "Failed to handle seed request. " + e);
            sendMessageToWebView(msg.replyTo, AwVariationsUtils.MSG_WITHOUT_SEED_DATA);
        }
    }

    // TODO(crbug.com/762607): Fix getPendingJob API level and remove suppression.
    @SuppressLint("NewApi")
    private static boolean pendingJobExists() {
        Context context = ContextUtils.getApplicationContext();
        JobScheduler scheduler =
                (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);
        return scheduler.getPendingJob(TaskIds.WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID) != null;
    }

    private static void scheduleSeedFetchJob() {
        Context context = ContextUtils.getApplicationContext();
        JobScheduler scheduler =
                (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);
        ComponentName componentName =
                new ComponentName(context, AwVariationsSeedFetchService.class);
        JobInfo.Builder builder =
                new JobInfo
                        .Builder(TaskIds.WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID, componentName)
                        // TODO(paulmiller): setOverrideDeadline should not be used to override
                        // network type in production.
                        .setOverrideDeadline(
                                0) /* Specify that this job should be run immediately, overriding
                                      any other constraints like network type. */
                        .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY);
        int result = scheduler.schedule(builder.build());
        if (result != JobScheduler.RESULT_SUCCESS) {
            Log.e(TAG, "Failed to schedule the variations seed fetch job.");
        }
    }

    private static void sendMessageToWebView(Messenger msgr, int msgId) {
        if (msgr == null) {
            Log.e(TAG, "Reply Messenger not found, cannot send back data.");
            return;
        }
        Message msg = Message.obtain();
        msg.what = msgId;
        if (msgId == AwVariationsUtils.MSG_WITH_SEED_DATA) {
            try {
                Bundle bundle = new Bundle();
                bundle.putParcelable(AwVariationsUtils.KEY_SEED_DATA,
                        getSeedFileDescriptor(AwVariationsUtils.SEED_DATA_FILENAME));
                bundle.putParcelable(AwVariationsUtils.KEY_SEED_PREF,
                        getSeedFileDescriptor(AwVariationsUtils.SEED_PREF_FILENAME));
                msg.setData(bundle);
            } catch (IOException e) {
                Log.e(TAG, "Failed to read variations seed data. " + e);
            }
        }
        try {
            msgr.send(msg);
        } catch (RemoteException e) {
            Log.e(TAG, "Error passing service object back to WebView. " + e);
        }
    }

    /**
     * Store the variations seed data as base64 string into a file in a given directory.
     * @param seedData The seed data.
     * @param variationsDir The directory where the file stores into.
     */
    @VisibleForTesting
    public static void storeSeedDataToFile(byte[] seedData, File variationsDir) throws IOException {
        BufferedWriter writer = null;
        try {
            File seedDataTempFile =
                    File.createTempFile(AwVariationsUtils.SEED_DATA_FILENAME, null, variationsDir);
            writer = new BufferedWriter(new FileWriter(seedDataTempFile));
            writer.write(Base64.encodeToString(seedData, Base64.NO_WRAP));
            AwVariationsUtils.renameTempFile(seedDataTempFile,
                    new File(variationsDir, AwVariationsUtils.SEED_DATA_FILENAME));
        } finally {
            AwVariationsUtils.closeStream(writer);
        }
    }

    /**
     * Store the variations seed preference line by line as a file in a given directory.
     * @param seedPref The seed preference holds the related field of the seed.
     * @param variationsDir The directory where the file stores into.
     * @throws IOException if fail to store the seed preference into a file in the given directory.
     */
    @VisibleForTesting
    public static void storeSeedPrefToFile(
            AwVariationsUtils.SeedPreference seedPref, File variationsDir) throws IOException {
        if (seedPref == null) {
            Log.e(TAG, "Variations seed preference is null.");
            return;
        }
        List<String> seedPrefAsList = seedPref.toArrayList();
        BufferedWriter writer = null;
        try {
            File seedPrefTempFile =
                    File.createTempFile(AwVariationsUtils.SEED_PREF_FILENAME, null, variationsDir);
            writer = new BufferedWriter(new FileWriter(seedPrefTempFile));
            for (String s : seedPrefAsList) {
                writer.write(s);
                writer.newLine();
            }
            AwVariationsUtils.renameTempFile(seedPrefTempFile,
                    new File(variationsDir, AwVariationsUtils.SEED_PREF_FILENAME));
        } finally {
            AwVariationsUtils.closeStream(writer);
        }
    }

    /**
     * Get the file descriptor of the variations seed file in the WebView APK directory.
     * @return The parcel file descriptor of an opened seed file.
     * @throws IOException if fail to get or create the WebView variations directory.
     */
    @VisibleForTesting
    public static ParcelFileDescriptor getSeedFileDescriptor(String seedFile) throws IOException {
        File webViewVariationsDir = getOrCreateVariationsDirectory();
        return ParcelFileDescriptor.open(
                new File(webViewVariationsDir, seedFile), ParcelFileDescriptor.MODE_READ_ONLY);
    }

    /**
     * Get or create the variations directory in the WebView APK directory.
     * @return The variations directory path.
     * @throws IOException if fail to create the directory and get the directory.
     */
    @VisibleForTesting
    public static File getOrCreateVariationsDirectory() throws IOException {
        File dir = ContextUtils.getApplicationContext().getFilesDir();

        if (dir.mkdir() || dir.isDirectory()) {
            return dir;
        }
        throw new IOException("Failed to get or create the WebView variations directory.");
    }

    /**
     * Read the test seed data in the file.
     * @param variationsDir The given variations directory.
     * @return String holds the base64-encoded test seed data.
     * @throws IOException if fail to get or create the WebView variations directory.
     */
    @VisibleForTesting
    public static String getSeedDataForTesting(File variationsDir) throws IOException {
        BufferedReader reader = null;
        try {
            File file = new File(variationsDir, AwVariationsUtils.SEED_DATA_FILENAME);
            reader = new BufferedReader(new FileReader(file));
            return reader.readLine();
        } finally {
            AwVariationsUtils.closeStream(reader);
        }
    }

    /**
     * Clear the test data.
     * @throws IOException if fail to get or create the WebView variations directory.
     */
    @VisibleForTesting
    public static void clearDataForTesting() throws IOException {
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        File variationsDir = getOrCreateVariationsDirectory();
        FileUtils.recursivelyDeleteFile(variationsDir);
    }
}
