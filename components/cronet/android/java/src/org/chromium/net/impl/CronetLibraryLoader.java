// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.net.NetworkChangeNotifier;

/**
 * CronetLibraryLoader loads and initializes native library on init thread.
 */
@JNINamespace("cronet")
@VisibleForTesting
public class CronetLibraryLoader {
    // Synchronize initialization.
    private static final Object sLoadLock = new Object();
    private static final String LIBRARY_NAME = "cronet." + ImplVersion.getCronetVersion();
    private static final String TAG = CronetLibraryLoader.class.getSimpleName();
    // Thread used for initialization work and processing callbacks for
    // long-lived global singletons. This thread lives forever as things like
    // the global singleton NetworkChangeNotifier live on it and are never killed.
    private static final HandlerThread sInitThread = new HandlerThread("CronetInit");
    // Has library loading commenced?  Setting guarded by sLoadLock.
    private static volatile boolean sLibraryLoaded = false;
    // Has ensureInitThreadInitialized() completed?
    private static volatile boolean sInitThreadInitDone = false;

    /**
     * Ensure that native library is loaded and initialized. Can be called from
     * any thread, the load and initialization is performed on init thread.
     */
    public static void ensureInitialized(
            final Context applicationContext, final CronetEngineBuilderImpl builder) {
        synchronized (sLoadLock) {
            if (!sLibraryLoaded) {
                ContextUtils.initApplicationContext(applicationContext);
                if (builder.libraryLoader() != null) {
                    builder.libraryLoader().loadLibrary(LIBRARY_NAME);
                } else {
                    System.loadLibrary(LIBRARY_NAME);
                }
                ContextUtils.initApplicationContextForNative();
                String implVersion = ImplVersion.getCronetVersion();
                if (!implVersion.equals(nativeGetCronetVersion())) {
                    throw new RuntimeException(String.format("Expected Cronet version number %s, "
                                    + "actual version number %s.",
                            implVersion, nativeGetCronetVersion()));
                }
                Log.i(TAG, "Cronet version: %s, arch: %s", implVersion,
                        System.getProperty("os.arch"));
                sLibraryLoaded = true;
            }

            if (!sInitThreadInitDone) {
                if (!sInitThread.isAlive()) {
                    sInitThread.start();
                }
                postToInitThread(new Runnable() {
                    @Override
                    public void run() {
                        ensureInitializedOnInitThread(applicationContext);
                    }
                });
            }
        }
    }

    /**
     * Returns {@code true} if running on the initialization thread.
     */
    private static boolean onInitThread() {
        return sInitThread.getLooper() == Looper.myLooper();
    }

    /**
     * Ensure that the init thread initialization has completed. Can only be called from
     * the init thread. Ensures that the NetworkChangeNotifier is initialzied and the
     * init thread native MessageLoop is initialized.
     */
    static void ensureInitializedOnInitThread(Context context) {
        assert sLibraryLoaded;
        assert onInitThread();
        if (sInitThreadInitDone) {
            return;
        }
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
        nativeCronetInitOnInitThread();
        sInitThreadInitDone = true;
    }

    /**
     * Run {@code r} on the initialization thread.
     */
    public static void postToInitThread(Runnable r) {
        if (onInitThread()) {
            r.run();
        } else {
            new Handler(sInitThread.getLooper()).post(r);
        }
    }

    // Native methods are implemented in cronet_library_loader.cc.
    private static native void nativeCronetInitOnInitThread();
    private static native String nativeGetCronetVersion();
}
