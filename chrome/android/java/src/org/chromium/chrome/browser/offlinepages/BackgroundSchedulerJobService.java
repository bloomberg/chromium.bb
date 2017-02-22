// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.annotation.TargetApi;
import android.app.job.JobParameters;
import android.app.job.JobService;
import android.os.Build;

/**
 * The background scheduler job service for handling background request in 5.1+.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP_MR1)
public class BackgroundSchedulerJobService extends JobService {
    @Override
    public boolean onStartJob(JobParameters params) {
        // TODO(fgorski): Implement and expose in metadata.
        // Perhaps initialize application context.
        // Perhaps ensure one job runs at a time only.
        // Kick off background processing of downloads here.
        // Processing will happen on a separate thread, and we'll inform OS when done.
        return true;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        // TODO(fgorski): Implement and expose in metadata.
        // Issue a cancel signal to request coordinator.
        // This is where synchronization might be necessary.
        // If we didn't stop ourselves, that likely means there is more work to do.
        return true;
    }
}
