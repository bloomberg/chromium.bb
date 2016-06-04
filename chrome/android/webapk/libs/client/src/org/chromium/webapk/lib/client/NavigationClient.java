// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.client;

import android.content.Context;
import android.content.Intent;

/**
 * NavigationClient provides an API to launch a WebAPK.
 */
public class NavigationClient {
    /**
     * Launches a WebAPK.
     * @param context Application context.
     * @param webApkPackageName Package name of the WebAPK to launch.
     * @param url URL to navigate WebAPK to.
     */
    public static void launchWebApk(Context context, String webApkPackageName, String url) {
        Intent intent;
        try {
            intent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
        } catch (Exception e) {
            return;
        }

        intent.setPackage(webApkPackageName);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }
}
