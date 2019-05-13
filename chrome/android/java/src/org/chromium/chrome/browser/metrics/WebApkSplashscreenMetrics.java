// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import org.chromium.chrome.browser.webapps.SplashscreenObserver;

/**
 * This class records cold start WebApk splashscreen metrics starting from the launch of the WebAPK
 * shell.
 */
public class WebApkSplashscreenMetrics implements SplashscreenObserver {
    private final long mShellApkLaunchTimeMs;

    public WebApkSplashscreenMetrics(long shellApkLaunchTimeMs) {
        mShellApkLaunchTimeMs = shellApkLaunchTimeMs;
    }

    @Override
    public void onTranslucencyRemoved() {}

    @Override
    public void onSplashscreenHidden(long startTimestamp, long endTimestamp) {
        if (mShellApkLaunchTimeMs == -1) return;

        if (UmaUtils.hasComeToForeground() && !UmaUtils.hasComeToBackground()) {
            // commit both shown/hidden histograms here because native may not be loaded when the
            // splashscreen is shown.
            WebApkUma.recordShellApkLaunchToSplashscreenVisible(
                    startTimestamp - mShellApkLaunchTimeMs);
            WebApkUma.recordShellApkLaunchToSplashscreenHidden(
                    endTimestamp - mShellApkLaunchTimeMs);
        }
    }
}
