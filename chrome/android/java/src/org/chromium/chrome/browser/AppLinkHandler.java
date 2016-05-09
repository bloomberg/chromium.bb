// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;

/** App link handler. */
public class AppLinkHandler {

    private static final Object INSTANCE_LOCK = new Object();
    private static AppLinkHandler sInstance;

    public static AppLinkHandler getInstance(ChromeApplication application) {
        synchronized (INSTANCE_LOCK) {
            if (sInstance == null) {
                sInstance = application.createAppLinkHandler();
            }
        }
        return sInstance;
    }

    /** Cache whether the feature is enabled. */
    public void cacheAppLinkEnabled(Context context) {
    }

    /** Handle intent. */
    public boolean handleIntent(Context context, Intent intent, boolean isCustomTabsIntent) {
        return false;
    }

    /** Commit metrics. */
    public void commitMetrics() {
    }

}
