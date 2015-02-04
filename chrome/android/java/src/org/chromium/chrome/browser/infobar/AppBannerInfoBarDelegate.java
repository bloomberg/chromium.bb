// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Bitmap;

import org.chromium.base.CalledByNative;

/**
 * Handles creation of AppBannerInfoBars.
 */
public class AppBannerInfoBarDelegate {
    private AppBannerInfoBarDelegate() {
    }

    @CalledByNative
    private static AppBannerInfoBarDelegate create() {
        return new AppBannerInfoBarDelegate();
    }

    @CalledByNative
    private InfoBar showInfoBar(
            long nativeInfoBar, String appTitle, Bitmap iconBitmap, String buttonText, String url) {
        return new AppBannerInfoBar(nativeInfoBar, appTitle, iconBitmap, buttonText, url);
    }
}