// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;

/**
 * CronetLibraryLoader loads and initializes native library on main thread.
 */
@JNINamespace("cronet")
class CronetLibraryLoader {
    /**
     * Synchronize access to sInitTaskPosted and initialization routine.
     */
    private static final Object sLoadLock = new Object();
    private static boolean sInitTaskPosted = false;

    /**
     * Ensure that native library is loaded and initialized. Can be called from
     * any thread, the load and initialization is performed on main thread.
     */
    public static void ensureInitialized(
            final Context context, final CronetEngine.Builder builder) {
        synchronized (sLoadLock) {
            if (sInitTaskPosted) {
                return;
            }
            System.loadLibrary(builder.libraryName());
            if (!Version.CRONET_VERSION.equals(nativeGetCronetVersion())) {
                throw new RuntimeException(String.format(
                      "Expected Cronet version number %s, "
                              + "actual version number %s.",
                      Version.CRONET_VERSION,
                      nativeGetCronetVersion()));
            }
            ContextUtils.initApplicationContext(context.getApplicationContext());
            // Init native Chromium CronetEngine on Main UI thread.
            Runnable task = new Runnable() {
                public void run() {
                    initOnMainThread(context);
                }
            };
            // Run task immediately or post it to the UI thread.
            if (Looper.getMainLooper() == Looper.myLooper()) {
                task.run();
            } else {
                // The initOnMainThread will complete on the main thread prior
                // to other tasks posted to the main thread.
                new Handler(Looper.getMainLooper()).post(task);
            }
            sInitTaskPosted = true;
        }
    }

    private static void initOnMainThread(final Context context) {
        NetworkChangeNotifier.init(context);
        // Registers to always receive network notifications. Note
        // that this call is fine for Cronet because Cronet
        // embedders do not have API access to create network change
        // observers. Existing observers in the net stack do not
        // perform expensive work.
        NetworkChangeNotifier.registerToReceiveNotificationsAlways();
        // registerToReceiveNotificationsAlways() is called before the native
        // NetworkChangeNotifierAndroid is created, so as to avoid receiving
        // the undesired initial network change observer notification, which
        // will cause active requests to fail with ERR_NETWORK_CHANGED.
        nativeCronetInitOnMainThread();
    }

    // Native methods are implemented in cronet_loader.cc.
    private static native void nativeCronetInitOnMainThread();
    private static native String nativeGetCronetVersion();
}
