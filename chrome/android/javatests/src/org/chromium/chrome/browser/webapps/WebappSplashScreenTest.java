// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.metrics.WebappUma;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabTestUtils;

/**
 * Tests for splash screens.
 */
public class WebappSplashScreenTest extends WebappActivityTestBase {
    private int getHistogramTotalCountFor(String histogram, int buckets) {
        int count = 0;

        for (int i = 0; i < buckets; ++i) {
            count += RecordHistogram.getHistogramValueCountForTesting(histogram, i);
        }

        return count;
    }

    private boolean hasHistogramEntry(String histogram, int maxValue) {
        for (int i = 0; i < maxValue; ++i) {
            if (RecordHistogram.getHistogramValueCountForTesting(histogram, i) > 0) {
                return true;
            }
        }
        return false;
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testDefaultBackgroundColor() throws Exception {
        startWebappActivity();
        ViewGroup splashScreen = getActivity().getSplashScreenForTests();
        ColorDrawable background = (ColorDrawable) splashScreen.getBackground();

        assertEquals(ApiCompatibilityUtils.getColor(getActivity().getResources(),
                                                    R.color.webapp_default_bg),
                background.getColor());
    }

    @SmallTest
    @Feature({"Webapps"})
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void testThemeColorWhenNotSpecified() throws Exception {
        startWebappActivity();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        assertEquals(Color.BLACK, getActivity().getWindow().getStatusBarColor());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterFirstPaint() throws Exception {
        startWebappActivity();
        assertTrue(getActivity().isSplashScreenVisibleForTests());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                TabTestUtils.simulateFirstVisuallyNonEmptyPaint(getActivity().getActivityTab());
            }
        });

