// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import org.chromium.base.CommandLine;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.BrowserStartupController;

/**
 * Wrapper for the steps needed to initialize the java and native sides of webview chromium.
 */
public abstract class AwBrowserProcess {
    public static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "webview";

    private static final String TAG = "AwBrowserProcess";

    /**
     * Loads the native library, and performs basic static construction of objects needed
     * to run webview in this process. Does not create threads; safe to call from zygote.
     * Note: it is up to the caller to ensure this is only called once.
     */
    public static void loadLibrary(Context context) {
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX, context);
        try {
            LibraryLoader libraryLoader = LibraryLoader.get(LibraryProcessType.PROCESS_WEBVIEW);
            libraryLoader.loadNow(context);
            // Switch the command line implementation from Java to native.
            // It's okay for the WebView to do this before initialization because we have
            // setup the JNI bindings by this point.
            libraryLoader.switchCommandLineForWebView();
        } catch (ProcessInitException e) {
            throw new RuntimeException("Cannot load WebView", e);
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
                    BrowserStartupController.get(context, LibraryProcessType.PROCESS_WEBVIEW)
                            .startBrowserProcessesSync(!CommandLine.getInstance().hasSwitch(
                                            AwSwitches.WEBVIEW_SANDBOXED_RENDERER));
                } catch (ProcessInitException e) {
                    throw new RuntimeException("Cannot initialize WebView", e);
                }
            }
        });
    }
}
