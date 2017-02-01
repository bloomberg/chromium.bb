// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Centralizes UMA data collection for WebAPKs. NOTE: Histogram names and values are defined in
 * tools/metrics/histograms/histograms.xml. Please update that file if any change is made.
 */
public class WebApkUma {
    // This enum is used to back UMA histograms, and should therefore be treated as append-only.
    public static final int UPDATE_REQUEST_SENT_FIRST_TRY = 0;
    public static final int UPDATE_REQUEST_SENT_ONSTOP = 1;
    public static final int UPDATE_REQUEST_SENT_WHILE_WEBAPK_IN_FOREGROUND = 2;
    public static final int UPDATE_REQUEST_SENT_MAX = 3;

    // This enum is used to back UMA histograms, and should therefore be treated as append-only.
    // The queued request times shouldn't exceed three.
    public static final int UPDATE_REQUEST_QUEUED_ONCE = 0;
    public static final int UPDATE_REQUEST_QUEUED_TWICE = 1;
    public static final int UPDATE_REQUEST_QUEUED_THREE_TIMES = 2;
    public static final int UPDATE_REQUEST_QUEUED_MAX = 3;

    public static final String HISTOGRAM_UPDATE_REQUEST_SENT =
            "WebApk.Update.RequestSent";

    public static final String HISTOGRAM_UPDATE_REQUEST_QUEUED = "WebApk.Update.RequestQueued";

    /**
     * Records the time point when a request to update a WebAPK is sent to the WebAPK Server.
     * @param type representing when the update request is sent to the WebAPK server.
     */
    public static void recordUpdateRequestSent(int type) {
        assert type >= 0 && type < UPDATE_REQUEST_SENT_MAX;
        RecordHistogram.recordEnumeratedHistogram(HISTOGRAM_UPDATE_REQUEST_SENT,
                type, UPDATE_REQUEST_SENT_MAX);
    }

    /**
     * Records the times that an update request has been queued once, twice and three times before
     * sending to WebAPK server.
     * @param times representing the times that an update has been queued.
     */
    public static void recordUpdateRequestQueued(int times) {
        RecordHistogram.recordEnumeratedHistogram(HISTOGRAM_UPDATE_REQUEST_QUEUED, times,
                UPDATE_REQUEST_QUEUED_MAX);
    }
}
