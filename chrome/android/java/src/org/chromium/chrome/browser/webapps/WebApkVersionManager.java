// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.webapk.lib.client.DexOptimizer;

import java.io.File;

/**
 * Updates installed WebAPKs after a Chrome update.
 */
public class WebApkVersionManager {
    /**
     * Name of the shared preference to store whether an attempt to extract the WebAPK runtime
     * library was made.
     */
    private static final String TRIED_EXTRACTING_DEX_PREF =
            "org.chromium.chrome.browser.webapps.TRIED_EXTRACTING_DEX";

    /**
     * Tries to extract the WebAPK runtime dex from the Chrome APK if it has not tried already.
     * Should not be called on UI thread.
     */
    public static void updateWebApksIfNeeded() {
        assert !ThreadUtils.runningOnUiThread();

        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        if (preferences.getBoolean(TRIED_EXTRACTING_DEX_PREF, false)) {
            return;
        }

        preferences.edit().putBoolean(TRIED_EXTRACTING_DEX_PREF, true).apply();

        Context context = ContextUtils.getApplicationContext();
        File dexDir = context.getDir("dex", Context.MODE_PRIVATE);
        File dexFile = new File(dexDir, "web_apk.dex");
        if (!FileUtils.extractAsset(context, "web_apk.dex", dexFile)
                || !DexOptimizer.optimize(dexFile)) {
            return;
        }

        // Make dex file world-readable so that WebAPK can use it.
        dexFile.setReadable(true, false);
    }
}
