// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;

import java.io.File;

/**
 * Java counterpart to webapk_installer.h
 * Contains functionality to install WebAPKs.
 */
public class WebApkInstaller {
    private static final String TAG = "WebApkInstaller";

    /**
     * Installs a WebAPK.
     * @param filePath File to install.
     * @param packageName Package name to install WebAPK at.
     * @return True if the install was started. A "true" return value does not guarantee that the
     *         install succeeds.
     */
    @CalledByNative
    static boolean installAsyncFromNative(String filePath, String packageName) {
        if (!installingFromUnknownSourcesAllowed()) {
            Log.e(TAG,
                    "WebAPK install failed because installation from unknown sources is disabled.");
            return false;
        }
        if (!new File(filePath).exists()) {
            return false;
        }
        Intent intent = new Intent(Intent.ACTION_VIEW);
        Uri fileUri = Uri.fromFile(new File(filePath));
        intent.setDataAndType(fileUri, "application/vnd.android.package-archive");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            ContextUtils.getApplicationContext().startActivity(intent);
        } catch (ActivityNotFoundException e) {
            return false;
        }
        return true;
    }

    /**
     * Returns whether the user has enabled installing apps from sources other than the Google Play
     * Store.
     */
    private static boolean installingFromUnknownSourcesAllowed() {
        Context context = ContextUtils.getApplicationContext();
        try {
            return Settings.Secure.getInt(
                           context.getContentResolver(), Settings.Secure.INSTALL_NON_MARKET_APPS)
                    == 1;
        } catch (Settings.SettingNotFoundException e) {
            return false;
        }
    }
}
