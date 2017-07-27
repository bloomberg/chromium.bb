// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.webapk.lib.client.WebApkIdentityServiceClient;
import org.chromium.webapk.lib.client.WebApkValidator;

/**
 * Contains functionality needed for Chrome to host WebAPKs.
 */
public class ChromeWebApkHost {
    private static ApplicationStatus.ApplicationStateListener sListener;

    public static void init() {
        WebApkValidator.init(
                ChromeWebApkHostSignature.EXPECTED_SIGNATURE, ChromeWebApkHostSignature.PUBLIC_KEY);
    }

    /* Returns whether launching renderer in WebAPK process is enabled by Chrome. */
    public static boolean canLaunchRendererInWebApkProcess() {
        return LibraryLoader.isInitialized() && nativeCanLaunchRendererInWebApkProcess();
    }

    /**
     * Checks whether Chrome is the runtime host of the WebAPK asynchronously. Accesses the
     * ApplicationStateListener needs to be called on UI thread.
     */
    @SuppressFBWarnings("LI_LAZY_INIT_UPDATE_STATIC")
    public static void checkChromeBacksWebApkAsync(String webApkPackageName,
            WebApkIdentityServiceClient.CheckBrowserBacksWebApkCallback callback) {
        ThreadUtils.assertOnUiThread();

        if (sListener == null) {
            // Registers an application listener to disconnect all connections to WebAPKs
            // when Chrome is stopped.
            sListener = new ApplicationStatus.ApplicationStateListener() {
                @Override
                public void onApplicationStateChange(int newState) {
                    if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES
                            || newState == ApplicationState.HAS_DESTROYED_ACTIVITIES) {
                        WebApkIdentityServiceClient.disconnectAll(
                                ContextUtils.getApplicationContext());
                        WebApkServiceClient.disconnectAll();

                        ApplicationStatus.unregisterApplicationStateListener(sListener);
                        sListener = null;
                    }
                }
            };
            ApplicationStatus.registerApplicationStateListener(sListener);
        }

        WebApkIdentityServiceClient.getInstance().checkBrowserBacksWebApkAsync(
                ContextUtils.getApplicationContext(), webApkPackageName, callback);
    }

    private static native boolean nativeCanLaunchRendererInWebApkProcess();
}
