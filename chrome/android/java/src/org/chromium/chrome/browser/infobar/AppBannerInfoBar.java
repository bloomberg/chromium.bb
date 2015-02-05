// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Bitmap;
import android.widget.TextView;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CalledByNative;
import org.chromium.chrome.R;

/**
 * Infobar informing the user about an app related to this page.
 */
public class AppBannerInfoBar extends ConfirmInfoBar {
    /** Web app: URL pointing to the web app. */
    private final String mAppUrl;

    // Banner for web apps.
    public AppBannerInfoBar(long nativeInfoBar, String appTitle, Bitmap iconBitmap, String url) {
        super(nativeInfoBar, null, 0, iconBitmap, appTitle, null, getAddToHomescreenText(), null);
        mAppUrl = url;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        TextView url = new TextView(layout.getContext());
        url.setText(mAppUrl);
        layout.setCustomContent(url);
        super.createContent(layout);
    }

    private static String getAddToHomescreenText() {
        return ApplicationStatus.getApplicationContext().getString(R.string.menu_add_to_homescreen);
    }

    @CalledByNative
    private static InfoBar createWebAppInfoBar(
            long nativeInfoBar, String appTitle, Bitmap iconBitmap, String url) {
        return new AppBannerInfoBar(nativeInfoBar, appTitle, iconBitmap, url);
    }
}