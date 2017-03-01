// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.crash;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.webkit.ValueCallback;

import org.chromium.android_webview.PlatformServiceBridge;
import org.chromium.android_webview.command_line.CommandLineUtil;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.MinidumpUploadCallable;
import org.chromium.components.minidump_uploader.MinidumpUploader;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;

/**
 * Class in charge of uploading minidumps from WebView's data directory.
 * This class gets invoked from a JobScheduler job and posts the operation of uploading minidumps to
 * a privately defined worker thread.
 * Note that this implementation is state-less in the sense that it doesn't keep track of whether it
 * successfully uploaded any minidumps. At the end of a job it simply checks whether there are any
 * minidumps left to upload, and if so, the job is rescheduled.
 */
public class MinidumpUploaderImpl implements MinidumpUploader {
    private static final String TAG = "MinidumpUploaderImpl";
    private Context mContext;
    private final ConnectivityManager mConnectivityManager;

    /**
     * Manages the set of pending and failed local minidump files.
     */
    private final CrashFileManager mFileManager;

    /**
     * Whether the current job has been canceled. This is written to from the main thread, and read
     * from the worker thread.
     */
    private boolean mCancelUpload = false;

    /**
     * The thread used for the actual work of uploading minidumps.
     */
    private Thread mWorkerThread;

    private boolean mPermittedByUser = false;

    @VisibleForTesting
    public static final int MAX_UPLOAD_TRIES_ALLOWED = 3;

    @VisibleForTesting
    public MinidumpUploaderImpl(Context context) {
        mConnectivityManager =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        mContext = context;
        File crashDir = CrashReceiverService.createWebViewCrashDir(context);
        mFileManager = createCrashFileManager(crashDir);
        if (!mFileManager.ensureCrashDirExists()) {
            Log.e(TAG, "Crash directory doesn't exist!");
        }
    }

    /**
     * Utility method to allow tests to customize the behavior of the crash file manager.
     * @param {crashDir} The directory in which to store crash files (i.e. minidumps).
     */
    @VisibleForTesting
    public CrashFileManager createCrashFileManager(File crashDir) {
        return new CrashFileManager(crashDir);
    }

    /**
     * Utility method to allow us to test the logic of this class by injecting
     * test-specific MinidumpUploadCallables.
     */
    @VisibleForTesting
    public MinidumpUploadCallable createMinidumpUploadCallable(File minidumpFile, File logfile) {
        return new MinidumpUploadCallable(
                minidumpFile, logfile, createWebViewCrashReportingManager());
    }

    /**
     * Utility method to allow us to test the logic of this class by injecting
     * a test-specific PlatformServiceBridge.
     */
    @VisibleForTesting
    public PlatformServiceBridge createPlatformServiceBridge() {
        return PlatformServiceBridge.getInstance(mContext);
    }

    @VisibleForTesting
    protected CrashReportingPermissionManager createWebViewCrashReportingManager() {
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
                NetworkInfo networkInfo = mConnectivityManager.getActiveNetworkInfo();
                if (networkInfo == null || !networkInfo.isConnected()) return false;
                return !mConnectivityManager.isActiveNetworkMetered();
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
                return mPermittedByUser;
            }
            @Override
            public boolean isUploadEnabledForTests() {
                // Note that CommandLine/CommandLineUtil are not thread safe. They are initialized
                // on the main thread, but before the current worker thread started - so this thread
                // will have seen the initialization of the CommandLine.
                return CommandLine.getInstance().hasSwitch(
                        CommandLineUtil.CRASH_UPLOADS_ENABLED_FOR_TESTING_SWITCH);
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
                MinidumpUploadCallable uploadCallable = createMinidumpUploadCallable(
                        minidump, mFileManager.getCrashUploadLogFile());
                int uploadResult = uploadCallable.call();

                // Bail if the job was canceled. Note that the cancelation status is checked AFTER
                // trying to upload a minidump. This is to ensure that the scheduler attempts to
                // upload at least one minidump per job. Otherwise, it's possible for a crash loop
                // to continually write files to the crash directory; each such write would
                // reschedule the job, and therefore cancel any pending jobs. In such a scenario,
                // it's important to make at least *some* progress uploading minidumps.
                // Note that other likely cancelation reasons are not a concern, because the upload
                // callable checks relevant state prior to uploading. For example, if the job is
                // canceled because the network connection is lost, or because the user switches
                // over to a metered connection, the callable will detect the changed network state,
                // and not attempt an upload.
                if (mCancelUpload) return;

                // Note that if the job was canceled midway through, the attempt number is not
                // incremented, even if the upload failed. This is because a common reason for
                // cancelation is loss of network connectivity, which does result in a failure, but
                // it's a transient failure rather than something non-recoverable.
                if (uploadResult == MinidumpUploadCallable.UPLOAD_FAILURE) {
                    String newName = CrashFileManager.tryIncrementAttemptNumber(minidump);
                    if (newName == null) {
                        Log.w(TAG, "Failed to increment attempt number of " + minidump);
                    }
                }
            }

            // Clean out old/uploaded minidumps. Note that this clean-up method is more strict than
            // our copying mechanism in the sense that it keeps fewer minidumps.
            mFileManager.cleanOutAllNonFreshMinidumpFiles();

            // Reschedule if there are still minidumps to upload.
            boolean reschedule =
                    mFileManager.getAllMinidumpFiles(MAX_UPLOAD_TRIES_ALLOWED).length > 0;
            mUploadsFinishedCallback.uploadsFinished(reschedule);
        }
    }

    @Override
    public void uploadAllMinidumps(
            final MinidumpUploader.UploadsFinishedCallback uploadsFinishedCallback) {
        ThreadUtils.assertOnUiThread();
        if (mWorkerThread != null) {
            throw new RuntimeException(
                    "A given minidump uploader instance should never be launched more than once.");
        }
        mWorkerThread = new Thread(
                new UploadRunnable(uploadsFinishedCallback), "MinidumpUploader-WorkerThread");
        mCancelUpload = false;

        createPlatformServiceBridge().queryMetricsSetting(new ValueCallback<Boolean>() {
            public void onReceiveValue(Boolean enabled) {
                ThreadUtils.assertOnUiThread();

                mPermittedByUser = enabled;

                // Note that the upload job might have been canceled by this time. However, it's
                // important to start the worker thread anyway to try to make some progress towards
                // uploading minidumps. This is to ensure that in the case where an app is crashing
                // over and over again, resulting in rescheduling jobs over and over again, there's
                // still a chance to upload at least one minidump per job, as long as that job
                // starts before it is canceled by the next job. See the UploadRunnable
                // implementation for more details.
                mWorkerThread.start();
            }
        });
    }

    /**
     * @return Whether to reschedule the uploads.
     */
    @Override
    public boolean cancelUploads() {
        mCancelUpload = true;

        // Reschedule if there are still minidumps to upload.
        return mFileManager.getAllMinidumpFiles(MAX_UPLOAD_TRIES_ALLOWED).length > 0;
    }

    @VisibleForTesting
    public void joinWorkerThreadForTesting() throws InterruptedException {
        mWorkerThread.join();
    }
}
