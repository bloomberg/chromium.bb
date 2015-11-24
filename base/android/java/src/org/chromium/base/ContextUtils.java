// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * This class provides Android Context utility methods.
 */
@JNINamespace("base::android")
public class ContextUtils {
    private static Context sApplicationContext;

    /**
     * Get the Android application context.
     *
     * Under normal circumstances there is only one application context in a process, so it's safe
     * to treat this as a global. In WebView it's possible for more than one app using WebView to be
     * running in a single process, but this mechanism is rarely used and this is not the only
     * problem in that scenario, so we don't currently forbid using it as a global.
     *
     * Do not downcast the context returned by this method to Application (or any subclass). It may
     * not be an Application object; it may be wrapped in a ContextWrapper. The only assumption you
     * may make is that it is a Context whose lifetime is the same as the lifetime of the process.
     */
    public static Context getApplicationContext() {
        assert sApplicationContext != null;
        return sApplicationContext;
    }

    /**
     * Initialize the Android application context.
     *
     * Either this or the native equivalent base::android::InitApplicationContext must be called
     * once during startup. JNI bindings must have been initialized, as the context is stored on
     * both sides.
     */
    public static void initApplicationContext(Context appContext) {
        assert appContext != null;
        assert sApplicationContext == null || sApplicationContext == appContext;
        initJavaSideApplicationContext(appContext);
        nativeInitNativeSideApplicationContext(appContext);
    }

    /**
     * JUnit Robolectric tests run without native code; allow them to set just the Java-side
     * context. Do not use in configurations that actually run on Android!
     */
    public static void initApplicationContextForJUnitTests(Context appContext) {
        initJavaSideApplicationContext(appContext);
    }

    @CalledByNative
    private static void initJavaSideApplicationContext(Context appContext) {
        sApplicationContext = appContext;
    }

    private static native void nativeInitNativeSideApplicationContext(Context appContext);
}
