// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.graphics.Bitmap;
import android.widget.TextView;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.banners.AppData;

/**
 * Infobar informing the user about an app related to this page.
 */
public class AppBannerInfoBar extends ConfirmInfoBar {
    private final String mAppTitle;

    // Data for native app installs.
    private final AppData mAppData;

    // Data for web app installs.
    private final String mAppUrl;

    // Banner for native apps.
    private AppBannerInfoBar(long nativeInfoBar, String appTitle, Bitmap iconBitmap, AppData data) {
        super(nativeInfoBar, null, 0, iconBitmap, appTitle, null, data.installButtonText(), null);
        mAppTitle = appTitle;
        mAppData = data;
        mAppUrl = null;
    }

    // Banner for web apps.
    private AppBannerInfoBar(long nativeInfoBar, String appTitle, Bitmap iconBitmap, String url) {
        super(nativeInfoBar, null, 0, iconBitmap, appTitle, null, getAddToHomescreenText(), null);
        mAppTitle = appTitle;
        mAppData = null;
        mAppUrl = url;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        if (mAppUrl != null) {
            TextView url = new TextView(layout.getContext());
            url.setText(mAppUrl);
            layout.setCustomContent(url);
        }

        super.createContent(layout);

        // Set up accessibility text.
        Context context = getContext();
        if (mAppData != null) {
            layout.setContentDescription(context.getString(
                    R.string.app_banner_view_native_app_accessibility, mAppTitle,
                    mAppData.rating()));
        } else {
            layout.setContentDescription(context.getString(
                    R.string.app_banner_view_web_app_accessibility, mAppTitle,
                    mAppUrl));
        }
    }

    private static String getAddToHomescreenText() {
        return ApplicationStatus.getApplicationContext().getString(R.string.menu_add_to_homescreen);
    }

    @CalledByNative
    private static InfoBar createNativeAppInfoBar(
            long nativeInfoBar, String appTitle, Bitmap iconBitmap, AppData appData) {
        return new AppBannerInfoBar(nativeInfoBar, appTitle, iconBitmap, appData);
    }

    @CalledByNative
    private static InfoBar createWebAppInfoBar(
            long nativeInfoBar, String appTitle, Bitmap iconBitmap, String url) {
        return new AppBannerInfoBar(nativeInfoBar, appTitle, iconBitmap, url);
    }
}
