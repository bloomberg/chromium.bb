// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.annotation.TargetApi;
import android.content.Intent;
import android.graphics.Color;
import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.metrics.WebappUma;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Tests for splash screens with EXTRA_THEME_COLOR specified in the Intent.
 */
public class WebappSplashScreenThemeColorTest extends WebappActivityTestBase {
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        startWebappActivity();
    }

    @Override
    protected Intent createIntent() {
        Intent intent = super.createIntent();
        intent.putExtra(ShortcutHelper.EXTRA_URL, "http://localhost");
        // This is setting Color.Magenta with 50% opacity.
        intent.putExtra(ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.argb(128, 255, 0, 255));
        return intent;
    }

    @SmallTest
    @Feature({"Webapps"})
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void testThemeColorWhenSpecified() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        assertEquals(ColorUtils.getDarkenedColorForStatusBar(Color.MAGENTA),
                getActivity().getWindow().getStatusBarColor());
    }

    @SmallTest
    @Feature({"Webapps"})
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void testThemeColorNotUsedIfPagesHasOne() throws InterruptedException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                TabTestUtils.simulateChangeThemeColor(getActivity().getActivityTab(), Color.GREEN);
            }
        });

        // Waits for theme-color to change so the test doesn't rely on system timing.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return getActivity().getWindow().getStatusBarColor()
                            == ColorUtils.getDarkenedColorForStatusBar(Color.GREEN);
                }
            }));
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testUmaThemeColorCustom() {
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                WebappUma.HISTOGRAM_SPLASHSCREEN_THEMECOLOR,
                WebappUma.SPLASHSCREEN_COLOR_STATUS_CUSTOM));
    }
}
