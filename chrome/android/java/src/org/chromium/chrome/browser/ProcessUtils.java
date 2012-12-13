// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

/**
 * A set of utility methods related to the various Chrome processes.
 * TODO(pliard): Remove this class when JavaScript timers toggling is handled directly on the native
 * side by using the system monitor.
 */
public class ProcessUtils {
    // To prevent this class from being instantiated.
    private ProcessUtils() {
    }

    /**
     * Suspends Webkit timers in all renderers.
     *
     * @param suspend true if timers should be suspended.
     */
    public static void toggleWebKitSharedTimers(boolean suspend) {
        nativeToggleWebKitSharedTimers(suspend);
    }

    private static native void nativeToggleWebKitSharedTimers(boolean suspend);
}
