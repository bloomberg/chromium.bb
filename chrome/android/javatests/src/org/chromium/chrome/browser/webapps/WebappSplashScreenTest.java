// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.content_public.common.ScreenOrientationValues;

/**
 * Tests whether the splash screen is displayed with the correct elements in place.
 */
public class WebappSplashScreenTest extends WebappActivityTestBase {
    private static final String WEBAPP_URL = "http://originalwebsite.com";

    @UiThreadTest
    @SmallTest
    @Feature({"Webapps"})
    public void testSplashScreenDoesntUseSmallWebappInfoIcons() {
        int smallSize = getActivity().getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_min_size) - 1;
        Bitmap image = Bitmap.createBitmap(smallSize, smallSize, Bitmap.Config.ARGB_8888);
        setActivityWebappInfoFromBitmap(image);

        ViewGroup splashScreen = getActivity().createSplashScreen(null);
        getActivity().setSplashScreenIconAndText(splashScreen, null, Color.WHITE);

        ImageView splashImage = (ImageView) splashScreen.findViewById(
                R.id.webapp_splash_screen_icon);
        assertNull(splashImage.getDrawable());
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Webapps"})
    public void testSplashScreenUsesMinWebappInfoIcons() {
        int minSizePx = getActivity().getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_min_size);
        Bitmap image = Bitmap.createBitmap(minSizePx, minSizePx, Bitmap.Config.ARGB_8888);
        setActivityWebappInfoFromBitmap(image);

        ViewGroup splashScreen = getActivity().createSplashScreen(null);
        getActivity().setSplashScreenIconAndText(splashScreen, null, Color.WHITE);

        ImageView splashImage = (ImageView) splashScreen.findViewById(
                R.id.webapp_splash_screen_icon);
        BitmapDrawable drawable = (BitmapDrawable) splashImage.getDrawable();
        assertEquals(minSizePx, drawable.getBitmap().getWidth());
        assertEquals(minSizePx, drawable.getBitmap().getHeight());
    }

    private void setActivityWebappInfoFromBitmap(Bitmap image) {
        WebappInfo mockInfo = WebappInfo.create(WEBAPP_ID, WEBAPP_URL,
                ShortcutHelper.encodeBitmapAsString(image), null, null,
                ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        getActivity().getWebappInfo().copy(mockInfo);
    }
}
