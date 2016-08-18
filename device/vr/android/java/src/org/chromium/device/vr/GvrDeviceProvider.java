// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.vr;

import android.content.Context;

import com.google.vr.ndk.base.GvrLayout;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * This is the implementation of the C++ counterpart GvrDeviceProvider.
 */
@JNINamespace("device")
class GvrDeviceProvider {
    private static final String TAG = "GvrDeviceProvider";
    private final GvrLayout mLayout;
    private Thread mGvrInitThread;

    private GvrDeviceProvider(Context context) {
        mLayout = new GvrLayout(context);

        // Initialize the GVR API on a separate thread to avoid strict mode
        // violations. Note that this doesn't fix the underlying issue of
        // blocking on disk reads here.
        mGvrInitThread = new Thread(new Runnable() {
            @Override
            public void run() {
                mLayout.getGvrApi();
            }
        });
        mGvrInitThread.start();
    }

    @CalledByNative
    private static GvrDeviceProvider create(Context context) {
        return new GvrDeviceProvider(context);
    }

    @CalledByNative
    private long getNativeContext() {
        try {
            mGvrInitThread.join();
        } catch (Exception ex) {
        }
        return mLayout.getGvrApi().getNativeGvrContext();
    }

    @CalledByNative
    private void shutdown() {
        mLayout.shutdown();
    }
}
