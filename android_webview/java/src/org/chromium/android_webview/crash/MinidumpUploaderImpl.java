// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.crash;

import android.content.Context;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.MinidumpUploadCallable;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;

/**
 * Class in charge of uploading Minidumps from WebView's data directory.
 * This class gets invoked from a JobScheduler job and posts the operation of uploading minidumps to
 * a privately defined worker thread.
 * Note that this implementation is state-less in the sense that it doesn't keep track of whether it
 * successfully uploaded any minidumps. At the end of a job it simply checks whether there are any
 * minidumps left to upload, and if so, the job is rescheduled.
 */
public class MinidumpUploaderImpl implements MinidumpUploader {
    private static final String TAG = "MinidumpUploaderImpl";
    private Thread mWorkerThread;
    private final CrashFileManager mFileManager;

    private Object mCancelLock = new Object();
    private boolean mCancelUpload = false;

    private final boolean mCleanOutMinidumps;

    @VisibleForTesting
    public static final int MAX_UPLOAD_TRIES_ALLOWED = 3;

    /**
     * Notify the worker thread that the current job has been canceled - so we shouldn't upload any
     * more minidumps.
     */
    private void setCancelUpload(boolean cancel) {
        synchronized (mCancelLock) {
            mCancelUpload = cancel;
        }
    }

    /**
     * Check whether the current job has been canceled.
     */
    private boolean getCancelUpload() {
        synchronized (mCancelLock) {
            return mCancelUpload;
        }
    }

    @VisibleForTesting
    public MinidumpUploaderImpl(Context context, boolean cleanOutMinidumps) {
        File webviewCrashDir = CrashReceiverService.createWebViewCrashDir(context);
        mFileManager = new CrashFileManager(webviewCrashDir);
        if (!mFileManager.ensureCrashDirExists()) {
            Log.e(TAG, "Crash directory doesn't exist!");
        }
        mCleanOutMinidumps = cleanOutMinidumps;
    }

    /**
     * Utility method to allow us to test the logic of this class by injecting
     * test-MinidumpUploadCallables.
     */
    @VisibleForTesting
    public MinidumpUploadCallable createMinidumpUploadCallable(File minidumpFile, File logfile) {
        return new MinidumpUploadCallable(
                minidumpFile, logfile, createWebViewCrashReportingManager());
    }

    private final CrashReportingPermissionManager createWebViewCrashReportingManager() {
        return new CrashReportingPermissionManager() {
            @Override
            public boolean isClientInMetricsSample() {
                // We will check whether the client is in the metrics sample before
                // generating a minidump - so if no minidump is generated this code will
                // never run and we don't need to check whether we are in the sample.
                // TODO(gsennton): when we switch to using Finch for this value we should use the
                // Finch value here as well.
                return true;
            }
            @Override
            public boolean isNetworkAvailableForCrashUploads() {
                // JobScheduler will call onStopJob causing our upload to be interrupted when our
                // network requirements no longer hold.
                return true;
            }
            @Override
            public boolean isCrashUploadDisabledByCommandLine() {
                return false;
            }
            /**
             * This method is already represented by isClientInMetricsSample() and
             * isNetworkAvailableForCrashUploads().
             */
            @Override
            public boolean isMetricsUploadPermitted() {
                return true;
            }
            @Override
            public boolean isUsageAndCrashReportingPermittedByUser() {
                // We ensure we have user permission before copying minidumps to the directory used
                // by this process - so always return true here.
                return true;
            }
            @Override
            public boolean isUploadEnabledForTests() {
                // We are already checking whether this feature is enabled for manual testing before
                // copying minidumps.
                return false;
            }
        };
    }

    /**
     * Runnable that upload minidumps.
     * This is where the actual uploading happens - an upload job consists of posting this Runnable
     * to the worker thread.
     */
    private class UploadRunnable implements Runnable {
        private final MinidumpUploader.UploadsFinishedCallback mUploadsFinishedCallback;

        public UploadRunnable(MinidumpUploader.UploadsFinishedCallback uploadsFinishedCallback) {
            mUploadsFinishedCallback = uploadsFinishedCallback;
        }

        @Override
        public void run() {
            File[] minidumps = mFileManager.getAllMinidumpFiles(MAX_UPLOAD_TRIES_ALLOWED);
            for (File minidump : minidumps) {
                // Store the name of the current minidump so that we can mark it as a failure from
                // the main thread if JobScheduler tells us to stop.
                MinidumpUploadCallable uploadCallable = createMinidumpUploadCallable(
                        minidump, mFileManager.getCrashUploadLogFile());
                int uploadResult = uploadCallable.call();

                // Job was canceled -> early out: any clean-up will be performed in cancelUploads().
                if (getCancelUpload()) return;

                if (uploadResult == MinidumpUploadCallable.UPLOAD_FAILURE) {
                    String newName = CrashFileManager.tryIncrementAttemptNumber(minidump);
                    if (newName == null) {
                        Log.w(TAG, "Failed to increment attempt number of " + minidump);
                    }
                }
            }

            // Clean out old/uploaded minidumps. Note that this clean-up method is more strict than
            // our copying mechanism in the sense that it keeps less minidumps.
            if (mCleanOutMinidumps) {
                mFileManager.cleanOutAllNonFreshMinidumpFiles();
            }

            // Reschedule if there are still minidumps to upload.
            boolean reschedule =
                    mFileManager.getAllMinidumpFiles(MAX_UPLOAD_TRIES_ALLOWED).length > 0;
            mUploadsFinishedCallback.uploadsFinished(reschedule);
        }
    }

    @Override
    public void uploadAllMinidumps(
            final MinidumpUploader.UploadsFinishedCallback uploadsFinishedCallback) {
        if (mWorkerThread != null) {
            throw new RuntimeException("Only one upload-job should be active at a time");
        }
        mWorkerThread = new Thread(new UploadRunnable(uploadsFinishedCallback), "mWorkerThread");
        setCancelUpload(false);
        mWorkerThread.start();
    }

    /**
     * @return whether to reschedule the uploads.
     */
    @Override
    public boolean cancelUploads() {
        setCancelUpload(true);

        // Reschedule if there are still minidumps to upload.
        return mFileManager.getAllMinidumpFiles(MAX_UPLOAD_TRIES_ALLOWED).length > 0;
    }
}
