// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.support.annotation.StringDef;

import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.MinidumpUploadCallable;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

/**
 * Service that is responsible for uploading crash minidumps to the Google crash server.
 */
public class MinidumpUploadService extends IntentService {
    private static final String TAG = "MinidmpUploadService";

    // Intent actions
    @VisibleForTesting
    static final String ACTION_UPLOAD = "com.google.android.apps.chrome.crash.ACTION_UPLOAD";

    // Intent bundle keys
    @VisibleForTesting
    static final String FILE_TO_UPLOAD_KEY = "minidump_file";
    static final String UPLOAD_LOG_KEY = "upload_log";

    /**
     * The number of times we will try to upload a crash.
     */
    public static final int MAX_TRIES_ALLOWED = 3;

    /**
     * Histogram related constants.
     */
    private static final String HISTOGRAM_NAME_PREFIX = "Tab.AndroidCrashUpload_";
    private static final int HISTOGRAM_MAX = 2;
    private static final int FAILURE = 0;
    private static final int SUCCESS = 1;

    @StringDef({BROWSER, RENDERER, GPU, OTHER})
    public @interface ProcessType {}
    static final String BROWSER = "Browser";
    static final String RENDERER = "Renderer";
    static final String GPU = "GPU";
    static final String OTHER = "Other";

    static final String[] TYPES = {BROWSER, RENDERER, GPU, OTHER};

    public MinidumpUploadService() {
        super(TAG);
        setIntentRedelivery(true);
    }

    /**
     * Stores the successes and failures from uploading crash to UMA,
     */
    public static void storeBreakpadUploadStatsInUma(ChromePreferenceManager pref) {
        for (String type : TYPES) {
            for (int success = pref.getCrashSuccessUploadCount(type); success > 0; success--) {
                RecordHistogram.recordEnumeratedHistogram(
                        HISTOGRAM_NAME_PREFIX + type, SUCCESS, HISTOGRAM_MAX);
            }
            for (int fail = pref.getCrashFailureUploadCount(type); fail > 0; fail--) {
                RecordHistogram.recordEnumeratedHistogram(
                        HISTOGRAM_NAME_PREFIX + type, FAILURE, HISTOGRAM_MAX);
            }

            pref.setCrashSuccessUploadCount(type, 0);
            pref.setCrashFailureUploadCount(type, 0);
        }
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        if (intent == null) return;
        if (!ACTION_UPLOAD.equals(intent.getAction())) {
            Log.w(TAG, "Got unknown action from intent: " + intent.getAction());
            return;
        }

        String minidumpFileName = intent.getStringExtra(FILE_TO_UPLOAD_KEY);
        if (minidumpFileName == null || minidumpFileName.isEmpty()) {
            Log.w(TAG, "Cannot upload crash data since minidump is absent.");
            return;
        }
        File minidumpFile = new File(minidumpFileName);
        if (!minidumpFile.isFile()) {
            Log.w(TAG, "Cannot upload crash data since specified minidump "
                    + minidumpFileName + " is not present.");
            return;
        }
        int tries = CrashFileManager.readAttemptNumber(minidumpFileName);
        // -1 means no attempt number was read.
        if (tries == -1) {
            tries = 0;
        }

        // Since we do not rename a file after reaching max number of tries,
        // files that have maxed out tries will NOT reach this.
        if (tries >= MAX_TRIES_ALLOWED || tries < 0) {
            // Reachable only if the file naming is incorrect by current standard.
            // Thus we log an error instead of recording failure to UMA.
            Log.e(TAG, "Giving up on trying to upload " + minidumpFileName
                    + " after failing to read a valid attempt number.");
            return;
        }

        String logfileName = intent.getStringExtra(UPLOAD_LOG_KEY);
        File logfile = new File(logfileName);

        // Try to upload minidump
        MinidumpUploadCallable minidumpUploadCallable =
                createMinidumpUploadCallable(minidumpFile, logfile);
        @MinidumpUploadCallable.MinidumpUploadStatus int uploadStatus =
                minidumpUploadCallable.call();

        if (uploadStatus == MinidumpUploadCallable.UPLOAD_SUCCESS) {
            // Only update UMA stats if an intended and successful upload.
            incrementCrashSuccessUploadCount(getNewNameAfterSuccessfulUpload(minidumpFileName));
        } else if (uploadStatus == MinidumpUploadCallable.UPLOAD_FAILURE) {
            // Unable to upload minidump. Incrementing try number and restarting.

            // Only create another attempt if we have successfully renamed
            // the file.
            String newName = CrashFileManager.tryIncrementAttemptNumber(minidumpFile);
            if (newName != null) {
                if (++tries < MAX_TRIES_ALLOWED) {
                    // TODO(nyquist): Do this as an exponential backoff.
                    MinidumpUploadRetry.scheduleRetry(
                            getApplicationContext(), getCrashReportingPermissionManager());
                } else {
                    // Only record failure to UMA after we have maxed out the allotted tries.
                    incrementCrashFailureUploadCount(newName);
                    Log.d(TAG, "Giving up on trying to upload " + minidumpFileName + "after "
                            + tries + " number of tries.");
                }
            } else {
                Log.w(TAG, "Failed to rename minidump " + minidumpFileName);
            }
        }
    }

