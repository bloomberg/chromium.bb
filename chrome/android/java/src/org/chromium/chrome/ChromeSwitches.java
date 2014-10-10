// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome;

/**
 * Contains all of the command line switches that are specific to the chrome/
 * portion of Chromium on Android.
 */
public abstract class ChromeSwitches {
    // Enables the new Website Settings dialog, which replaces the old one.
    // TODO(sashab): Once the new WebsiteSettingsPopup is ready to launch,
    // remove this flag and delete WebsiteSettingsLegacyPopup and all it's
    // dependencies.
    public static final String ENABLE_NEW_WEBSITE_SETTINGS = "enable-new-website-settings";

    // Prevent instantiation.
    private ChromeSwitches() {}
}
