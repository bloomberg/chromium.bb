// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Passes UMA logs from native to PlatformServiceBridge.
 */
@JNINamespace("android_webview")
public class AwMetricsLogUploader {
    @CalledByNative
    public static void uploadLog(byte[] data) {
        // getInstance only needs a Context on the first call. WebViewChromiumFactoryProvider will
        // have already called it, so we can pass null here.
        PlatformServiceBridge.getInstance(null).logMetrics(data);
    }
}
