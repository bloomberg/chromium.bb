// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.annotation.SuppressLint;
import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwScrollOffsetManager;
import org.chromium.android_webview.test.AwTestBase.PopupInfo;
import org.chromium.android_webview.test.AwTestBase.TestDependencyFactory;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JavascriptEventObserver;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.parameter.SkipCommandLineParameterization;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.display.DisplayAndroid;

import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for synchronous scrolling.
 */
@RunWith(AwJUnit4ClassRunner.class)
@SuppressFBWarnings("DLS_DEAD_LOCAL_STORE")
public class AndroidScrollIntegrationTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule() {
        @Override
        public TestDependencyFactory createTestDependencyFactory() {
            return new TestDependencyFactory() {
                @Override
                public AwScrollOffsetManager createScrollOffsetManager(
                        AwScrollOffsetManager.Delegate delegate) {
                    return new AwScrollOffsetManager(delegate);
                }
                @Override
                public AwTestContainerView createAwTestContainerView(
                        AwTestRunnerActivity activity, boolean allowHardwareAcceleration) {
                    return new ScrollTestContainerView(activity, allowHardwareAcceleration);
                }
            };
        }

    };

    private TestWebServer mWebServer;

    private static class OverScrollByCallbackHelper extends CallbackHelper {
        int mDeltaX;
        int mDeltaY;
        int mScrollRangeY;

        public int getDeltaX() {
            assert getCallCount() > 0;
            return mDeltaX;
        }

        public int getDeltaY() {
            assert getCallCount() > 0;
            return mDeltaY;
        }

        public int getScrollRangeY() {
            assert getCallCount() > 0;
            return mScrollRangeY;
        }

        public void notifyCalled(int deltaX, int deltaY, int scrollRangeY) {
            mDeltaX = deltaX;
            mDeltaY = deltaY;
            mScrollRangeY = scrollRangeY;
            notifyCalled();
        }
    }

    private static class ScrollTestContainerView extends AwTestContainerView {
        private int mMaxScrollXPix = -1;
        private int mMaxScrollYPix = -1;

        private CallbackHelper mOnScrollToCallbackHelper = new CallbackHelper();
        private OverScrollByCallbackHelper mOverScrollByCallbackHelper =
                new OverScrollByCallbackHelper();

        public ScrollTestContainerView(Context context, boolean allowHardwareAcceleration) {
            super(context, allowHardwareAcceleration);
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
            mOverScrollByCallbackHelper.notifyCalled(deltaX, deltaY, scrollRangeY);
            return super.overScrollBy(deltaX, deltaY, scrollX, scrollY,
                     scrollRangeX, scrollRangeY, maxOverScrollX, maxOverScrollY, isTouchEvent);
        }

        @Override
        public void scrollTo(int x, int y) {
            if (mMaxScrollXPix != -1) x = Math.min(mMaxScrollXPix, x);
            if (mMaxScrollYPix != -1) y = Math.min(mMaxScrollYPix, y);
            super.scrollTo(x, y);
            mOnScrollToCallbackHelper.notifyCalled();
        }
    }

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() throws Exception {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    private static final String TEST_PAGE_COMMON_HEADERS =
            "<meta name=\"viewport\" content=\""
            + "width=device-width, initial-scale=1, minimum-scale=1\"> "
            + "<style type=\"text/css\"> "
            + "   body { "
            + "      margin: 0px; "
            + "   } "
            + "   div { "
            + "      width:10000px; "
            + "      height:10000px; "
            + "      background-color: blue; "
            + "   } "
            + "</style> ";
    private static final String TEST_PAGE_COMMON_CONTENT = "<div>test div</div> ";

    private String makeTestPage(String onscrollObserver, String firstFrameObserver,
            String extraContent) {
        String content = TEST_PAGE_COMMON_CONTENT + extraContent;
        if (onscrollObserver != null) {
            content += "<script> "
                     + "   window.onscroll = function(oEvent) { "
                     + "       " + onscrollObserver + ".notifyJava(); "
                     + "   } "
                     + "</script>";
        }
        if (firstFrameObserver != null) {
            content += "<script> "
                     + "   window.framesToIgnore = 20; "
                     + "   window.onAnimationFrame = function(timestamp) { "
                     + "     if (window.framesToIgnore == 0) { "
                     + "         " + firstFrameObserver + ".notifyJava(); "
                     + "     } else {"
                     + "       window.framesToIgnore -= 1; "
                     + "       window.requestAnimationFrame(window.onAnimationFrame); "
                     + "     } "
                     + "   }; "
                     + "   window.requestAnimationFrame(window.onAnimationFrame); "
                     + "</script>";
        }
        return CommonResources.makeHtmlPageFrom(TEST_PAGE_COMMON_HEADERS, content);
    }

    private void scrollToOnMainSync(final View view, final int xPix, final int yPix) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> view.scrollTo(xPix, yPix));
    }

    private void setMaxScrollOnMainSync(final ScrollTestContainerView testContainerView,
            final int maxScrollXPix, final int maxScrollYPix) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            testContainerView.setMaxScrollX(maxScrollXPix);
            testContainerView.setMaxScrollY(maxScrollYPix);
        });
    }

    private boolean checkScrollOnMainSync(final ScrollTestContainerView testContainerView,
            final int scrollXPix, final int scrollYPix) {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                () -> scrollXPix == testContainerView.getScrollX()
                        && scrollYPix == testContainerView.getScrollY());
    }

    private void assertScrollOnMainSync(final ScrollTestContainerView testContainerView,
            final int scrollXPix, final int scrollYPix) {
        final AtomicInteger scrolledXPix = new AtomicInteger();
        final AtomicInteger scrolledYPix = new AtomicInteger();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            scrolledXPix.set(testContainerView.getScrollX());
            scrolledYPix.set(testContainerView.getScrollY());
        });
        // Actual scrolling is done using this formula:
        // floor (scroll_offset_dip * max_offset) / max_scroll_offset_dip
        // where max_offset is calculated using a ceil operation.
        // This combination of ceiling and flooring can lead to a deviation from the test
        // calculation, which simply uses the more direct:
        // floor (scroll_offset_dip * dip_scale)
        //
        // While the math used in the functional code is correct (See crbug.com/261239), it can't
        // be verified down to the pixel in this test which doesn't have all internal values.
        // In non-rational cases, this can lead to a deviation of up to one pixel when using
        // the floor directly. To accomodate this scenario, the test allows a -1 px deviation.
        //
        // For example, imagine the following valid values:
        // scroll_offset_dip = 132
        // max_offset = 532
        // max_scroll_offset_dip = 399
        // dip_scale = 1.33125
        //
        // The functional code will return
        // floor (132 * 532 / 399) = 176
        // The test code will return
        // floor (132 * 1.33125) = 175
        //
        // For more information, see crbug.com/537343
        Assert.assertTrue("Actual and expected x-scroll offsets do not match. Expected "
                        + scrollXPix + ", actual " + scrolledXPix.get(),
                scrollXPix == scrolledXPix.get() || scrollXPix == scrolledXPix.get() - 1);
        Assert.assertTrue("Actual and expected y-scroll offsets do not match. Expected "
                        + scrollYPix + ", actual " + scrolledYPix.get(),
                scrollYPix == scrolledYPix.get() || scrollYPix == scrolledYPix.get() - 1);
    }

    private void assertScrollInJs(final AwContents awContents,
            final TestAwContentsClient contentsClient,
            final int xCss, final int yCss) throws Exception {
        AwActivityTestRule.pollInstrumentationThread(() -> {
            String x = mActivityTestRule.executeJavaScriptAndWaitForResult(
                    awContents, contentsClient, "window.scrollX");
            String y = mActivityTestRule.executeJavaScriptAndWaitForResult(
                    awContents, contentsClient, "window.scrollY");
            return (Integer.toString(xCss).equals(x)
                    && Integer.toString(yCss).equals(y));
        });
    }

    private void assertScrolledToBottomInJs(final AwContents awContents,
            final TestAwContentsClient contentsClient) throws Exception {
        final String isBottomScript = "window.scrollY == "
                + "(window.document.documentElement.scrollHeight - window.innerHeight)";
        AwActivityTestRule.pollInstrumentationThread(() -> {
            String r = mActivityTestRule.executeJavaScriptAndWaitForResult(
                    awContents, contentsClient, isBottomScript);
            return r.equals("true");
        });
    }

    private void loadTestPageAndWaitForFirstFrame(final ScrollTestContainerView testContainerView,
            final TestAwContentsClient contentsClient,
            final String onscrollObserverName, final String extraContent) throws Exception {
        final JavascriptEventObserver firstFrameObserver = new JavascriptEventObserver();
        final String firstFrameObserverName = "firstFrameObserver";
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> firstFrameObserver.register(testContainerView.getContentViewCore(),
                        firstFrameObserverName));

        mActivityTestRule.loadDataSync(testContainerView.getAwContents(),
                contentsClient.getOnPageFinishedHelper(),
                makeTestPage(onscrollObserverName, firstFrameObserverName, extraContent),
                "text/html", false);

        // We wait for "a couple" of frames for the active tree in CC to stabilize and for pending
        // tree activations to stop clobbering the root scroll layer's scroll offset. This wait
        // doesn't strictly guarantee that but there isn't a good alternative and this seems to
        // work fine.
        firstFrameObserver.waitForEvent(WAIT_TIMEOUT_MS);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUiScrollReflectedInJs() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final double deviceDIPScale =
                DisplayAndroid.getNonMultiDisplay(testContainerView.getContext()).getDipScale();
        final int targetScrollXCss = 233;
        final int targetScrollYCss = 322;
        final int targetScrollXPix = (int) Math.ceil(targetScrollXCss * deviceDIPScale);
        final int targetScrollYPix = (int) Math.ceil(targetScrollYCss * deviceDIPScale);
        final JavascriptEventObserver onscrollObserver = new JavascriptEventObserver();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> onscrollObserver.register(testContainerView.getContentViewCore(),
                        "onscrollObserver"));

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, "onscrollObserver", "");

        scrollToOnMainSync(testContainerView, targetScrollXPix, targetScrollYPix);

        onscrollObserver.waitForEvent(WAIT_TIMEOUT_MS);
        assertScrollInJs(testContainerView.getAwContents(), contentsClient,
                targetScrollXCss, targetScrollYCss);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SuppressLint("DefaultLocale")
    public void testJsScrollReflectedInUi() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final double deviceDIPScale =
                DisplayAndroid.getNonMultiDisplay(testContainerView.getContext()).getDipScale();
        final int targetScrollXCss = 132;
        final int targetScrollYCss = 243;
        final int targetScrollXPix = (int) Math.floor(targetScrollXCss * deviceDIPScale);
        final int targetScrollYPix = (int) Math.floor(targetScrollYCss * deviceDIPScale);

        mActivityTestRule.loadDataSync(testContainerView.getAwContents(),
                contentsClient.getOnPageFinishedHelper(), makeTestPage(null, null, ""), "text/html",
                false);

        final CallbackHelper onScrollToCallbackHelper =
                testContainerView.getOnScrollToCallbackHelper();
        final int scrollToCallCount = onScrollToCallbackHelper.getCallCount();
        mActivityTestRule.executeJavaScriptAndWaitForResult(testContainerView.getAwContents(),
                contentsClient,
                String.format("window.scrollTo(%d, %d);", targetScrollXCss, targetScrollYCss));
        onScrollToCallbackHelper.waitForCallback(scrollToCallCount);

        assertScrollOnMainSync(testContainerView, targetScrollXPix, targetScrollYPix);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testJsScrollFromBody() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final double deviceDIPScale =
                DisplayAndroid.getNonMultiDisplay(testContainerView.getContext()).getDipScale();
        final int targetScrollXCss = 132;
        final int targetScrollYCss = 243;
        final int targetScrollXPix = (int) Math.floor(targetScrollXCss * deviceDIPScale);
        final int targetScrollYPix = (int) Math.floor(targetScrollYCss * deviceDIPScale);

        final String scrollFromBodyScript =
                "<script> "
                + "  window.scrollTo(" + targetScrollXCss + ", " + targetScrollYCss + "); "
                + "</script> ";

        final CallbackHelper onScrollToCallbackHelper =
                testContainerView.getOnScrollToCallbackHelper();
        final int scrollToCallCount = onScrollToCallbackHelper.getCallCount();
        mActivityTestRule.loadDataAsync(testContainerView.getAwContents(),
                makeTestPage(null, null, scrollFromBodyScript), "text/html", false);
        onScrollToCallbackHelper.waitForCallback(scrollToCallCount);

        assertScrollOnMainSync(testContainerView, targetScrollXPix, targetScrollYPix);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testJsScrollCanBeAlteredByUi() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final double deviceDIPScale =
                DisplayAndroid.getNonMultiDisplay(testContainerView.getContext()).getDipScale();
        final int targetScrollXCss = 132;
        final int targetScrollYCss = 243;
        final int targetScrollXPix = (int) Math.floor(targetScrollXCss * deviceDIPScale);
        final int targetScrollYPix = (int) Math.floor(targetScrollYCss * deviceDIPScale);

        final int maxScrollXCss = 101;
        final int maxScrollYCss = 201;
        final int maxScrollXPix = (int) Math.floor(maxScrollXCss * deviceDIPScale);
        final int maxScrollYPix = (int) Math.floor(maxScrollYCss * deviceDIPScale);

        mActivityTestRule.loadDataSync(testContainerView.getAwContents(),
                contentsClient.getOnPageFinishedHelper(), makeTestPage(null, null, ""), "text/html",
                false);

        setMaxScrollOnMainSync(testContainerView, maxScrollXPix, maxScrollYPix);

        final CallbackHelper onScrollToCallbackHelper =
                testContainerView.getOnScrollToCallbackHelper();
        final int scrollToCallCount = onScrollToCallbackHelper.getCallCount();
        mActivityTestRule.executeJavaScriptAndWaitForResult(testContainerView.getAwContents(),
                contentsClient,
                "window.scrollTo(" + targetScrollXCss + "," + targetScrollYCss + ")");
        onScrollToCallbackHelper.waitForCallback(scrollToCallCount);

        assertScrollOnMainSync(testContainerView, maxScrollXPix, maxScrollYPix);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipCommandLineParameterization // crbug.com/616505
    @RetryOnFailure
    public void testTouchScrollCanBeAlteredByUi() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final int dragSteps = 10;
        final int dragStepSize = 24;
        // Watch out when modifying - if the y or x delta aren't big enough vertical or horizontal
        // scroll snapping will kick in.
        final int targetScrollXPix = dragStepSize * dragSteps;
        final int targetScrollYPix = dragStepSize * dragSteps;

        final double deviceDIPScale =
                DisplayAndroid.getNonMultiDisplay(testContainerView.getContext()).getDipScale();
        final int maxScrollXPix = 101;
        final int maxScrollYPix = 211;
        // Make sure we can't hit these values simply as a result of scrolling.
        assert (maxScrollXPix % dragStepSize) != 0;
        assert (maxScrollYPix % dragStepSize) != 0;
        final int maxScrollXCss = (int) Math.floor(maxScrollXPix / deviceDIPScale);
        final int maxScrollYCss = (int) Math.floor(maxScrollYPix / deviceDIPScale);

        setMaxScrollOnMainSync(testContainerView, maxScrollXPix, maxScrollYPix);

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, null, "");

        final CallbackHelper onScrollToCallbackHelper =
                testContainerView.getOnScrollToCallbackHelper();
        final int scrollToCallCount = onScrollToCallbackHelper.getCallCount();
        AwTestTouchUtils.dragCompleteView(testContainerView,
                0, -targetScrollXPix, // these need to be negative as we're scrolling down.
                0, -targetScrollYPix,
                dragSteps);

        for (int i = 1; i <= dragSteps; ++i) {
            onScrollToCallbackHelper.waitForCallback(scrollToCallCount, i);
            if (checkScrollOnMainSync(testContainerView, maxScrollXPix, maxScrollYPix)) break;
        }

        assertScrollOnMainSync(testContainerView, maxScrollXPix, maxScrollYPix);
        assertScrollInJs(testContainerView.getAwContents(), contentsClient,
                maxScrollXCss, maxScrollYCss);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverScrollX() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        final OverScrollByCallbackHelper overScrollByCallbackHelper =
                testContainerView.getOverScrollByCallbackHelper();
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final int overScrollDeltaX = 30;
        final int oneStep = 1;

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, null, "");

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
        Assert.assertTrue(0 > overScrollByCallbackHelper.getDeltaX());
        Assert.assertEquals(0, overScrollByCallbackHelper.getDeltaY());

        assertScrollOnMainSync(testContainerView, 0, 0);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverScrollY() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        final OverScrollByCallbackHelper overScrollByCallbackHelper =
                testContainerView.getOverScrollByCallbackHelper();
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final int overScrollDeltaY = 30;
        final int oneStep = 1;

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, null, "");

        int overScrollCallCount = overScrollByCallbackHelper.getCallCount();
        AwTestTouchUtils.dragCompleteView(testContainerView,
                0, 0,
                0, overScrollDeltaY,
                oneStep);
        overScrollByCallbackHelper.waitForCallback(overScrollCallCount);
        Assert.assertEquals(0, overScrollByCallbackHelper.getDeltaX());
        Assert.assertTrue(0 > overScrollByCallbackHelper.getDeltaY());

        assertScrollOnMainSync(testContainerView, 0, 0);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFlingScroll() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, null, "");

        assertScrollOnMainSync(testContainerView, 0, 0);

        final CallbackHelper onScrollToCallbackHelper =
                testContainerView.getOnScrollToCallbackHelper();
        final int scrollToCallCount = onScrollToCallbackHelper.getCallCount();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> testContainerView.getAwContents().flingScroll(1000, 1000));

        onScrollToCallbackHelper.waitForCallback(scrollToCallCount);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            Assert.assertTrue(testContainerView.getScrollX() > 0);
            Assert.assertTrue(testContainerView.getScrollY() > 0);
        });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFlingScrollOnPopup() throws Throwable {
        final TestAwContentsClient parentContentsClient = new TestAwContentsClient();
        final ScrollTestContainerView parentContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        parentContentsClient);
        final AwContents parentContents = parentContainerView.getAwContents();
        mActivityTestRule.enableJavaScriptOnUiThread(parentContents);

        final String popupPath = "/popup.html";
        final String parentPageHtml = CommonResources.makeHtmlPageFrom("", "<script>"
                        + "function tryOpenWindow() {"
                        + "  var newWindow = window.open('" + popupPath + "');"
                        + "}</script> <h1>Parent</h1>");

        final String popupPageHtml = CommonResources.makeHtmlPageFrom(
                "<title>" + "Popup Window" + "</title>",
                "This is a popup window");

        mActivityTestRule.triggerPopup(parentContents, parentContentsClient, mWebServer,
                parentPageHtml, popupPageHtml, popupPath, "tryOpenWindow()");
        final PopupInfo popupInfo = mActivityTestRule.connectPendingPopup(parentContents);
        Assert.assertEquals(
                "Popup Window", mActivityTestRule.getTitleOnUiThread(popupInfo.popupContents));

        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) popupInfo.popupContainerView;
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());
        loadTestPageAndWaitForFirstFrame(
                testContainerView, popupInfo.popupContentsClient, null, "");

        assertScrollOnMainSync(testContainerView, 0, 0);

        final CallbackHelper onScrollToCallbackHelper =
                testContainerView.getOnScrollToCallbackHelper();
        final int scrollToCallCount = onScrollToCallbackHelper.getCallCount();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> testContainerView.getAwContents().flingScroll(1000, 1000));

        onScrollToCallbackHelper.waitForCallback(scrollToCallCount);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            Assert.assertTrue(testContainerView.getScrollX() > 0);
            Assert.assertTrue(testContainerView.getScrollY() > 0);
        });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPageDown() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, null, "");

        assertScrollOnMainSync(testContainerView, 0, 0);

        final int maxScrollYPix =
                mActivityTestRule.runTestOnUiThreadAndGetResult(
                        () -> (testContainerView.getAwContents().computeVerticalScrollRange()
                                - testContainerView.getHeight()));

        final CallbackHelper onScrollToCallbackHelper =
                testContainerView.getOnScrollToCallbackHelper();
        final int scrollToCallCount = onScrollToCallbackHelper.getCallCount();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> testContainerView.getAwContents().pageDown(true));

        // Wait for the animation to hit the bottom of the page.
        for (int i = 1;; ++i) {
            onScrollToCallbackHelper.waitForCallback(scrollToCallCount, i);
            if (checkScrollOnMainSync(testContainerView, 0, maxScrollYPix)) break;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPageUp() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final double deviceDIPScale =
                DisplayAndroid.getNonMultiDisplay(testContainerView.getContext()).getDipScale();
        final int targetScrollYCss = 243;
        final int targetScrollYPix = (int) Math.ceil(targetScrollYCss * deviceDIPScale);

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, null, "");

        assertScrollOnMainSync(testContainerView, 0, 0);

        scrollToOnMainSync(testContainerView, 0, targetScrollYPix);

        final CallbackHelper onScrollToCallbackHelper =
                testContainerView.getOnScrollToCallbackHelper();
        final int scrollToCallCount = onScrollToCallbackHelper.getCallCount();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> testContainerView.getAwContents().pageUp(true));

        // Wait for the animation to hit the bottom of the page.
        for (int i = 1;; ++i) {
            onScrollToCallbackHelper.waitForCallback(scrollToCallCount, i);
            if (checkScrollOnMainSync(testContainerView, 0, 0)) break;
        }
    }

    private static class TestGestureStateListener extends GestureStateListener {
        private CallbackHelper mOnScrollUpdateGestureConsumedHelper = new CallbackHelper();

        public CallbackHelper getOnScrollUpdateGestureConsumedHelper() {
            return mOnScrollUpdateGestureConsumedHelper;
        }

        @Override
        public void onPinchStarted() {
        }

        @Override
        public void onPinchEnded() {
        }

        @Override
        public void onFlingStartGesture(int scrollOffsetY, int scrollExtentY) {}

        @Override
        public void onScrollUpdateGestureConsumed() {
            mOnScrollUpdateGestureConsumedHelper.notifyCalled();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testTouchScrollingConsumesScrollByGesture() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        final TestGestureStateListener testGestureStateListener = new TestGestureStateListener();
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final int dragSteps = 10;
        final int dragStepSize = 24;
        // Watch out when modifying - if the y or x delta aren't big enough vertical or horizontal
        // scroll snapping will kick in.
        final int targetScrollXPix = dragStepSize * dragSteps;
        final int targetScrollYPix = dragStepSize * dragSteps;

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, null,
                "<div>"
                + "  <div style=\"width:10000px; height: 10000px;\"> force scrolling </div>"
                + "</div>");

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> testContainerView.getContentViewCore().addGestureStateListener(
                        testGestureStateListener));
        final CallbackHelper onScrollUpdateGestureConsumedHelper =
                testGestureStateListener.getOnScrollUpdateGestureConsumedHelper();

        final int callCount = onScrollUpdateGestureConsumedHelper.getCallCount();
        AwTestTouchUtils.dragCompleteView(testContainerView,
                0, -targetScrollXPix, // these need to be negative as we're scrolling down.
                0, -targetScrollYPix,
                dragSteps);
        onScrollUpdateGestureConsumedHelper.waitForCallback(callCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPinchZoomUpdatesScrollRangeSynchronously() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        final OverScrollByCallbackHelper overScrollByCallbackHelper =
                testContainerView.getOverScrollByCallbackHelper();
        final AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.enableJavaScriptOnUiThread(awContents);

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, null, "");

        // Containers to execute asserts on the test thread
        final AtomicBoolean canZoomIn = new AtomicBoolean(false);
        final AtomicReference<Float> atomicOldScale = new AtomicReference<>();
        final AtomicReference<Float> atomicNewScale = new AtomicReference<>();
        final AtomicInteger atomicOldScrollRange = new AtomicInteger();
        final AtomicInteger atomicNewScrollRange = new AtomicInteger();
        final AtomicInteger atomicContentHeight = new AtomicInteger();
        final AtomicInteger atomicOldContentHeightApproximation = new AtomicInteger();
        final AtomicInteger atomicNewContentHeightApproximation = new AtomicInteger();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            canZoomIn.set(awContents.canZoomIn());

            int oldScrollRange =
                    awContents.computeVerticalScrollRange() - testContainerView.getHeight();
            float oldScale = awContents.getScale();
            atomicOldContentHeightApproximation.set(
                    (int) Math.ceil(awContents.computeVerticalScrollRange() / oldScale));

            awContents.zoomIn();

            int newScrollRange =
                    awContents.computeVerticalScrollRange() - testContainerView.getHeight();
            float newScale = awContents.getScale();
            atomicNewContentHeightApproximation.set(
                    (int) Math.ceil(awContents.computeVerticalScrollRange() / newScale));

            atomicOldScale.set(oldScale);
            atomicNewScale.set(newScale);
            atomicOldScrollRange.set(oldScrollRange);
            atomicNewScrollRange.set(newScrollRange);
            atomicContentHeight.set(awContents.getContentHeightCss());
        });
        Assert.assertTrue(canZoomIn.get());
        Assert.assertTrue(
                String.format(Locale.ENGLISH, "Scale range should increase after zoom (%f) > (%f)",
                        atomicNewScale.get(), atomicOldScale.get()),
                atomicNewScale.get() > atomicOldScale.get());
        Assert.assertTrue(
                String.format(Locale.ENGLISH, "Scroll range should increase after zoom (%d) > (%d)",
                        atomicNewScrollRange.get(), atomicOldScrollRange.get()),
                atomicNewScrollRange.get() > atomicOldScrollRange.get());
        Assert.assertTrue(
                String.format(Locale.ENGLISH, "Old content height should be close (%d) ~= (%d)",
                        atomicContentHeight.get(), atomicOldContentHeightApproximation.get()),
                Math.abs(atomicContentHeight.get() - atomicOldContentHeightApproximation.get())
                        <= 1);
        Assert.assertTrue(
                String.format(Locale.ENGLISH, "New content height should be close (%d) ~= (%d)",
                        atomicContentHeight.get(), atomicNewContentHeightApproximation.get()),
                Math.abs(atomicContentHeight.get() - atomicNewContentHeightApproximation.get())
                        <= 1);
    }

    @Test
    @SmallTest
    @Feature("AndroidWebView")
    public void testScrollOffsetAfterCapturePicture() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ScrollTestContainerView testContainerView =
                (ScrollTestContainerView) mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient);
        mActivityTestRule.enableJavaScriptOnUiThread(testContainerView.getAwContents());

        final int targetScrollYPix = 322;

        loadTestPageAndWaitForFirstFrame(testContainerView, contentsClient, null, "");

        assertScrollOnMainSync(testContainerView, 0, 0);

        scrollToOnMainSync(testContainerView, 0, targetScrollYPix);

        final int scrolledYPix =
                mActivityTestRule.runTestOnUiThreadAndGetResult(
                        () -> testContainerView.getScrollY());

        Assert.assertTrue(scrolledYPix > 0);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> testContainerView.getAwContents().capturePicture());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> Assert.assertEquals(testContainerView.getScrollY(), scrolledYPix));
    }
}
