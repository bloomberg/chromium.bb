// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.Context;
import android.os.AsyncTask;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeVersionInfo;

/**
 * This class provides the basic interface to the Physical Web feature.
 */
public class PhysicalWeb {
    /**
     * Evaluate whether the environment is one in which the Physical Web should
     * be enabled.
     * @return true if the PhysicalWeb should be enabled
     */
    public static boolean featureIsEnabled() {
        boolean allowedChannel =
                ChromeVersionInfo.isLocalBuild() || ChromeVersionInfo.isDevBuild();
        boolean switchEnabled =
                CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_PHYSICAL_WEB);
        return allowedChannel && switchEnabled;
    }

    /**
     * Start the Physical Web feature.
     * At the moment, this only enables URL discovery over BLE.
     * @param application An instance of {@link ChromeApplication}, used to get the
     * appropriate PhysicalWebBleClient implementation.
     */
    public static void startPhysicalWeb(ChromeApplication application) {
        PhysicalWebBleClient physicalWebBleClient = PhysicalWebBleClient.getInstance(application);
        physicalWebBleClient.subscribe();
        clearUrlsAsync(application);
    }

    /**
     * Stop the Physical Web feature.
     * @param application An instance of {@link ChromeApplication}, used to get the
     * appropriate PhysicalWebBleClient implementation.
     */
    public static void stopPhysicalWeb(ChromeApplication application) {
        PhysicalWebBleClient physicalWebBleClient = PhysicalWebBleClient.getInstance(application);
        physicalWebBleClient.unsubscribe();
        clearUrlsAsync(application);
    }

    private static void clearUrlsAsync(final Context context) {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                UrlManager.getInstance(context).clearUrls();
            }
        };
        AsyncTask.THREAD_POOL_EXECUTOR.execute(task);
    }
}
