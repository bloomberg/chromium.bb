// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.vr;

import android.content.Context;
import android.os.StrictMode;

import com.google.vr.ndk.base.GvrLayout;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * This is the implementation of the C++ counterpart GvrDeviceProvider.
 */
@JNINamespace("device")
class GvrDeviceProvider {
    private static final String TAG = "GvrDeviceProvider";
    private final GvrLayout mLayout;

    private GvrDeviceProvider(Context context) {
        mLayout = new GvrLayout(context);
    }

    @CalledByNative
    private static GvrDeviceProvider create(Context context) {
        return new GvrDeviceProvider(context);
    }

    @CalledByNative
    private long getNativeContext() {
        long nativeGvrContext = 0;

        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();

        try {
            nativeGvrContext = mLayout.getGvrApi().getNativeGvrContext();
        } catch (Exception ex) {
            Log.e(TAG, "Unable to instantiate GvrApi", ex);
            return 0;
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        return nativeGvrContext;
    }

    @CalledByNative
    private void shutdown() {
        mLayout.shutdown();
    }
}
