// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.crash;

import android.annotation.TargetApi;
import android.app.job.JobParameters;
import android.app.job.JobService;
import android.os.Build;

import org.chromium.base.ContextUtils;

/**
 * Class that interacts with the Android JobScheduler to upload Minidumps at appropriate times.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
// OBS: This class needs to be public to be started from android.app.ActivityThread.
public class MinidumpUploadJobService extends JobService {
    Object mRunningLock = new Object();
    boolean mRunningJob = false;
    MinidumpUploader mMinidumpUploader;

    @Override
    public void onCreate() {
        super.onCreate();
    }

    @Override
    public boolean onStartJob(JobParameters params) {
        // Ensure we can use ContextUtils later on (from minidump_uploader component).
        ContextUtils.initApplicationContext(this.getApplicationContext());

        // Ensure we only run one job at a time.
        synchronized (mRunningLock) {
            assert !mRunningJob;
            mRunningJob = true;
        }
        mMinidumpUploader = new MinidumpUploaderImpl(this, true /* cleanOutMinidumps */);
        mMinidumpUploader.uploadAllMinidumps(createJobFinishedCallback(params));
        return true; // true = processing work on a separate thread, false = done already.
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        boolean reschedule = mMinidumpUploader.cancelUploads();
        synchronized (mRunningLock) {
            mRunningJob = false;
        }
        return reschedule;
    }

    private MinidumpUploader.UploadsFinishedCallback createJobFinishedCallback(
            final JobParameters params) {
        return new MinidumpUploader.UploadsFinishedCallback() {
            @Override
            public void uploadsFinished(boolean reschedule) {
                synchronized (mRunningLock) {
                    mRunningJob = false;
                }
                MinidumpUploadJobService.this.jobFinished(params, reschedule);
            }
        };
    }

    @Override
    public void onDestroy() {
        mMinidumpUploader = null;
        super.onDestroy();
    }
}
