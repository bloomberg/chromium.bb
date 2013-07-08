// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;
import android.view.Gravity;
import android.view.View;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JavascriptEventObserver;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.ui.gfx.DeviceDisplayInfo;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Integration tests for synchronous scrolling.
 */
public class AndroidScrollIntegrationTest extends AwTestBase {
    private static final int SCROLL_OFFSET_PROPAGATION_TIMEOUT_MS = 6 * 1000;

    private static class OverScrollByCallbackHelper extends CallbackHelper {
        int mDeltaX;
        int mDeltaY;

        public int getDeltaX() {
            assert getCallCount() > 0;
            return mDeltaX;
        }

        public int getDeltaY() {
            assert getCallCount() > 0;
            return mDeltaY;
        }

        public void notifyCalled(int deltaX, int deltaY) {
            mDeltaX = deltaX;
            mDeltaY = deltaY;
            notifyCalled();
        }
    }

    private static class ScrollTestContainerView extends AwTestContainerView {
        private int mMaxScrollXPix = -1;
        private int mMaxScrollYPix = -1;

        private CallbackHelper mOnScrollToCallbackHelper = new CallbackHelper();
        private OverScrollByCallbackHelper mOverScrollByCallbackHelper =
            new OverScrollByCallbackHelper();

        public ScrollTestContainerView(Context context) {
            super(context);
        }

        public CallbackHelper getOnScrollToCallbackHelper() {
            return mOnScrollToCallbackHelper;
        }

        public OverScrollByCallbackHelper getOverScrollByCallbackHelper() {
            return mOverScrollByCallbackHelper;
        }

        public void setMaxScrollX(int maxScrollXPix) {
            mMaxScrollXPix = maxScrollXPix;
        }

        public void setMaxScrollY(int maxScrollYPix) {
            mMaxScrollYPix = maxScrollYPix;
        }

        @Override
        protected boolean overScrollBy(int deltaX, int deltaY, int scrollX, int scrollY,
                     int scrollRangeX, int scrollRangeY, int maxOverScrollX, int maxOverScrollY,
                     boolean isTouchEvent) {
            mOverScrollByCallbackHelper.notifyCalled(deltaX, deltaY);
            return super.overScrollBy(deltaX, deltaY, scrollX, scrollY,
                     scrollRangeX, scrollRangeY, maxOverScrollX, maxOverScrollY, isTouchEvent);
        }

        @Override
        public void scrollTo(int x, int y) {
            if (mMaxScrollXPix != -1)
                x = Math.min(mMaxScrollXPix, x);
            if (mMaxScrollYPix != -1)
                y = Math.min(mMaxScrollYPix, y);
            super.scrollTo(x, y);
            mOnScrollToCallbackHelper.notifyCalled();
        }
    }

    @Override
    protected TestDependencyFactory createTestDependencyFactory() {
        return new TestDependencyFactory() {
            @Override
            public AwTestContainerView createAwTestContainerView(AwTestRunnerActivity activity) {
                return new ScrollTestContainerView(activity);
            }
        };
    }

    private String makeTestPage(String onscrollObserver, String firstFrameObserver) {
        String headers =
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> " +
            "<style type=\"text/css\"> " +
            "   div { " +
            "      width:1000px; " +
            "      height:10000px; " +
            "      background-color: blue; " +
            "   } " +
            "</style> ";
        String content = "<div>test div</div> ";
        if (onscrollObserver != null) {
            content +=
            "<script> " +
            "   window.onscroll = function(oEvent) { " +
            "       " + onscrollObserver + ".notifyJava(); " +
            "   } " +
            "</script>";
        }
        if (firstFrameObserver != null) {
            content +=
            "<script> " +
            "   window.framesToIgnore = 10; " +
            "   window.onAnimationFrame = function(timestamp) { " +
            "     if (window.framesToIgnore == 0) { " +
            "         " + firstFrameObserver + ".notifyJava(); " +
            "     } else {" +
            "       window.framesToIgnore -= 1; " +
            "       window.requestAnimationFrame(window.onAnimationFrame); " +
            "     } " +
            "   }; " +
            "   window.requestAnimationFrame(window.onAnimationFrame); " +
            "</script>";
        }
        return CommonResources.makeHtmlPageFrom(headers, content);
    }

