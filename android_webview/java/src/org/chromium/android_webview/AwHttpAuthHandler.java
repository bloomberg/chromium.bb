// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

@JNINamespace("android_webview")
public class AwHttpAuthHandler {

    private int mNativeAwHttpAuthHandler;

    public void proceed(String username, String password) {
        nativeProceed(mNativeAwHttpAuthHandler, username, password);
    }

    public void cancel() {
        nativeCancel(mNativeAwHttpAuthHandler);
    }

    @CalledByNative
    public static AwHttpAuthHandler create(int nativeAwAuthHandler) {
        return new AwHttpAuthHandler(nativeAwAuthHandler);
    }

    private AwHttpAuthHandler(int nativeAwHttpAuthHandler) {
        mNativeAwHttpAuthHandler = nativeAwHttpAuthHandler;
    }

    private native void nativeProceed(int nativeAwHttpAuthHandler,
            String username, String password);
    private native void nativeCancel(int nativeAwHttpAuthHandler);
}
