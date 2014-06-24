// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.res.Configuration;
import android.graphics.Canvas;
import android.os.SystemClock;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentViewCore.InternalAccessDelegate;
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
            "initial-scale=2.0, maximum-scale=2.0\" />" +
            "<style>body { width: 5000px; height: 5000px; }</style></head>" +
            "<body>Lorem ipsum dolor sit amet, consectetur adipiscing elit.</body>" +
            "</html>");

    /**
     * InternalAccessDelegate to ensure AccessibilityEvent notifications (Eg:TYPE_VIEW_SCROLLED)
     * are being sent properly on scrolling a page.
     */
    static class TestInternalAccessDelegate implements InternalAccessDelegate {

        private boolean mScrollChanged;
        private final Object mLock = new Object();



        @Override
        public boolean drawChild(Canvas canvas, View child, long drawingTime) {
            return false;
        }

        @Override
        public boolean super_onKeyUp(int keyCode, KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_dispatchKeyEventPreIme(KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_dispatchKeyEvent(KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_onGenericMotionEvent(MotionEvent event) {
            return false;
        }

        @Override
        public void super_onConfigurationChanged(Configuration newConfig) {
        }

        @Override
        public void onScrollChanged(int lPix, int tPix, int oldlPix, int oldtPix) {
            synchronized (mLock) {
                mScrollChanged = true;
            }
        }

        @Override
        public boolean awakenScrollBars() {
            return false;
        }

        @Override
        public boolean super_awakenScrollBars(int startDelay, boolean invalidate) {
            return false;
        }

        /**
         * @return Whether OnScrollChanged() has been called.
         */
        public boolean isScrollChanged() {
            synchronized (mLock) {
                return mScrollChanged;
            }
        }
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
                getContentViewCore().flingForTest(SystemClock.uptimeMillis(), 0, 0, vx, vy);
            }
        });
    }

    private void scrollTo(final int x, final int y) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().getContainerView().scrollTo(x, y);
            }
        });
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        launchContentShellWithUrl(LARGE_PAGE);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        assertWaitForPageScaleFactorMatch(2.0f);

        assertEquals(0, getContentViewCore().getNativeScrollXForTest());
        assertEquals(0, getContentViewCore().getNativeScrollYForTest());
    }

    @SmallTest
    @Feature({"Main"})
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

    @SmallTest
    @RerunWithUpdatedContainerView
    @Feature({"Main"})
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

    /**
     * To ensure the device properly responds to bounds-exceeding scrolls, e.g., overscroll
     * effects are properly initialized.
     */
    @SmallTest
    @RerunWithUpdatedContainerView
    @Feature({"Main"})
    public void testOverScroll() throws Throwable {
        // Overscroll lower-left.
        scrollTo(-10000, 10000);
        assertWaitForScroll(true, false);

        // Overscroll lower-right.
        scrollTo(10000, 10000);
        assertWaitForScroll(false, false);

        // Overscroll upper-right.
        scrollTo(10000, -10000);
        assertWaitForScroll(false, true);

        // Overscroll top-left.
        scrollTo(-10000, -10000);
        assertWaitForScroll(true, true);

        // Diagonal overscroll lower-right.
        scrollTo(10000, 10000);
        assertWaitForScroll(false, false);
    }

    /**
     * To ensure the AccessibilityEvent notifications (Eg:TYPE_VIEW_SCROLLED) are being sent
     * properly on scrolling a page.
     */
    @SmallTest
    @RerunWithUpdatedContainerView
    @Feature({"Main"})
    public void testOnScrollChanged() throws Throwable {
        final int scrollToX = getContentViewCore().getNativeScrollXForTest() + 2500;
        final int scrollToY = getContentViewCore().getNativeScrollYForTest() + 2500;
        final TestInternalAccessDelegate containerViewInternals = new TestInternalAccessDelegate();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().setContainerViewInternals(containerViewInternals);
            }
        });
        scrollTo(scrollToX, scrollToY);
        assertWaitForScroll(false, false);
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return containerViewInternals.isScrollChanged();
            }
        }));
    }
}
