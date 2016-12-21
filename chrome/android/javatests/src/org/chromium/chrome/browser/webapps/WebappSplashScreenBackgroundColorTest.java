// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.metrics.WebappUma;

/**
 * Tests for splash screens with EXTRA_BACKGROND_COLOR specified in the Intent.
 */
public class WebappSplashScreenBackgroundColorTest extends WebappActivityTestBase {
    @Override
    protected Intent createIntent() {
        Intent intent = super.createIntent();
        // This is setting Color.GREEN with 50% opacity.
        intent.putExtra(ShortcutHelper.EXTRA_BACKGROUND_COLOR, 0x8000FF00L);
        return intent;
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testShowBackgroundColorAndRecordUma() throws Exception {
        startWebappActivity();

        ViewGroup splashScreen = waitUntilSplashScreenAppears();
        ColorDrawable background = (ColorDrawable) splashScreen.getBackground();

        assertEquals(Color.GREEN, background.getColor());

        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                WebappUma.HISTOGRAM_SPLASHSCREEN_BACKGROUNDCOLOR,
                WebappUma.SPLASHSCREEN_COLOR_STATUS_CUSTOM));
    }
}
