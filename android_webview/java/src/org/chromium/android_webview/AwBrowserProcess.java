// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.content.app.LibraryLoader;
import org.chromium.content.browser.AndroidBrowserProcess;
import org.chromium.content.browser.ResourceExtractor;

/**
 * Wrapper for the steps needed to initialize the java and native sides of webview chromium.
 */
public abstract class AwBrowserProcess {
    /**
     * The name of the library to load.
     */
    private static final String NATIVE_LIBRARY = "webviewchromium";

    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "webview";

    /**
     * Loads the native library, and performs basic static construction of objects needed
     * to run webview in this process. Does not create threads; safe to call from zygote.
     * Note: it is up to the caller to ensure this is only called once.
     */
    public static void loadLibrary() {
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        LibraryLoader.setLibraryToLoad(NATIVE_LIBRARY);
        LibraryLoader.loadNow();
    }

    /**
     * Starts the chromium browser process running within this process. Creates threads
     * and performs other per-app resource allocations; must not be called from zygote.
     * Note: it is up to the caller to ensure this is only called once.
     */
    public static void start(final Context context) {
        // We must post to the UI thread to cover the case that the user
        // has invoked Chromium startup by using the (thread-safe)
        // CookieManager rather than creating a WebView.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                LibraryLoader.ensureInitialized();
                AndroidBrowserProcess.initContentViewProcess(context,
                        AndroidBrowserProcess.MAX_RENDERERS_SINGLE_PROCESS);
            }
        });
    }
}