    private void scrollToOnMainSync(final View view, final int xPix, final int yPix) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                view.scrollTo(xPix, yPix);
            }
        });
    }

    private void setMaxScrollOnMainSync(final ScrollTestContainerView testContainerView,
            final int maxScrollXPix, final int maxScrollYPix) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                testContainerView.setMaxScrollX(maxScrollXPix);
                testContainerView.setMaxScrollY(maxScrollYPix);
            }
        });
    }

    private boolean checkScrollOnMainSync(final ScrollTestContainerView testContainerView,
            final int scrollXPix, final int scrollYPix) {
        final AtomicBoolean equal = new AtomicBoolean(false);
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                equal.set((scrollXPix == testContainerView.getScrollX()) &&
                    (scrollYPix == testContainerView.getScrollY()));
            }
        });
        return equal.get();
    }

    private void assertScrollOnMainSync(final ScrollTestContainerView testContainerView,
            final int scrollXPix, final int scrollYPix) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                assertEquals(scrollXPix, testContainerView.getScrollX());
                assertEquals(scrollYPix, testContainerView.getScrollY());
            }
        });
    }

    private void assertScrollInJs(final AwContents awContents,
            final TestAwContentsClient contentsClient,
            final int xCss, final int yCss) throws Exception {
        String x = executeJavaScriptAndWaitForResult(awContents, contentsClient, "window.scrollX");
        String y = executeJavaScriptAndWaitForResult(awContents, contentsClient, "window.scrollY");

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    try {
                        String x = executeJavaScriptAndWaitForResult(awContents, contentsClient,
                            "window.scrollX");
                        String y = executeJavaScriptAndWaitForResult(awContents, contentsClient,
                            "window.scrollY");
                        return (Integer.toString(xCss).equals(x) &&
                            Integer.toString(yCss).equals(y));
                    } catch (Throwable t) {
                        t.printStackTrace();
                        fail("Failed to get window.scroll(X/Y): " + t.toString());
                        return false;
                    }
                }
            }, WAIT_TIMEOUT_SECONDS * 1000, CHECK_INTERVAL));
    }

    private void loadTestPageAndWaitForFirstFrame(final ScrollTestContainerView testContainerView,
            final TestAwContentsClient contentsClient,
            final String onscrollObserverName) throws Exception {
        final JavascriptEventObserver firstFrameObserver = new JavascriptEventObserver();
        final String firstFrameObserverName = "firstFrameObserver";
        enableJavaScriptOnUiThread(testContainerView.getAwContents());

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                firstFrameObserver.register(testContainerView.getContentViewCore(),
                    firstFrameObserverName);
            }
        });

        loadDataSync(testContainerView.getAwContents(), contentsClient.getOnPageFinishedHelper(),
                makeTestPage(onscrollObserverName, firstFrameObserverName), "text/html", false);

        // We wait for "a couple" of frames for the active tree in CC to stabilize and for pending
        // tree activations to stop clobbering the root scroll layer's scroll offset. This wait
        // doesn't strictly guarantee that but there isn't a good alternative and this seems to
        // work fine.
        firstFrameObserver.waitForEvent(WAIT_TIMEOUT_SECONDS * 1000);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUiScrollReflectedInJs() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
            (ScrollTestContainerView) createAwTestContainerViewOnMainSync(contentsClient);
        enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final double deviceDIPScale =
            DeviceDisplayInfo.create(testContainerView.getContext()).getDIPScale();
        final int targetScrollXCss = 233;
        final int targetScrollYCss = 322;
        final int targetScrollXPix = (int) Math.round(targetScrollXCss * deviceDIPScale);
        final int targetScrollYPix = (int) Math.round(targetScrollYCss * deviceDIPScale);
        final JavascriptEventObserver onscrollObserver = new JavascriptEventObserver();

        Log.w("AndroidScrollIntegrationTest", String.format("scroll in Js (%d, %d) -> (%d, %d)",
                    targetScrollXCss, targetScrollYCss, targetScrollXPix, targetScrollYPix));

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                onscrollObserver.register(testContainerView.getContentViewCore(),
                    "onscrollObserver");
            }
        });

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, "onscrollObserver");

        scrollToOnMainSync(testContainerView, targetScrollXPix, targetScrollYPix);

        onscrollObserver.waitForEvent(SCROLL_OFFSET_PROPAGATION_TIMEOUT_MS);
        assertScrollInJs(testContainerView.getAwContents(), contentsClient,
                targetScrollXCss, targetScrollYCss);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testJsScrollReflectedInUi() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
            (ScrollTestContainerView) createAwTestContainerViewOnMainSync(contentsClient);
        enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final double deviceDIPScale =
            DeviceDisplayInfo.create(testContainerView.getContext()).getDIPScale();
        final int targetScrollXCss = 132;
        final int targetScrollYCss = 243;
        final int targetScrollXPix = (int) Math.round(targetScrollXCss * deviceDIPScale);
        final int targetScrollYPix = (int) Math.round(targetScrollYCss * deviceDIPScale);

        loadDataSync(testContainerView.getAwContents(), contentsClient.getOnPageFinishedHelper(),
                makeTestPage(null, null), "text/html", false);

        final CallbackHelper onScrollToCallbackHelper =
            testContainerView.getOnScrollToCallbackHelper();
        final int scrollToCallCount = onScrollToCallbackHelper.getCallCount();
        executeJavaScriptAndWaitForResult(testContainerView.getAwContents(), contentsClient,
                String.format("window.scrollTo(%d, %d);", targetScrollXCss, targetScrollYCss));
        onScrollToCallbackHelper.waitForCallback(scrollToCallCount);

        assertScrollOnMainSync(testContainerView, targetScrollXPix, targetScrollYPix);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testJsScrollCanBeAlteredByUi() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
            (ScrollTestContainerView) createAwTestContainerViewOnMainSync(contentsClient);
        enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final double deviceDIPScale =
            DeviceDisplayInfo.create(testContainerView.getContext()).getDIPScale();
        final int targetScrollXCss = 132;
        final int targetScrollYCss = 243;
        final int targetScrollXPix = (int) Math.round(targetScrollXCss * deviceDIPScale);
        final int targetScrollYPix = (int) Math.round(targetScrollYCss * deviceDIPScale);

        final int maxScrollXCss = 101;
        final int maxScrollYCss = 201;
        final int maxScrollXPix = (int) Math.round(maxScrollXCss * deviceDIPScale);
        final int maxScrollYPix = (int) Math.round(maxScrollYCss * deviceDIPScale);

        loadDataSync(testContainerView.getAwContents(), contentsClient.getOnPageFinishedHelper(),
                makeTestPage(null, null), "text/html", false);

        setMaxScrollOnMainSync(testContainerView, maxScrollXPix, maxScrollYPix);

        final CallbackHelper onScrollToCallbackHelper =
            testContainerView.getOnScrollToCallbackHelper();
        final int scrollToCallCount = onScrollToCallbackHelper.getCallCount();
        executeJavaScriptAndWaitForResult(testContainerView.getAwContents(), contentsClient,
                "window.scrollTo(" + targetScrollXCss + "," + targetScrollYCss + ")");
        onScrollToCallbackHelper.waitForCallback(scrollToCallCount);

        assertScrollOnMainSync(testContainerView, maxScrollXPix, maxScrollYPix);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testTouchScrollCanBeAlteredByUi() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
            (ScrollTestContainerView) createAwTestContainerViewOnMainSync(contentsClient);
        enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final int dragSteps = 10;
        final int dragStepSize = 24;
        // Watch out when modifying - if the y or x delta aren't big enough vertical or horizontal
        // scroll snapping will kick in.
        final int targetScrollXPix = dragStepSize * dragSteps;
        final int targetScrollYPix = dragStepSize * dragSteps;

        final double deviceDIPScale =
            DeviceDisplayInfo.create(testContainerView.getContext()).getDIPScale();
        final int maxScrollXPix = 101;
        final int maxScrollYPix = 211;
        // Make sure we can't hit these values simply as a result of scrolling.
        assert (maxScrollXPix % dragStepSize) != 0;
        assert (maxScrollYPix % dragStepSize) != 0;
        final int maxScrollXCss = (int) Math.round(maxScrollXPix / deviceDIPScale);
        final int maxScrollYCss = (int) Math.round(maxScrollYPix / deviceDIPScale);

        setMaxScrollOnMainSync(testContainerView, maxScrollXPix, maxScrollYPix);

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, null);

        final CallbackHelper onScrollToCallbackHelper =
            testContainerView.getOnScrollToCallbackHelper();
        final int scrollToCallCount = onScrollToCallbackHelper.getCallCount();
        AwTestTouchUtils.dragCompleteView(testContainerView,
                0, -targetScrollXPix, // these need to be negative as we're scrolling down.
                0, -targetScrollYPix,
                dragSteps);

        for (int i = 1; i <= dragSteps; ++i) {
            onScrollToCallbackHelper.waitForCallback(scrollToCallCount, i);
            if (checkScrollOnMainSync(testContainerView, maxScrollXPix, maxScrollYPix))
                break;
        }

        assertScrollOnMainSync(testContainerView, maxScrollXPix, maxScrollYPix);
        assertScrollInJs(testContainerView.getAwContents(), contentsClient,
                maxScrollXCss, maxScrollYCss);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverScrollX() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
            (ScrollTestContainerView) createAwTestContainerViewOnMainSync(contentsClient);
        final OverScrollByCallbackHelper overScrollByCallbackHelper =
            testContainerView.getOverScrollByCallbackHelper();
        enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final int overScrollDeltaX = 30;
        final int oneStep = 1;

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, null);

        // Scroll separately in different dimensions because of vertical/horizontal scroll
        // snap.
        final int overScrollCallCount = overScrollByCallbackHelper.getCallCount();
        AwTestTouchUtils.dragCompleteView(testContainerView,
                0, overScrollDeltaX,
                0, 0,
                oneStep);
        overScrollByCallbackHelper.waitForCallback(overScrollCallCount);
        // Unfortunately the gesture detector seems to 'eat' some number of pixels. For now
        // checking that the value is < 0 (overscroll is reported as negative values) will have to
        // do.
        assertTrue(0 > overScrollByCallbackHelper.getDeltaX());
        assertEquals(0, overScrollByCallbackHelper.getDeltaY());

        assertScrollOnMainSync(testContainerView, 0, 0);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverScrollY() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
            (ScrollTestContainerView) createAwTestContainerViewOnMainSync(contentsClient);
        final OverScrollByCallbackHelper overScrollByCallbackHelper =
            testContainerView.getOverScrollByCallbackHelper();
        enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final int overScrollDeltaY = 30;
        final int oneStep = 1;

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, null);

        int overScrollCallCount = overScrollByCallbackHelper.getCallCount();
        AwTestTouchUtils.dragCompleteView(testContainerView,
                0, 0,
                0, overScrollDeltaY,
                oneStep);
        overScrollByCallbackHelper.waitForCallback(overScrollCallCount);
        assertEquals(0, overScrollByCallbackHelper.getDeltaX());
        assertTrue(0 > overScrollByCallbackHelper.getDeltaY());

        assertScrollOnMainSync(testContainerView, 0, 0);
    }
}
