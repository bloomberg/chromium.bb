// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.metrics.WebappUma;

/**
 * Tests for splash screens with an icon registered in WebappRegistry.
 */
@RetryOnFailure
public class WebappSplashScreenIconTest extends WebappActivityTestBase {

    @Override
    protected Intent createIntent() {
        Intent intent = super.createIntent();
        intent.putExtra(ShortcutHelper.EXTRA_ICON, TEST_ICON);
        return intent;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        WebappRegistry.getInstance().getWebappDataStorage(WEBAPP_ID).updateSplashScreenImage(
                TEST_SPLASH_ICON);
        startWebappActivity();
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testShowSplashIcon() {
        ViewGroup splashScreen = waitUntilSplashScreenAppears();
        ImageView splashImage = (ImageView) splashScreen.findViewById(
                R.id.webapp_splash_screen_icon);
        BitmapDrawable drawable = (BitmapDrawable) splashImage.getDrawable();

        assertEquals(512, drawable.getBitmap().getWidth());
        assertEquals(512, drawable.getBitmap().getHeight());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testUmaCustomIcon() {
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                WebappUma.HISTOGRAM_SPLASHSCREEN_ICON_TYPE,
                WebappUma.SPLASHSCREEN_ICON_TYPE_CUSTOM));

        Bitmap icon = ShortcutHelper.decodeBitmapFromString(TEST_SPLASH_ICON);
        int sizeInDp = Math.round((float) icon.getWidth()
                / getActivity().getResources().getDisplayMetrics().density);
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                WebappUma.HISTOGRAM_SPLASHSCREEN_ICON_SIZE, sizeInDp));
    }
}
