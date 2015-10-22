// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Bitmap;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.metrics.WebappUma;

/**
 * Tests for splash screens with an icon registered in WebappRegistry.
 */
public class WebappSplashScreenIconTest extends WebappActivityTestBase {

    @Override
    protected Intent createIntent() {
        Intent intent = super.createIntent();
        intent.putExtra(ShortcutHelper.EXTRA_ICON, TEST_ICON);
        return intent;
    }

    @Override
    protected void setUp() throws Exception {
        WebappDataStorage.open(getInstrumentation().getTargetContext(), WEBAPP_ID)
                .updateSplashScreenImage(ShortcutHelper.decodeBitmapFromString(TEST_SPLASH_ICON));

        super.setUp();
    }

    // TODO(mlamouri): test that we actually show the icon.

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
