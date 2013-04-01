// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.FlakyTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_shell_apk.ContentShellTestBase;

/*
 * Tests that we can scroll and fling a ContentView running inside ContentShell.
 */
public class ContentViewScrollingTest extends ContentShellTestBase {

    private static final String LARGE_PAGE = UrlUtils.encodeHtmlDataUri(
            "<html><head>" +
            "<meta name=\"viewport\" content=\"width=device-width, " +
            "initial-scale=1.0, maximum-scale=1.0\" />" +
            "<style>body { width: 5000px; height: 5000px; }</style></head>" +
            "<body>Lorem ipsum dolor sit amet, consectetur adipiscing elit.</body>" +
            "</html>");

    private void assertWaitForPageScaleFactor(final float scale) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getContentViewCore().getScale() == scale;
            }
        }));
    }

    private void assertWaitForScroll(final boolean hugLeft, final boolean hugTop)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                // Scrolling and flinging don't result in exact coordinates.
                final int MIN_THRESHOLD = 5;
                final int MAX_THRESHOLD = 100;

                boolean xCorrect = hugLeft ?
                        getContentViewCore().getNativeScrollXForTest() < MIN_THRESHOLD :
                        getContentViewCore().getNativeScrollXForTest() > MAX_THRESHOLD;
                boolean yCorrect = hugTop ?
                        getContentViewCore().getNativeScrollYForTest() < MIN_THRESHOLD :
                        getContentViewCore().getNativeScrollYForTest() > MAX_THRESHOLD;
                return xCorrect && yCorrect;
            }
        }));
    }

    private void fling(final int vx, final int vy) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().fling(System.currentTimeMillis(), 0, 0, vx, vy);
            }
        });
    }

    private void scrollTo(final int x, final int y) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().scrollTo(x, y);
            }
        });
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        launchContentShellWithUrl(LARGE_PAGE);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        assertWaitForPageScaleFactor(1.0f);

        assertEquals(0, getContentViewCore().getNativeScrollXForTest());
        assertEquals(0, getContentViewCore().getNativeScrollYForTest());
    }

    /**
     * @SmallTest
     * @Feature({"Main"})
     * crbug.com/224458
     */
    @FlakyTest
    public void testFling() throws Throwable {
        // Vertical fling to lower-left.
        fling(0, -1000);
        assertWaitForScroll(true, false);

        // Horizontal fling to lower-right.
        fling(-1000, 0);
        assertWaitForScroll(false, false);

        // Vertical fling to upper-right.
        fling(0, 1000);
        assertWaitForScroll(false, true);

        // Horizontal fling to top-left.
        fling(1000, 0);
        assertWaitForScroll(true, true);

        // Diagonal fling to bottom-right.
        fling(-1000, -1000);
        assertWaitForScroll(false, false);
    }

    /**
     * @SmallTest
     * @Feature({"Main"})
     * crbug.com/224458
     */
    @FlakyTest
    public void testScroll() throws Throwable {
        // Vertical scroll to lower-left.
        scrollTo(0, 2500);
        assertWaitForScroll(true, false);

        // Horizontal scroll to lower-right.
        scrollTo(2500, 2500);
        assertWaitForScroll(false, false);

        // Vertical scroll to upper-right.
        scrollTo(2500, 0);
        assertWaitForScroll(false, true);

        // Horizontal scroll to top-left.
        scrollTo(0, 0);
        assertWaitForScroll(true, true);

        // Diagonal scroll to bottom-right.
        scrollTo(2500, 2500);
        assertWaitForScroll(false, false);
    }
}
