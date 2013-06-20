// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.content.Context;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * Utility methods used by content_shell for running Blink's layout tests on Android.
 */
@JNINamespace("content")
class ShellLayoutTestUtils {
    /**
     * @return The directory in which the test files, for example FIFOs, can be stored.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static String getApplicationFilesDirectory(Context appContext) {
        return appContext.getFilesDir().getAbsolutePath();
    }
}
