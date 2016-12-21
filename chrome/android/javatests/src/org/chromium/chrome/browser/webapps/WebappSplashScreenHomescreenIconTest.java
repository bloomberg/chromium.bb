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
 * Tests for splash screens with EXTRA_ICON specified in the Intent.
 */
public class WebappSplashScreenHomescreenIconTest extends WebappActivityTestBase {
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        startWebappActivity();
    }

    @Override
    protected Intent createIntent() {
        Intent intent = super.createIntent();
        intent.putExtra(ShortcutHelper.EXTRA_ICON, TEST_ICON);
        return intent;
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testShowFallbackIcon() {
        ViewGroup splashScreen = waitUntilSplashScreenAppears();
        ImageView splashImage = (ImageView) splashScreen.findViewById(
                R.id.webapp_splash_screen_icon);
        BitmapDrawable drawable = (BitmapDrawable) splashImage.getDrawable();

        assertEquals(192, drawable.getBitmap().getWidth());
        assertEquals(192, drawable.getBitmap().getHeight());
    }

    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testUmaFallbackIcon() {
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                WebappUma.HISTOGRAM_SPLASHSCREEN_ICON_TYPE,
                WebappUma.SPLASHSCREEN_ICON_TYPE_FALLBACK));

        Bitmap icon = ShortcutHelper.decodeBitmapFromString(TEST_ICON);
        int sizeInDp = Math.round((float) icon.getWidth()
                / getActivity().getResources().getDisplayMetrics().density);
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                WebappUma.HISTOGRAM_SPLASHSCREEN_ICON_SIZE, sizeInDp));
    }
}
