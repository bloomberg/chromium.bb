// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.net.NetworkChangeNotifier;

/**
 * This class is intended to observe the lifetime of webviews. It receives
 * a callback when the first webview instance (native aw_content) is created
 * and when the last webview instance is destroyed.
 */
@JNINamespace("android_webview")
public class AwWebViewLifecycleObserver {

    private static final String TAG = "AwWebViewLifecycle";
    // Assume there is ACCESS_NETWORK_STATE permission unless told otherwise.
    private static boolean sHasNetworkStatePermission = true;

    // Called on UI thread.
    @CalledByNative
    private static void onFirstWebViewCreated(Context appContext) {
        ThreadUtils.assertOnUiThread();

        if (sHasNetworkStatePermission) {
            try {
                if (!NetworkChangeNotifier.isInitialized()) {
                    NetworkChangeNotifier.init(appContext);
                }
                NetworkChangeNotifier.registerToReceiveNotificationsAlways();
            } catch (SecurityException e) {
                // Cannot enable network information api. The application does not have the
                // ACCESS_NETWORK_STATE permission.
                sHasNetworkStatePermission = false;
            }
        }
    }

    // Called on UI thread.
    @CalledByNative
    private static void onLastWebViewDestroyed(Context appContext) {
        ThreadUtils.assertOnUiThread();

        if (sHasNetworkStatePermission && NetworkChangeNotifier.isInitialized()) {
            // Force unregister for network change broadcasts.
            NetworkChangeNotifier.setAutoDetectConnectivityState(false);
        }
    }

    @VisibleForTesting
    public static void setHasNetworkStatePermission(boolean allow) {
        sHasNetworkStatePermission = allow;
    }
}
