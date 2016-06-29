// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.common;

/**
 * Stores WebAPK related constants.
 */
public final class WebApkConstants {
    public static final String WEBAPK_PACKAGE_PREFIX = "org.chromium.webapk";

    // WebAPK id prefix. The id is used for storing WebAPK data in Chrome's SharedPreferences.
    public static final String WEBAPK_ID_PREFIX = "webapk:";

    // Used for sending Android Manifest properties to WebappLauncherActivity.
    public static final String EXTRA_WEBAPK_DISPLAY_MODE =
            "org.chromium.webapk.lib.common.webapk_display_mode";
    public static final String EXTRA_WEBAPK_ORIENTATION =
            "org.chromium.webapk.lib.common.webapk_orientation";
}
