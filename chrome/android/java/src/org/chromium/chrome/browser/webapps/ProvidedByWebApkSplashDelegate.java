// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.view.View;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.webapk.lib.common.WebApkCommonUtils;

/** Delegate which uses splash screen screenshot from the WebAPK's content provider. */
public class ProvidedByWebApkSplashDelegate implements SplashDelegate {
    @Override
    public View buildSplashView(WebappInfo webappInfo) {
        Context appContext = ContextUtils.getApplicationContext();
        ImageView splashView = new ImageView(appContext);
        int backgroundColor =
                ColorUtils.getOpaqueColor(webappInfo.backgroundColor(ApiCompatibilityUtils.getColor(
                        appContext.getResources(), R.color.webapp_default_bg)));
        splashView.setBackgroundColor(backgroundColor);

        Bitmap splashBitmap = null;
        try (StrictModeContext smc = StrictModeContext.allowDiskReads()) {
            splashBitmap = FileUtils.queryBitmapFromContentProvider(appContext,
                    Uri.parse(WebApkCommonUtils.generateSplashContentProviderUri(
                            webappInfo.webApkPackageName())));
        }
        if (splashBitmap != null) {
            splashView.setScaleType(ImageView.ScaleType.FIT_CENTER);
            splashView.setImageBitmap(splashBitmap);
        }

        return splashView;
    }

    @Override
    public void onSplashHidden(Tab tab, @SplashController.SplashHidesReason int reason,
            long startTimestamp, long endTimestamp) {
        // TODO(pkotwicz) implement.
    }

    @Override
    public boolean shouldWaitForSubsequentPageLoadToHideSplash() {
        return false;
    }
}
