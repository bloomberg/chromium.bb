// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.app.Application;
import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.components.embedder_support.application.FontPreloadingWorkaround;

/**
 * Application subclass for SystemWebView and Trichrome.
 *
 * Application subclass is only used in renderer processes and in the WebView APK's own services.
 * None of this code runs in an application which simply uses WebView.
 */
@JNINamespace("android_webview")
public class WebViewApkApplication extends Application {
    // Called by the framework for ALL processes. Runs before ContentProviders are created.
    // Quirk: context.getApplicationContext() returns null during this method.
    @Override
    protected void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        ContextUtils.initApplicationContext(this);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        FontPreloadingWorkaround.maybeInstallWorkaround(this);
    }

    /**
     * Performs minimal native library initialization required when running as a stand-alone APK.
     * @return True if the library was loaded, false if running as webview stub.
     */
    static synchronized boolean initializeNative() {
        try {
            if (LibraryLoader.getInstance().isInitialized()) {
                return true;
            }
            LibraryLoader.getInstance().loadNow();
        } catch (Throwable unused) {
            // Happens for WebView Stub. Throws NoClassDefFoundError because of no
            // NativeLibraries.java being generated.
            return false;
        }
        LibraryLoader.getInstance().switchCommandLineForWebView();
        nativeInitializePakResources();
        return true;
    }

    private static native void nativeInitializePakResources();
}
