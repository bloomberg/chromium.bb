// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Build;
import android.os.StrictMode;

import org.chromium.base.BuildConfig;
import org.chromium.base.CommandLine;

/**
 * Initialize application-level StrictMode reporting.
 */
public class ChromeStrictMode {
    private static boolean sIsStrictModeAlreadyConfigured = false;

    public static void configureStrictMode() {
        if (sIsStrictModeAlreadyConfigured) {
            return;
        }
        sIsStrictModeAlreadyConfigured = true;
        CommandLine commandLine = CommandLine.getInstance();
        if ("eng".equals(Build.TYPE)
                || BuildConfig.sIsDebug
                || ChromeVersionInfo.isLocalBuild()
                || commandLine.hasSwitch(ChromeSwitches.STRICT_MODE)) {
            StrictMode.enableDefaults();
            StrictMode.ThreadPolicy.Builder threadPolicy =
                    new StrictMode.ThreadPolicy.Builder(StrictMode.getThreadPolicy());
            threadPolicy = threadPolicy.detectAll()
                    .penaltyFlashScreen()
                    .penaltyDeathOnNetwork();
            /*
             * Explicitly enable detection of all violations except file URI leaks, as that results
             * in false positives when file URI intents are passed between Chrome activities in
             * separate processes. See http://crbug.com/508282#c11.
             */
            StrictMode.VmPolicy.Builder vmPolicy = new StrictMode.VmPolicy.Builder();
            vmPolicy = vmPolicy.detectActivityLeaks()
                    .detectLeakedClosableObjects()
                    .detectLeakedRegistrationObjects()
                    .detectLeakedSqlLiteObjects()
                    .penaltyLog();
            if ("death".equals(commandLine.getSwitchValue(ChromeSwitches.STRICT_MODE))) {
                threadPolicy = threadPolicy.penaltyDeath();
                vmPolicy = vmPolicy.penaltyDeath();
            } else if ("testing".equals(commandLine.getSwitchValue(ChromeSwitches.STRICT_MODE))) {
                threadPolicy = threadPolicy.penaltyDeath();
                // Currently VmDeathPolicy kills the process, and is not visible on bot test output.
            }
            StrictMode.setThreadPolicy(threadPolicy.build());
            StrictMode.setVmPolicy(vmPolicy.build());
        }
    }
}
