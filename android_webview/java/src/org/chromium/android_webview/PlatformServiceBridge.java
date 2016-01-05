// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.webkit.ValueCallback;

import org.chromium.base.Log;

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

    protected PlatformServiceBridge() {}

    public static PlatformServiceBridge getInstance(Context applicationContext) {
        if (sInstance != null) {
            return sInstance;
        }

        // Try to get a specialized service bridge.
        try {
            Class<?> cls = Class.forName(PLATFORM_SERVICE_BRIDGE);
            sInstance = (PlatformServiceBridge) cls.getDeclaredConstructor(Context.class)
                    .newInstance(applicationContext);
            return sInstance;
        } catch (ClassNotFoundException e) {
            // This is not an error; it just means this device doesn't have specialized services.
        } catch (IllegalAccessException | IllegalArgumentException | InstantiationException
                | NoSuchMethodException e) {
            Log.e(TAG, "Failed to get " + PLATFORM_SERVICE_BRIDGE + ": " + e);
        } catch (InvocationTargetException e) {
            Log.e(TAG, "Failed invocation to get " + PLATFORM_SERVICE_BRIDGE + ":", e.getCause());
        }

        // Otherwise, get the generic service bridge.
        sInstance = new PlatformServiceBridge();
        return sInstance;
    }

    public void setMetricsSettingListener(ValueCallback<Boolean> callback) {}
}
