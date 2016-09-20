// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.instantapps;

import android.content.Intent;
import android.graphics.Bitmap;

/**
 * Encapsulates information needed to display an {@link InstantAppsInfoBar}.
 */
public class InstantAppsBannerData {
    private String mAppName;
    private Bitmap mAppIcon;
    private String mHostname;
    private Intent mIntent;

    public InstantAppsBannerData(String appName, Bitmap icon, String hostname, Intent intent) {
        mAppName = appName;
        mAppIcon = icon;
        mHostname = hostname;
        mIntent = intent;
    }

    /** @return The name of the Instant App. */
    public String getAppName() {
        return mAppName;
    }

    /** @return The badged Instant App icon. */
    public Bitmap getIcon() {
        return mAppIcon;
    }

    /** @return The host name for the URL. */
    public String getHostname() {
        return mHostname;
    }

    /** @return The intent to launch on "Open App" button click. */
    public Intent getIntent() {
        return mIntent;
    }
}
