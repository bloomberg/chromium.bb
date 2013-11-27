// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

/**
 * Static methods to record user actions.
 *
 * We have a different native method for each action as we want to use c++ code to
 * extract user command keys from code and show them in the dashboard.
 * See: chromium/src/content/browser/user_metrics.h for more details.
 */
public class UmaBridge {

    /**
     * Record that the user opened the menu.
     */
    public static void menuShow() {
        nativeRecordMenuShow();
    }

    /**
     * Record that the user opened the menu through software menu button.
     * @param isByHwButton
     * @param isDragging
     */
    public static void usingMenu(boolean isByHwButton, boolean isDragging) {
        nativeRecordUsingMenu(isByHwButton, isDragging);
    }

    private static native void nativeRecordMenuShow();
    private static native void nativeRecordUsingMenu(boolean isByHwButton, boolean isDragging);
}
