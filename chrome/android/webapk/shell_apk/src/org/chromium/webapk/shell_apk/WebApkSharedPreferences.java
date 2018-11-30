// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.SharedPreferences;

/** Contains utility methods and constants related to WebAPK shared preferences. */
public final class WebApkSharedPreferences {
    /** Name of the shared preferences file. */
    private static final String PREF_PACKAGE = "org.chromium.webapk.shell_apk";

    /** Shared preference for the selected host browser. */
    public static final String PREF_RUNTIME_HOST = "runtime_host";

    /** Shared preferenec for the host browser's version code. */
    public static final String PREF_REMOTE_VERSION_CODE =
            "org.chromium.webapk.shell_apk.version_code";

    /**
     * Shared preference for the version number of the dynamically loaded dex by the WebAPK
     * service.
     */
    public static final String PREF_RUNTIME_DEX_VERSION =
            "org.chromium.webapk.shell_apk.dex_version";

    /** Timestamp of when the WebAPK asked the host browser to relaunch the WebAPK. */
    public static final String SHARED_PREF_REQUEST_HOST_BROWSER_RELAUNCH_TIMESTAMP =
            "org.chromium.webapk.shell_apk.request_host_browser_relaunch_timestamp";

    public static SharedPreferences getPrefs(Context context) {
        return context.getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE);
    }
}
