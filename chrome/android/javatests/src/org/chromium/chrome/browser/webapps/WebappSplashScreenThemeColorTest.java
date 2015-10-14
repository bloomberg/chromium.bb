// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.annotation.TargetApi;
import android.content.Intent;
import android.graphics.Color;
import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.util.ColorUtils;

/**
 * Tests for splashscreen.
 */
public class WebappSplashScreenThemeColorTest extends WebappActivityTestBase {

    @Override
    protected Intent createIntent() {
        Intent intent = super.createIntent();
        intent.putExtra(ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.MAGENTA);
        return intent;
    }

    @SmallTest
    @Feature({"Webapps"})
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void testSplashscreenThemeColorWhenSpecified() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        assertEquals(ColorUtils.getDarkenedColorForStatusBar(Color.MAGENTA),
                getActivity().getWindow().getStatusBarColor());
    }
}
