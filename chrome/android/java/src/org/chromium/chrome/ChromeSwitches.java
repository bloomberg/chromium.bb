// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome;

/**
 * Contains all of the command line switches that are specific to the chrome/
 * portion of Chromium on Android.
 */
public abstract class ChromeSwitches {
    // Disables the new Website Settings dialog, which replaces the old one.
    // TODO(sashab): Once the new WebsiteSettingsPopup is ready to be permanent,
    // remove this flag and delete WebsiteSettingsLegacyPopup and all it's
    // dependencies.
    public static final String DISABLE_NEW_WEBSITE_SETTINGS = "disable-new-website-settings";

    /** Mimic a low end device */
    public static final String ENABLE_ACCESSIBILITY_TAB_SWITCHER =
            "enable-accessibility-tab-switcher";

    /** Whether fullscreen support is disabled (auto hiding controls, etc...). */
    public static final String DISABLE_FULLSCREEN = "disable-fullscreen";

    /** Show the undo bar for high end UI devices. */
    public static final String ENABLE_HIGH_END_UI_UNDO = "enable-high-end-ui-undo";

    /** Enable toolbar swipe to change tabs in document mode */
    public static final String ENABLE_TOOLBAR_SWIPE_IN_DOCUMENT_MODE =
            "enable-toolbar-swipe-in-document-mode";

    // Prevent instantiation.
    private ChromeSwitches() {}
}
