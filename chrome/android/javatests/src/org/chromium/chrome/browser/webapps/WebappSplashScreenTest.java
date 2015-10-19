// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.annotation.TargetApi;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.common.ScreenOrientationValues;

/**
 * Tests for splashscreen.
 */
public class WebappSplashScreenTest extends WebappActivityTestBase {
    @UiThreadTest
    @SmallTest
    @Feature({"Webapps"})
    public void testDoesntUseSmallWebappInfoIcons() {
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
    public void testUsesMinWebappInfoIcons() {
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
        assertTrue(getActivity().isSplashScreenVisibleForTest());

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

        // Waits for the splashscreen animation to finish.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return !getActivity().isSplashScreenVisibleForTest();
                }
            }));
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterCrash() throws InterruptedException {
        assertTrue(getActivity().isSplashScreenVisibleForTest());

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

        // Waits for the splashscreen animation to finish.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return !getActivity().isSplashScreenVisibleForTest();
                }
            }));
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterLoadCompletes() throws InterruptedException {
        assertTrue(getActivity().isSplashScreenVisibleForTest());

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

        // Waits for the splashscreen animation to finish.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return !getActivity().isSplashScreenVisibleForTest();
                }
            }));
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterLoadFails() throws InterruptedException {
        assertTrue(getActivity().isSplashScreenVisibleForTest());

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

        // Waits for the splashscreen animation to finish.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return !getActivity().isSplashScreenVisibleForTest();
                }
            }));
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterMultipleEvents() throws InterruptedException {
        assertTrue(getActivity().isSplashScreenVisibleForTest());

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

        // Waits for the splashscreen animation to finish.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return !getActivity().isSplashScreenVisibleForTest();
                }
            }));
    }

    private void setActivityWebappInfoFromBitmap(Bitmap image) {
        WebappInfo mockInfo = WebappInfo.create(WEBAPP_ID, "about:blank",
                ShortcutHelper.encodeBitmapAsString(image), null, null,
                ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        getActivity().getWebappInfo().copy(mockInfo);
    }
}
