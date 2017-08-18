// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.view.Choreographer;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Helper class for interfacing with the Android Choreographer from native code.
 */
@JNINamespace("vr_shell")
public class AndroidVSyncHelper {
    private final long mNativeAndroidVSyncHelper;

    private final Choreographer.FrameCallback mCallback = new Choreographer.FrameCallback() {
        @Override
        public void doFrame(long frameTimeNanos) {
            if (mNativeAndroidVSyncHelper == 0) return;
            nativeOnVSync(mNativeAndroidVSyncHelper, frameTimeNanos);
        }
    };

    @CalledByNative
    private static AndroidVSyncHelper create(long nativeAndroidVSyncHelper) {
        return new AndroidVSyncHelper(nativeAndroidVSyncHelper);
    }

    private AndroidVSyncHelper(long nativeAndroidVSyncHelper) {
        mNativeAndroidVSyncHelper = nativeAndroidVSyncHelper;
    }

    @CalledByNative
    private void requestVSync() {
        Choreographer.getInstance().postFrameCallback(mCallback);
    }

    @CalledByNative
    private void cancelVSyncRequest() {
        Choreographer.getInstance().removeFrameCallback(mCallback);
    }

    private native void nativeOnVSync(long nativeAndroidVSyncHelper, long frameTimeNanos);
}
