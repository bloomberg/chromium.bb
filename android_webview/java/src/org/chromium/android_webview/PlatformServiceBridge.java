// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;

import java.lang.reflect.InvocationTargetException;

/**
 * This class manages platform-specific services. (i.e. Google Services) The platform
 * should extend this class and use this base class to fetch their specialized version.
 */
public class PlatformServiceBridge {
    private static final String TAG = "PlatformServiceBrid-";
    private static final String PLATFORM_SERVICE_BRIDGE =
            "com.android.webview.chromium.PlatformServiceBridgeGoogle";

    private static PlatformServiceBridge sInstance;
    private static final Object sInstanceLock = new Object();

    protected PlatformServiceBridge() {}

    @SuppressWarnings("unused")
    public static PlatformServiceBridge getInstance() {
        synchronized (sInstanceLock) {
            if (sInstance != null) return sInstance;

            // Try to get a specialized service bridge. Starting with Android O, failed reflection
            // may cause file reads. The reflection will go away soon: https://crbug.com/682070
            try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
                Class<?> cls = Class.forName(PLATFORM_SERVICE_BRIDGE);
                sInstance = (PlatformServiceBridge) cls.getDeclaredConstructor().newInstance();
                return sInstance;
            } catch (ClassNotFoundException e) {
                // This is not an error; it just means this device doesn't have specialized
                // services.
            } catch (IllegalAccessException | IllegalArgumentException | InstantiationException
                    | NoSuchMethodException e) {
                Log.e(TAG, "Failed to get " + PLATFORM_SERVICE_BRIDGE + ": " + e);
            } catch (InvocationTargetException e) {
                Log.e(TAG, "Failed invocation to get " + PLATFORM_SERVICE_BRIDGE + ": ",
                        e.getCause());
            }

            // Otherwise, get the generic service bridge.
            sInstance = new PlatformServiceBridge();
            return sInstance;
        }
    }

    // Provide a mocked PlatformServiceBridge for testing.
    public static void injectInstance(PlatformServiceBridge testBridge) {
        // Although reference assignments are atomic, we still wouldn't want to assign it in the
        // middle of getInstance().
        synchronized (sInstanceLock) {
            sInstance = testBridge;
        }
    }

    // Can WebView use Google Play Services (a.k.a. GMS)?
    public boolean canUseGms() {
        return false;
    }

    // Overriding implementations may call "callback" asynchronously. For simplicity (and not
    // because of any technical limitation) we require that "queryMetricsSetting" and "callback"
    // both get called on WebView's UI thread.
    public void queryMetricsSetting(Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();
        callback.onResult(false);
    }

    // Takes an uncompressed, serialized UMA proto and logs it via a platform-specific mechanism.
    public void logMetrics(byte[] data) {}
}
