// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.util.Log;

import org.chromium.base.ActivityStatus;
import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.NativeClassQualifiedName;

import android.os.Handler;
import android.os.Looper;

/**
 * Used by the SimpleIndex in net/disk_cache/simple/ to listen to changes in the android app state
 * such as the app going to the background or foreground.
 */
public class SimpleCacheActivityStatusNotifier implements ActivityStatus.StateListener {
    private int mNativePtr = 0;
    private final Looper mIoLooper;

    @CalledByNative
    public static SimpleCacheActivityStatusNotifier newInstance(int nativePtr) {
        return new SimpleCacheActivityStatusNotifier(nativePtr);
    }

    private SimpleCacheActivityStatusNotifier(int nativePtr) {
        this.mIoLooper = Looper.myLooper();
        this.mNativePtr = nativePtr;
        // Call the singleton's ActivityStatus in the UI thread.
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                ActivityStatus.registerStateListener(SimpleCacheActivityStatusNotifier.this);
            }
        });
    }

    @CalledByNative
    public void prepareToBeDestroyed() {
        this.mNativePtr = 0;
        // Call the singleton's ActivityStatus in the UI thread.
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                ActivityStatus.unregisterStateListener(SimpleCacheActivityStatusNotifier.this);
            }
        });
    }

    // ActivityStatus.StateListener
    @Override
    public void onActivityStateChange(final int state) {
        if (state == ActivityStatus.RESUMED ||
            state == ActivityStatus.STOPPED) {
            new Handler(mIoLooper).post(new Runnable() {
                @Override
                public void run() {
                    if (SimpleCacheActivityStatusNotifier.this.mNativePtr != 0)
                        nativeNotifyActivityStatusChanged(
                            SimpleCacheActivityStatusNotifier.this.mNativePtr, state);
                }
            });
        }
    }

    @NativeClassQualifiedName("net::SimpleCacheActivityStatusNotifier")
    private native void nativeNotifyActivityStatusChanged(int nativePtr, int newActivityStatus);
}
