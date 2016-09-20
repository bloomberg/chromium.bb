// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.instantapps.InstantAppsBannerData;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.content_public.browser.WebContents;

import java.net.URI;

/**
 * Delegate for {@link InstantAppsInfoBar}. Use launch() method to display the infobar.
 */
public class InstantAppsInfoBarDelegate {
    private static final String TAG = "IAInfoBarDelegate";

    private InstantAppsBannerData mData;

    public static void launch(WebContents webContents, String label, Bitmap icon, Intent intent) {
        String hostname = "";
        try {
            URI uri = URI.create(webContents.getUrl());
            hostname = uri.getRawAuthority();
        } catch (IllegalArgumentException e) {
            // not able to parse the URL.
        }
        InstantAppsBannerData appsBannerData = new InstantAppsBannerData(
                label, icon, hostname, intent);
        nativeLaunch(webContents, appsBannerData);
    }

    @CalledByNative
    private static InstantAppsInfoBarDelegate create() {
        return new InstantAppsInfoBarDelegate();
    }

    private InstantAppsInfoBarDelegate() {}

    @CalledByNative
    private void openInstantApp(InstantAppsBannerData data) {
        Context appContext = ContextUtils.getApplicationContext();
        try {
            appContext.startActivity(data.getIntent());
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "No activity found to launch intent " + data.getIntent(), e);
        }
        InstantAppsHandler.getInstance((ChromeApplication) appContext)
                .recordDefaultOpen(data.getHostname());
    }

    private static native void nativeLaunch(WebContents webContents, InstantAppsBannerData data);
}
