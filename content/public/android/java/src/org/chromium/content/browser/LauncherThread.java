// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Handler;

import org.chromium.base.JavaHandlerThread;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

@JNINamespace("content::android")
final class LauncherThread {
    private static final JavaHandlerThread sThread =
            new JavaHandlerThread("Chrome_ProcessLauncherThread");
    private static final Handler sHandler;
    static {
        sThread.maybeStart();
        sHandler = new Handler(sThread.getLooper());
    }

    static void post(Runnable r) {
        sHandler.post(r);
    }

    @CalledByNative
    private static JavaHandlerThread getHandlerThread() {
        return sThread;
    }

    private LauncherThread() {}
}
