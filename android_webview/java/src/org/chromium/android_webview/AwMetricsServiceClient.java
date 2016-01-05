// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.webkit.ValueCallback;

import org.chromium.base.annotations.JNINamespace;

/**
 * Java twin of the homonymous C++ class. The Java side is only responsible for
 * switching metrics on and off. Since the setting is a platform feature, it
 * must be obtained through PlatformServiceBridge.
 */
@JNINamespace("android_webview")
public class AwMetricsServiceClient {
    public AwMetricsServiceClient(Context applicationContext) {
        PlatformServiceBridge.getInstance(applicationContext)
                .setMetricsSettingListener(new ValueCallback<Boolean>() {
                    public void onReceiveValue(Boolean enabled) {
                        nativeSetMetricsEnabled(enabled);
                    }
                });
    }

    public static native void nativeSetMetricsEnabled(boolean enabled);
}
