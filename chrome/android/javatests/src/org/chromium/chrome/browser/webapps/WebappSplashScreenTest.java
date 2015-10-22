// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.annotation.TargetApi;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.metrics.WebappUma;
import org.chromium.chrome.browser.tab.TabObserver;

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
    public void testDefaultBackgroundColor() {
        ViewGroup splashScreen = getActivity().getSplashScreenForTests();
        ColorDrawable background = (ColorDrawable) splashScreen.getBackground();

        assertEquals(ApiCompatibilityUtils.getColor(getActivity().getResources(),
                                                    R.color.webapp_default_bg),
                background.getColor());
    }

    @SmallTest
    @Feature({"Webapps"})
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void testThemeColorWhenNotSpecified() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        assertEquals(Color.BLACK, getActivity().getWindow().getStatusBarColor());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterFirstPaint() throws InterruptedException {
        assertTrue(getActivity().isSplashScreenVisibleForTests());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                RewindableIterator<TabObserver> observers =
                        getActivity().getActivityTab().getTabObservers();
                while (observers.hasNext()) {
                    observers.next().didFirstVisuallyNonEmptyPaint(getActivity().getActivityTab());
                }
            }
        });

        assertTrue(waitUntilSplashscreenHides());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterCrash() throws InterruptedException {
        assertTrue(getActivity().isSplashScreenVisibleForTests());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                RewindableIterator<TabObserver> observers =
                        getActivity().getActivityTab().getTabObservers();
                while (observers.hasNext()) {
                    observers.next().onCrash(getActivity().getActivityTab(), true);
                }
            }
        });

        assertTrue(waitUntilSplashscreenHides());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterLoadCompletes() throws InterruptedException {
        assertTrue(getActivity().isSplashScreenVisibleForTests());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                RewindableIterator<TabObserver> observers =
                        getActivity().getActivityTab().getTabObservers();
                while (observers.hasNext()) {
                    observers.next().onPageLoadFinished(getActivity().getActivityTab());
                }
            }
        });

        assertTrue(waitUntilSplashscreenHides());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterLoadFails() throws InterruptedException {
        assertTrue(getActivity().isSplashScreenVisibleForTests());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                RewindableIterator<TabObserver> observers =
                        getActivity().getActivityTab().getTabObservers();
                while (observers.hasNext()) {
                    observers.next().onPageLoadFailed(getActivity().getActivityTab(), 0);
                }
            }
        });

        assertTrue(waitUntilSplashscreenHides());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterMultipleEvents() throws InterruptedException {
        assertTrue(getActivity().isSplashScreenVisibleForTests());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                RewindableIterator<TabObserver> observers =
                        getActivity().getActivityTab().getTabObservers();
                while (observers.hasNext()) {
                    observers.next().onPageLoadFinished(getActivity().getActivityTab());
                }

                observers.rewind();
                while (observers.hasNext()) {
                    observers.next().onPageLoadFailed(getActivity().getActivityTab(), 0);
                }

                observers.rewind();
                while (observers.hasNext()) {
                    observers.next().didFirstVisuallyNonEmptyPaint(getActivity().getActivityTab());
                }
            }
        });

        assertTrue(waitUntilSplashscreenHides());
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testUmaOnNativeLoad() {
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
    public void testUmaWhenSplashHides() throws InterruptedException {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                RewindableIterator<TabObserver> observers =
                        getActivity().getActivityTab().getTabObservers();
                while (observers.hasNext()) {
                    observers.next().didFirstVisuallyNonEmptyPaint(getActivity().getActivityTab());
                }
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
}