        assertTrue(waitUntilSplashscreenHides());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterCrash() throws Exception {
        startWebappActivity();
        assertTrue(getActivity().isSplashScreenVisibleForTests());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                TabTestUtils.simulateCrash(getActivity().getActivityTab(), true);
            }
        });

        assertTrue(waitUntilSplashscreenHides());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterLoadCompletes() throws Exception {
        startWebappActivity();
        assertTrue(getActivity().isSplashScreenVisibleForTests());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                TabTestUtils.simulatePageLoadFinished(getActivity().getActivityTab());
            }
        });

        assertTrue(waitUntilSplashscreenHides());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterLoadFails() throws Exception {
        startWebappActivity();
        assertTrue(getActivity().isSplashScreenVisibleForTests());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                TabTestUtils.simulatePageLoadFailed(getActivity().getActivityTab(), 0);
            }
        });

        assertTrue(waitUntilSplashscreenHides());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterMultipleEvents() throws Exception {
        startWebappActivity();
        assertTrue(getActivity().isSplashScreenVisibleForTests());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Tab tab = getActivity().getActivityTab();

                TabTestUtils.simulatePageLoadFinished(tab);
                TabTestUtils.simulatePageLoadFailed(tab, 0);
                TabTestUtils.simulateFirstVisuallyNonEmptyPaint(tab);
            }
        });

        assertTrue(waitUntilSplashscreenHides());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testUmaOnNativeLoad() throws Exception {
        startWebappActivity();

        // Tests UMA values.
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                WebappUma.HISTOGRAM_SPLASHSCREEN_BACKGROUNDCOLOR,
                WebappUma.SPLASHSCREEN_COLOR_STATUS_DEFAULT));
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                WebappUma.HISTOGRAM_SPLASHSCREEN_THEMECOLOR,
                WebappUma.SPLASHSCREEN_COLOR_STATUS_DEFAULT));
        assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(
                WebappUma.HISTOGRAM_SPLASHSCREEN_ICON_TYPE,
                WebappUma.SPLASHSCREEN_ICON_TYPE_NONE));

        // Tests UMA counts.
        assertEquals(1, getHistogramTotalCountFor(WebappUma.HISTOGRAM_SPLASHSCREEN_BACKGROUNDCOLOR,
                WebappUma.SPLASHSCREEN_COLOR_STATUS_MAX));
        assertEquals(1, getHistogramTotalCountFor(WebappUma.HISTOGRAM_SPLASHSCREEN_THEMECOLOR,
                WebappUma.SPLASHSCREEN_COLOR_STATUS_MAX));
        assertEquals(1, getHistogramTotalCountFor(WebappUma.HISTOGRAM_SPLASHSCREEN_ICON_TYPE,
                WebappUma.SPLASHSCREEN_ICON_TYPE_MAX));

        // Given that there is no icon, the ICON_SIZE UMA should not be recorded.
        assertEquals(0, getHistogramTotalCountFor(WebappUma.HISTOGRAM_SPLASHSCREEN_ICON_SIZE, 50));

        // DURATION and HIDES UMA should not have been recorded yet.
        assertFalse(hasHistogramEntry(WebappUma.HISTOGRAM_SPLASHSCREEN_DURATION, 3000));
        assertEquals(0, getHistogramTotalCountFor(WebappUma.HISTOGRAM_SPLASHSCREEN_HIDES,
                WebappUma.SPLASHSCREEN_HIDES_REASON_MAX));
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testUmaWhenSplashHides() throws Exception {
        startWebappActivity();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                TabTestUtils.simulateFirstVisuallyNonEmptyPaint(getActivity().getActivityTab());
            }
        });

        assertTrue(waitUntilSplashscreenHides());

        // DURATION and HIDES should now have a value.
        assertTrue(hasHistogramEntry(WebappUma.HISTOGRAM_SPLASHSCREEN_DURATION, 3000));
        assertEquals(1, getHistogramTotalCountFor(WebappUma.HISTOGRAM_SPLASHSCREEN_HIDES,
                WebappUma.SPLASHSCREEN_HIDES_REASON_MAX));

        // The other UMA records should not have changed.
        assertEquals(1, getHistogramTotalCountFor(WebappUma.HISTOGRAM_SPLASHSCREEN_BACKGROUNDCOLOR,
                WebappUma.SPLASHSCREEN_COLOR_STATUS_MAX));
        assertEquals(1, getHistogramTotalCountFor(WebappUma.HISTOGRAM_SPLASHSCREEN_THEMECOLOR,
                WebappUma.SPLASHSCREEN_COLOR_STATUS_MAX));
        assertEquals(1, getHistogramTotalCountFor(WebappUma.HISTOGRAM_SPLASHSCREEN_ICON_TYPE,
                WebappUma.SPLASHSCREEN_ICON_TYPE_MAX));
        assertEquals(0, getHistogramTotalCountFor(WebappUma.HISTOGRAM_SPLASHSCREEN_ICON_SIZE, 50));
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testRegularSplashScreenAppears() throws Exception {
        // Register a properly-sized icon for the splash screen.
        Context context = getInstrumentation().getTargetContext();
        int thresholdSize = context.getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_threshold);
        int bitmapSize = thresholdSize + 1;
        Bitmap splashBitmap = Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        WebappDataStorage.open(context, WEBAPP_ID).updateSplashScreenImage(splashBitmap);

        startWebappActivity(createIntent());
        assertTrue(getActivity().isSplashScreenVisibleForTests());
        ViewGroup splashScreen = getActivity().getSplashScreenForTests();

        ImageView splashImage =
                (ImageView) splashScreen.findViewById(R.id.webapp_splash_screen_icon);
        assertEquals(bitmapSize, splashImage.getMeasuredWidth());
        assertEquals(bitmapSize, splashImage.getMeasuredHeight());

        TextView splashText = (TextView) splashScreen.findViewById(R.id.webapp_splash_screen_name);
        int[] rules = ((RelativeLayout.LayoutParams) splashText.getLayoutParams()).getRules();
        assertEquals(RelativeLayout.TRUE, rules[RelativeLayout.ALIGN_PARENT_BOTTOM]);
        assertEquals(0, rules[RelativeLayout.BELOW]);
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testSmallSplashScreenAppears() throws Exception {
        // Register a smaller icon for the splash screen.
        Context context = getInstrumentation().getTargetContext();
        int thresholdSize = context.getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_threshold);
        int bitmapSize = context.getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_minimum);
        Bitmap splashBitmap = Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        WebappDataStorage.open(context, WEBAPP_ID).updateSplashScreenImage(splashBitmap);

        startWebappActivity(createIntent());
        assertTrue(getActivity().isSplashScreenVisibleForTests());
        ViewGroup splashScreen = getActivity().getSplashScreenForTests();

        // The icon is centered within a fixed-size area on the splash screen.
        ImageView splashImage =
                (ImageView) splashScreen.findViewById(R.id.webapp_splash_screen_icon);
        assertEquals(thresholdSize, splashImage.getMeasuredWidth());
        assertEquals(thresholdSize, splashImage.getMeasuredHeight());

        // The web app name is anchored to the icon.
        TextView splashText = (TextView) splashScreen.findViewById(R.id.webapp_splash_screen_name);
        int[] rules = ((RelativeLayout.LayoutParams) splashText.getLayoutParams()).getRules();
        assertEquals(0, rules[RelativeLayout.ALIGN_PARENT_BOTTOM]);
        assertEquals(R.id.webapp_splash_screen_icon, rules[RelativeLayout.BELOW]);
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testSplashScreenWithoutImageAppears() throws Exception {
        // Register an image that's too small for the splash screen.
        Context context = getInstrumentation().getTargetContext();
        int bitmapSize = context.getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_minimum) - 1;
        Bitmap splashBitmap = Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        WebappDataStorage.open(context, WEBAPP_ID).updateSplashScreenImage(splashBitmap);

        Intent intent = createIntent();
        intent.putExtra(ShortcutHelper.EXTRA_IS_ICON_GENERATED, true);
        startWebappActivity(intent);
        assertTrue(getActivity().isSplashScreenVisibleForTests());
        ViewGroup splashScreen = getActivity().getSplashScreenForTests();

        // There's no icon displayed.
        ImageView splashImage =
                (ImageView) splashScreen.findViewById(R.id.webapp_splash_screen_icon);
        assertNull(splashImage);

        View spacer = splashScreen.findViewById(R.id.webapp_splash_space);
        assertNotNull(spacer);

        // The web app name is anchored to the top of the spacer.
        TextView splashText = (TextView) splashScreen.findViewById(R.id.webapp_splash_screen_name);
        int[] rules = ((RelativeLayout.LayoutParams) splashText.getLayoutParams()).getRules();
        assertEquals(0, rules[RelativeLayout.ALIGN_PARENT_BOTTOM]);
        assertEquals(0, rules[RelativeLayout.BELOW]);
        assertEquals(R.id.webapp_splash_space, rules[RelativeLayout.ABOVE]);
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testSplashScreenAppearsWithoutRegisteredSplashImage() throws Exception {
        // Don't register anything for the web app, which represents apps that were added to the
        // home screen before splash screen images were downloaded.
        startWebappActivity(createIntent());
        assertTrue(getActivity().isSplashScreenVisibleForTests());
        ViewGroup splashScreen = getActivity().getSplashScreenForTests();

        // There's no icon displayed.
        ImageView splashImage =
                (ImageView) splashScreen.findViewById(R.id.webapp_splash_screen_icon);
        assertNull(splashImage);

        View spacer = splashScreen.findViewById(R.id.webapp_splash_space);
        assertNotNull(spacer);

        // The web app name is anchored to the top of the spacer.
        TextView splashText = (TextView) splashScreen.findViewById(R.id.webapp_splash_screen_name);
        int[] rules = ((RelativeLayout.LayoutParams) splashText.getLayoutParams()).getRules();
        assertEquals(0, rules[RelativeLayout.ALIGN_PARENT_BOTTOM]);
        assertEquals(0, rules[RelativeLayout.BELOW]);
        assertEquals(0, rules[RelativeLayout.CENTER_IN_PARENT]);
        assertEquals(R.id.webapp_splash_space, rules[RelativeLayout.ABOVE]);
    }
}