    /**
     * Get the permission manager, can be overridden for testing.
     */
    CrashReportingPermissionManager getCrashReportingPermissionManager() {
        return PrivacyPreferencesManager.getInstance();
    }

    private static String getNewNameAfterSuccessfulUpload(String fileName) {
        return fileName.replace("dmp", "up").replace("forced", "up");
    }

    @ProcessType
    @VisibleForTesting
    protected static String getCrashType(String fileName) {
        // Read file and get the line containing name="ptype".
        BufferedReader fileReader = null;
        try {
            fileReader = new BufferedReader(new FileReader(fileName));
            String line;
            while ((line = fileReader.readLine()) != null) {
                if (line.equals("Content-Disposition: form-data; name=\"ptype\"")) {
                    // Crash type is on the line after the next line.
                    fileReader.readLine();
                    String crashType = fileReader.readLine();
                    if (crashType == null) {
                        return OTHER;
                    }

                    if (crashType.equals("browser")) {
                        return BROWSER;
                    }

                    if (crashType.equals("renderer")) {
                        return RENDERER;
                    }

                    if (crashType.equals("gpu-process")) {
                        return GPU;
                    }

                    return OTHER;
                }
            }
        } catch (IOException e) {
            Log.w(TAG, "Error while reading crash file %s: %s", fileName, e.toString());
        } finally {
            StreamUtil.closeQuietly(fileReader);
        }
        return OTHER;
    }

    /**
     * Increment the count of success/failure by 1 and distinguish between different types of
     * crashes by looking into the file.
     * @param fileName is the name of a minidump file that contains the type of crash.
     */
    private void incrementCrashSuccessUploadCount(String fileName) {
        ChromePreferenceManager.getInstance().incrementCrashSuccessUploadCount(
                getCrashType(fileName));
    }

    private void incrementCrashFailureUploadCount(String fileName) {
        ChromePreferenceManager.getInstance().incrementCrashFailureUploadCount(
                getCrashType(fileName));
    }

    /**
     * Factory method for creating minidump callables.
     *
     * This may be overridden for tests.
     *
     * @param minidumpFile the File to upload.
     * @param logfile the Log file to write to upon successful uploads.
     * @return a new MinidumpUploadCallable.
     */
    @VisibleForTesting
    MinidumpUploadCallable createMinidumpUploadCallable(File minidumpFile, File logfile) {
        return new MinidumpUploadCallable(
                minidumpFile, logfile, getCrashReportingPermissionManager());
    }

    /**
     * Attempts to upload the specified {@param minidumpFile}.
     *
     * Note that this method is asynchronous. All that is guaranteed is that an upload attempt will
     * be enqueued.
     *
     * @param context The application context, in which to initiate the crash report upload.
     * @throws A security excpetion if the caller doesn't have permission to start the upload
     *         service. This can only happen on KitKat and below, due to a framework bug.
     */
    public static void tryUploadCrashDump(Context context, File minidumpFile)
            throws SecurityException {
        CrashFileManager fileManager = new CrashFileManager(context.getCacheDir());
        Intent intent = new Intent(context, MinidumpUploadService.class);
        intent.setAction(ACTION_UPLOAD);
        intent.putExtra(FILE_TO_UPLOAD_KEY, minidumpFile.getAbsolutePath());
        intent.putExtra(UPLOAD_LOG_KEY, fileManager.getCrashUploadLogFile().getAbsolutePath());
        context.startService(intent);
    }

    /**
     * Attempts to upload all minidump files using the given {@link android.content.Context}.
     *
     * Note that this method is asynchronous. All that is guaranteed is that
     * upload attempts will be enqueued.
     *
     * This method is safe to call from the UI thread.
     *
     * @param context Context of the application.
     */
    public static void tryUploadAllCrashDumps(Context context) {
        CrashFileManager fileManager = new CrashFileManager(context.getCacheDir());
        File[] minidumps = fileManager.getAllMinidumpFiles(MAX_TRIES_ALLOWED);
        Log.i(TAG, "Attempting to upload accumulated crash dumps.");
        for (File minidump : minidumps) {
            MinidumpUploadService.tryUploadCrashDump(context, minidump);
        }
    }

    /**
     * Attempts to upload the crash report with the given local ID.
     *
     * Note that this method is asynchronous. All that is guaranteed is that
     * upload attempts will be enqueued.
     *
     * This method is safe to call from the UI thread.
     *
     * @param context the context to use for the intent.
     * @param localId The local ID of the crash report.
     */
    @CalledByNative
    public static void tryUploadCrashDumpWithLocalId(Context context, String localId) {
        if (localId == null || localId.isEmpty()) {
            Log.w(TAG, "Cannot force crash upload since local crash id is absent.");
            return;
        }

        CrashFileManager fileManager = new CrashFileManager(context.getCacheDir());
        File minidumpFile = fileManager.getCrashFileWithLocalId(localId);
        if (minidumpFile == null) {
            Log.w(TAG, "Could not find a crash dump with local ID " + localId);
            return;
        }
        File renamedMinidumpFile = CrashFileManager.trySetForcedUpload(minidumpFile);
        if (renamedMinidumpFile == null) {
            Log.w(TAG, "Could not rename the file " + minidumpFile.getName() + " for re-upload");
            return;
        }
        MinidumpUploadService.tryUploadCrashDump(context, renamedMinidumpFile);
    }
}
