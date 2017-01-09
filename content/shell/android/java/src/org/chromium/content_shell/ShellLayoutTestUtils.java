// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.test.util.UrlUtils;

/**
 * Utility methods used by content_shell for running Blink's layout tests on Android.
 */
@JNINamespace("content")
class ShellLayoutTestUtils {
    /**
     * @return The directory in which test data is stored.
     */
    @CalledByNative
    private static String getIsolatedTestRoot() {
        return UrlUtils.getIsolatedTestRoot();
    }
}
