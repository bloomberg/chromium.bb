// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import org.chromium.base.JNINamespace;

import java.util.ArrayList;
import java.util.List;

/**
 * Used for recording metrics about Chrome launches.
 */
@JNINamespace("metrics")
public class LaunchMetrics {
    private static final List<String> sActivityUrls = new ArrayList<String>();
    private static final List<String> sTabUrls = new ArrayList<String>();

    /**
     * Records the launch of a standalone Activity for a URL (i.e. a WebappActivity).
     * @param url URL that kicked off the Activity's creation.
     */
    public static void recordHomeScreenLaunchIntoStandaloneActivity(String url) {
        sActivityUrls.add(url);
    }

    /**
     * Records the launch of a Tab for a URL (i.e. a Home screen shortcut).
     * @param url URL that kicked off the Tab's creation.
     */
    public static void recordHomeScreenLaunchIntoTab(String url) {
        sTabUrls.add(url);
    }

    /**
     * Calls out to native code to record URLs that have been launched via the Home screen.
     * This intermediate step is necessary because Activity.onCreate() may be called when
     * the native library has not yet been loaded.
     */
    public static void commitLaunchMetrics() {
        for (String url : sActivityUrls) {
            nativeRecordLaunch(true, url);
        }
        for (String url : sTabUrls) {
            nativeRecordLaunch(false, url);
        }
        sActivityUrls.clear();
        sTabUrls.clear();
    }

    private static native void nativeRecordLaunch(boolean standalone, String url);
}
