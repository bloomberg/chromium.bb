// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.metrics.WebappUma;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for splash screens with EXTRA_BACKGROND_COLOR specified in the Intent.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class WebappSplashScreenBackgroundColorTest {
    @Rule
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testShowBackgroundColorAndRecordUma() throws Exception {
        mActivityTestRule.startWebappActivity(
                mActivityTestRule
                        .createIntent()
                        // This is setting Color.GREEN with 50% opacity.
                        .putExtra(ShortcutHelper.EXTRA_BACKGROUND_COLOR, 0x8000FF00L));

        ViewGroup splashScreen = mActivityTestRule.waitUntilSplashScreenAppears();
        ColorDrawable background = (ColorDrawable) splashScreen.getBackground();

        Assert.assertEquals(Color.GREEN, background.getColor());

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        WebappUma.HISTOGRAM_SPLASHSCREEN_BACKGROUNDCOLOR,
                        WebappUma.SPLASHSCREEN_COLOR_STATUS_CUSTOM));
    }
}
