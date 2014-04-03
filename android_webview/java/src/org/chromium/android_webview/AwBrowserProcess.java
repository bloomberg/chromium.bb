// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.os.Build;
import android.util.Log;

import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.BrowserStartupController;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Wrapper for the steps needed to initialize the java and native sides of webview chromium.
 */
public abstract class AwBrowserProcess {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "webview";
    private static final String TAG = "AwBrowserProcess";

    /**
     * Loads the native library, and performs basic static construction of objects needed
     * to run webview in this process. Does not create threads; safe to call from zygote.
     * Note: it is up to the caller to ensure this is only called once.
     */
    public static void loadLibrary() {
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        try {
            LibraryLoader.loadNow();
            initTraceEvent();
        } catch (ProcessInitException e) {
            throw new RuntimeException("Cannot load WebView", e);
        }
    }

    // TODO(benm): Move this function into WebView code in Android tree to avoid reflection.
    private static void initTraceEvent() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN) return;

        try {
            final Class<?> traceClass = Class.forName("android.os.Trace");
            final long traceTagView = traceClass.getField("TRACE_TAG_WEBVIEW").getLong(null);

            final Class<?> systemPropertiesClass = Class.forName("android.os.SystemProperties");
            final Method systemPropertiesGetLongMethod = systemPropertiesClass.getDeclaredMethod(
                    "getLong", String.class, Long.TYPE);
            final Method addChangeCallbackMethod = systemPropertiesClass.getDeclaredMethod(
                    "addChangeCallback", Runnable.class);

            // Won't reach here if any of the above reflect lookups fail.
            addChangeCallbackMethod.invoke(null, new Runnable() {
                @Override
                public void run() {
                    try {
                        long enabledFlags = (Long) systemPropertiesGetLongMethod.invoke(
                                null, "debug.atrace.tags.enableflags", 0);
                        TraceEvent.setATraceEnabled((enabledFlags & traceTagView) != 0);
                    } catch (IllegalArgumentException e) {
                        Log.e(TAG, "systemPropertyChanged", e);
                    } catch (IllegalAccessException e) {
                        Log.e(TAG, "systemPropertyChanged", e);
                    } catch (InvocationTargetException e) {
                        Log.e(TAG, "systemPropertyChanged", e);
                    }
                }
            });
        } catch (ClassNotFoundException e) {
            Log.e(TAG, "initTraceEvent", e);
        } catch (NoSuchMethodException e) {
            Log.e(TAG, "initTraceEvent", e);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "initTraceEvent", e);
        } catch (IllegalAccessException e) {
            Log.e(TAG, "initTraceEvent", e);
        } catch (InvocationTargetException e) {
            Log.e(TAG, "initTraceEvent", e);
        } catch (NoSuchFieldException e) {
            Log.e(TAG, "initTraceEvent", e);
        }
    }

    /**
     * Starts the chromium browser process running within this process. Creates threads
     * and performs other per-app resource allocations; must not be called from zygote.
     * Note: it is up to the caller to ensure this is only called once.
     * @param context The Android application context
     */
    public static void start(final Context context) {
        // We must post to the UI thread to cover the case that the user
        // has invoked Chromium startup by using the (thread-safe)
        // CookieManager rather than creating a WebView.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    BrowserStartupController.get(context).startBrowserProcessesSync(
                                BrowserStartupController.MAX_RENDERERS_SINGLE_PROCESS);
                } catch (ProcessInitException e) {
                    throw new RuntimeException("Cannot initialize WebView", e);
                }
            }
        });
    }
}
