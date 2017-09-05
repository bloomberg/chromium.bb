// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.vr;

import android.content.Context;
import android.os.StrictMode;
import android.view.Display;
import android.view.WindowManager;

import com.google.vr.cardboard.DisplaySynchronizer;
import com.google.vr.ndk.base.GvrApi;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Creates an active GvrContext from a GvrApi created from the Application Context. This GvrContext
 * cannot be used for VR rendering, and should only be used to query pose information and device
 * parameters.
 */
@JNINamespace("device")
public class NonPresentingGvrContext {
    private GvrApi mGvrApi;

    private NonPresentingGvrContext() {
        Context context = ContextUtils.getApplicationContext();
        WindowManager windowManager =
                (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();
        DisplaySynchronizer synchronizer = new DisplaySynchronizer(context, display);

        // Creating the GvrApi can sometimes create the Daydream config file.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            mGvrApi = new GvrApi(context, synchronizer);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @CalledByNative
    private static NonPresentingGvrContext create() {
        try {
            return new NonPresentingGvrContext();
        } catch (IllegalStateException | UnsatisfiedLinkError e) {
            return null;
        }
    }

    @CalledByNative
    private long getNativeGvrContext() {
        return mGvrApi.getNativeGvrContext();
    }

    @CalledByNative
    private void shutdown() {
        mGvrApi.shutdown();
    }
}
