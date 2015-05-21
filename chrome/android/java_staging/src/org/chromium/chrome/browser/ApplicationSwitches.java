// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

/**
 * Contains all of the command line switches that are specific to the Chrome Android java
 * application.
 *
 * See {@link org.chromium.chrome.ChromeSwitches}.
 */
public abstract class ApplicationSwitches {
    /**
     * Disables the new icon-centric NTP design.
     */
    public static final String DISABLE_ICON_NTP = "disable-icon-ntp";

    /**
     * Enables the new icon-centric NTP design.
     */
    public static final String ENABLE_ICON_NTP = "enable-icon-ntp";

    /**
     * Disable bottom infobar-like Reader Mode panel.
     */
    public static final String DISABLE_READER_MODE_BOTTOM_BAR = "disable-reader-mode-bottom-bar";

    /**
     * Enable Reader Mode button.
     */
    public static final String ENABLE_READER_MODE_BUTTON = "enable-reader-mode-toolbar-icon";

    // Prevent instantiation.
    private ApplicationSwitches() {}
}
