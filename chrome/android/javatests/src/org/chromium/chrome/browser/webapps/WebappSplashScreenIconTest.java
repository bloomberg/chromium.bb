// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.metrics.WebappUma;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for splash screens with an icon registered in WebappRegistry.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
@RetryOnFailure
public class WebappSplashScreenIconTest {
    @Rule
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Before
    public void setUp() throws Exception {
        WebappRegistry.getInstance()
                .getWebappDataStorage(WebappActivityTestRule.WEBAPP_ID)
                .updateSplashScreenImage(WebappActivityTestRule.TEST_SPLASH_ICON);
        mActivityTestRule.startWebappActivity(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_ICON, WebappActivityTestRule.TEST_ICON));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testShowSplashIcon() {
        ViewGroup splashScreen = mActivityTestRule.waitUntilSplashScreenAppears();
        ImageView splashImage = (ImageView) splashScreen.findViewById(
                R.id.webapp_splash_screen_icon);
        BitmapDrawable drawable = (BitmapDrawable) splashImage.getDrawable();

        Assert.assertEquals(512, drawable.getBitmap().getWidth());
        Assert.assertEquals(512, drawable.getBitmap().getHeight());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testUmaCustomIcon() {
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        WebappUma.HISTOGRAM_SPLASHSCREEN_ICON_TYPE,
                        WebappUma.SPLASHSCREEN_ICON_TYPE_CUSTOM));

        Bitmap icon =
                ShortcutHelper.decodeBitmapFromString(WebappActivityTestRule.TEST_SPLASH_ICON);
        int sizeInDp = Math.round((float) icon.getWidth()
                / mActivityTestRule.getActivity().getResources().getDisplayMetrics().density);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        WebappUma.HISTOGRAM_SPLASHSCREEN_ICON_SIZE, sizeInDp));
    }
}
