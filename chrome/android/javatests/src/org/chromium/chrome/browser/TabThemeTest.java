// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.filters.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests related to the Tab's theme color.
 */
public class TabThemeTest extends ChromeActivityTestCaseBase<ChromeTabbedActivity> {

    private static final String TEST_PAGE = "/chrome/test/data/android/simple.html";
    private static final String THEMED_TEST_PAGE =
            "/chrome/test/data/android/theme_color_test.html";

    // The theme_color_test.html page uses a pure red theme color.
    private static final int THEME_COLOR = 0xffff0000;

    /**
     * A WebContentsObserver for watching changes in the theme color.
     */
    private static class ThemeColorWebContentsObserver extends WebContentsObserver {
        private CallbackHelper mCallbackHelper;

        public ThemeColorWebContentsObserver(WebContents webContents) {
            super(webContents);
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void didChangeThemeColor(int color) {
            mCallbackHelper.notifyCalled();
        }

        public CallbackHelper getCallbackHelper() {
            return mCallbackHelper;
        }
    }

    public TabThemeTest() {
        super(ChromeTabbedActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    /**
     * AssertEquals two colors as strings so the text output shows their hex value.
     */
    private void assertColorsEqual(int color1, int color2) {
        assertEquals(Integer.toHexString(color1), Integer.toHexString(color2));
    }

    /**
     * Test that the toolbar has the correct color set.
     */
    @Feature({"Toolbar-Theme-Color"})
    @MediumTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @RetryOnFailure
    public void testThemeColorIsCorrect()
            throws ExecutionException, InterruptedException, TimeoutException {

        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                getInstrumentation().getContext());

        final Tab tab = getActivity().getActivityTab();

        ThemeColorWebContentsObserver colorObserver =
                new ThemeColorWebContentsObserver(tab.getWebContents());
        CallbackHelper themeColorHelper = colorObserver.getCallbackHelper();

        // Navigate to a themed page.
        int curCallCount = themeColorHelper.getCallCount();
        loadUrl(testServer.getURL(THEMED_TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(THEME_COLOR, tab.getThemeColor());

        // Navigate to a native page from a themed page.
        loadUrl("chrome://newtab");
        // WebContents does not set theme color for native pages, so don't wait for the call.
        int nativePageThemeColor = ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return tab.getNativePage().getThemeColor();
            }
        });
        assertColorsEqual(nativePageThemeColor, tab.getThemeColor());

        // Navigate to a themed page from a native page.
        curCallCount = themeColorHelper.getCallCount();
        loadUrl(testServer.getURL(THEMED_TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(THEME_COLOR, tab.getThemeColor());

        // Navigate to a non-native non-themed page.
        curCallCount = themeColorHelper.getCallCount();
        loadUrl(testServer.getURL(TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(tab.getDefaultThemeColor(), tab.getThemeColor());

        // Navigate to a themed page from a non-native page.
        curCallCount = themeColorHelper.getCallCount();
        loadUrl(testServer.getURL(THEMED_TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(THEME_COLOR, tab.getThemeColor());
    }
}
