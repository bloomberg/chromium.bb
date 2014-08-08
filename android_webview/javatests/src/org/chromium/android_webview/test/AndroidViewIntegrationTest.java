// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwLayoutSizer;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.ui.gfx.DeviceDisplayInfo;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for certain edge cases related to integrating with the Android view system.
 */
public class AndroidViewIntegrationTest extends AwTestBase {
    private static final int CONTENT_SIZE_CHANGE_STABILITY_TIMEOUT_MS = 1000;

    private static class OnContentSizeChangedHelper extends CallbackHelper {
        private int mWidth;
        private int mHeight;

        public int getWidth() {
            assert getCallCount() > 0;
            return mWidth;
        }

        public int getHeight() {
            assert getCallCount() > 0;
            return mHeight;
        }

        public void onContentSizeChanged(int widthCss, int heightCss) {
            mWidth = widthCss;
            mHeight = heightCss;
            notifyCalled();
        }
    }

    private OnContentSizeChangedHelper mOnContentSizeChangedHelper =
        new OnContentSizeChangedHelper();
    private CallbackHelper mOnPageScaleChangedHelper = new CallbackHelper();

    private class TestAwLayoutSizer extends AwLayoutSizer {
        @Override
        public void onContentSizeChanged(int widthCss, int heightCss) {
            super.onContentSizeChanged(widthCss, heightCss);
            if (mOnContentSizeChangedHelper != null)
                mOnContentSizeChangedHelper.onContentSizeChanged(widthCss, heightCss);
        }

        @Override
        public void onPageScaleChanged(float pageScaleFactor) {
            super.onPageScaleChanged(pageScaleFactor);
            if (mOnPageScaleChangedHelper != null)
                mOnPageScaleChangedHelper.notifyCalled();
        }
    }

    @Override
    protected TestDependencyFactory createTestDependencyFactory() {
        return new TestDependencyFactory() {
            @Override
            public AwLayoutSizer createLayoutSizer() {
                return new TestAwLayoutSizer();
            }
        };
    }

    final LinearLayout.LayoutParams mWrapContentLayoutParams =
        new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);

    private AwTestContainerView createCustomTestContainerViewOnMainSync(
            final AwContentsClient awContentsClient, final int visibility) throws Exception {
        final AtomicReference<AwTestContainerView> testContainerView =
                new AtomicReference<AwTestContainerView>();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                testContainerView.set(createAwTestContainerView(awContentsClient));
                testContainerView.get().setLayoutParams(mWrapContentLayoutParams);
                testContainerView.get().setVisibility(visibility);
            }
        });
        return testContainerView.get();
    }

    private AwTestContainerView createDetachedTestContainerViewOnMainSync(
            final AwContentsClient awContentsClient) {
        final AtomicReference<AwTestContainerView> testContainerView =
                new AtomicReference<AwTestContainerView>();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                testContainerView.set(createDetachedAwTestContainerView(awContentsClient));
            }
        });
        return testContainerView.get();
    }

    private void assertZeroHeight(final AwTestContainerView testContainerView) throws Throwable {
        // Make sure the test isn't broken by the view having a non-zero height.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                assertEquals(0, testContainerView.getHeight());
            }
        });
    }

    private int getRootLayoutWidthOnMainThread() throws Exception {
        final AtomicReference<Integer> width = new AtomicReference<Integer>();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                width.set(Integer.valueOf(getActivity().getRootLayoutWidth()));
            }
        });
        return width.get();
    }

    /**
     * This checks for issues related to loading content into a 0x0 view.
     *
     * A 0x0 sized view is common if the WebView is set to wrap_content and newly created. The
     * expected behavior is for the WebView to expand after some content is loaded.
     * In Chromium it would be valid to not load or render content into a WebContents with a 0x0
     * view (since the user can't see it anyway) and only do so after the view's size is non-zero.
     * Such behavior is unacceptable for the WebView and this test is to ensure that such behavior
     * is not re-introduced.
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testZeroByZeroViewLoadsContent() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView = createCustomTestContainerViewOnMainSync(
                contentsClient, View.VISIBLE);
        assertZeroHeight(testContainerView);

        final int contentSizeChangeCallCount = mOnContentSizeChangedHelper.getCallCount();
        final int pageScaleChangeCallCount = mOnPageScaleChangedHelper.getCallCount();
        loadUrlAsync(testContainerView.getAwContents(), CommonResources.ABOUT_HTML);
        mOnPageScaleChangedHelper.waitForCallback(pageScaleChangeCallCount);
        mOnContentSizeChangedHelper.waitForCallback(contentSizeChangeCallCount);
        assertTrue(mOnContentSizeChangedHelper.getHeight() > 0);
    }

    /**
     * Check that a content size change notification is issued when the view is invisible.
     *
     * This makes sure that any optimizations related to the view's visibility don't inhibit
     * the ability to load pages. Many applications keep the WebView hidden when it's loading.
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testInvisibleViewLoadsContent() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView = createCustomTestContainerViewOnMainSync(
                contentsClient, View.INVISIBLE);
        assertZeroHeight(testContainerView);

        final int contentSizeChangeCallCount = mOnContentSizeChangedHelper.getCallCount();
        final int pageScaleChangeCallCount = mOnPageScaleChangedHelper.getCallCount();
        loadUrlAsync(testContainerView.getAwContents(), CommonResources.ABOUT_HTML);
        mOnPageScaleChangedHelper.waitForCallback(pageScaleChangeCallCount);
        mOnContentSizeChangedHelper.waitForCallback(contentSizeChangeCallCount);
        assertTrue(mOnContentSizeChangedHelper.getHeight() > 0);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                assertEquals(View.INVISIBLE, testContainerView.getVisibility());
            }
        });
    }

    /**
     * Check that a content size change notification is sent even if the WebView is off screen.
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDisconnectedViewLoadsContent() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createDetachedTestContainerViewOnMainSync(contentsClient);
        assertZeroHeight(testContainerView);

        final int contentSizeChangeCallCount = mOnContentSizeChangedHelper.getCallCount();
        final int pageScaleChangeCallCount = mOnPageScaleChangedHelper.getCallCount();
        loadUrlAsync(testContainerView.getAwContents(), CommonResources.ABOUT_HTML);
        mOnPageScaleChangedHelper.waitForCallback(pageScaleChangeCallCount);
        mOnContentSizeChangedHelper.waitForCallback(contentSizeChangeCallCount);
        assertTrue(mOnContentSizeChangedHelper.getHeight() > 0);
    }

    private String makeHtmlPageOfSize(int widthCss, int heightCss, boolean heightPercent) {
        String content = "<div class=\"normal\">a</div>";
        if (heightPercent)
            content += "<div class=\"heightPercent\"></div>";
        return CommonResources.makeHtmlPageFrom(
            "<style type=\"text/css\">" +
                "body { margin:0px; padding:0px; } " +
                ".normal { " +
                   "width:" + widthCss + "px; " +
                   "height:" + heightCss + "px; " +
                   "background-color: red; " +
                 "} " +
                 ".heightPercent { " +
                   "height: 150%; " +
                   "background-color: blue; " +
                 "} " +
            "</style>", content);
    }

    private void waitForContentSizeToChangeTo(OnContentSizeChangedHelper helper, int callCount,
            int widthCss, int heightCss) throws Exception {
        final int maxSizeChangeNotificationsToWaitFor = 5;
        for (int i = 1; i <= maxSizeChangeNotificationsToWaitFor; i++) {
            helper.waitForCallback(callCount, i);
            if ((heightCss == -1 || helper.getHeight() == heightCss) &&
                    (widthCss == -1 || helper.getWidth() == widthCss)) {
                break;
            }
            // This means that we hit the max number of iterations but the expected contents size
            // wasn't reached.
            assertTrue(i != maxSizeChangeNotificationsToWaitFor);
        }
    }

    private void loadPageOfSizeAndWaitForSizeChange(AwContents awContents,
            OnContentSizeChangedHelper helper, int widthCss, int heightCss,
            boolean heightPercent) throws Exception {

        final String htmlData = makeHtmlPageOfSize(widthCss, heightCss, heightPercent);
        final int contentSizeChangeCallCount = helper.getCallCount();
        loadDataAsync(awContents, htmlData, "text/html", false);

        waitForContentSizeToChangeTo(helper, contentSizeChangeCallCount, widthCss, heightCss);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSizeUpdateWhenDetached() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView = createDetachedTestContainerViewOnMainSync(
                contentsClient);
        assertZeroHeight(testContainerView);

        final int contentWidthCss = 142;
        final int contentHeightCss = 180;

        loadPageOfSizeAndWaitForSizeChange(testContainerView.getAwContents(),
                mOnContentSizeChangedHelper, contentWidthCss, contentHeightCss, false);
    }

    public void waitForNoLayoutsPending() throws InterruptedException {
        // This is to make sure that there are no more pending size change notifications. Ideally
        // we'd assert that the renderer is idle (has no pending layout passes) but that would
        // require quite a bit of plumbing, so we just wait a bit and make sure the size hadn't
        // changed.
        Thread.sleep(CONTENT_SIZE_CHANGE_STABILITY_TIMEOUT_MS);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAbsolutePositionContributesToContentSize() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView = createDetachedTestContainerViewOnMainSync(
                contentsClient);
        assertZeroHeight(testContainerView);

        final int widthCss = 142;
        final int heightCss = 180;

        final String htmlData = CommonResources.makeHtmlPageFrom(
            "<style type=\"text/css\">" +
                "body { margin:0px; padding:0px; } " +
                "div { " +
                   "position: absolute; " +
                   "width:" + widthCss + "px; " +
                   "height:" + heightCss + "px; " +
                   "background-color: red; " +
                 "} " +
            "</style>", "<div>a</div>");

        final int contentSizeChangeCallCount = mOnContentSizeChangedHelper.getCallCount();
        loadDataAsync(testContainerView.getAwContents(), htmlData, "text/html", false);

        waitForContentSizeToChangeTo(mOnContentSizeChangedHelper, contentSizeChangeCallCount,
                widthCss, heightCss);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testViewSizedCorrectlyInWrapContentMode() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView = createCustomTestContainerViewOnMainSync(
                contentsClient, View.VISIBLE);
        assertZeroHeight(testContainerView);

        final double deviceDIPScale =
            DeviceDisplayInfo.create(testContainerView.getContext()).getDIPScale();

        final int contentWidthCss = 142;
        final int contentHeightCss = 180;

        // In wrap-content mode the AwLayoutSizer will size the view to be as wide as the parent
        // view.
        final int expectedWidthCss =
            (int) Math.ceil(getRootLayoutWidthOnMainThread() / deviceDIPScale);
        final int expectedHeightCss = contentHeightCss;

        loadPageOfSizeAndWaitForSizeChange(testContainerView.getAwContents(),
                mOnContentSizeChangedHelper, expectedWidthCss, expectedHeightCss, false);

        waitForNoLayoutsPending();
        assertEquals(expectedWidthCss, mOnContentSizeChangedHelper.getWidth());
        assertEquals(expectedHeightCss, mOnContentSizeChangedHelper.getHeight());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testViewSizedCorrectlyInWrapContentModeWithDynamicContents() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView = createCustomTestContainerViewOnMainSync(
                contentsClient, View.VISIBLE);
        assertZeroHeight(testContainerView);

        final double deviceDIPScale =
            DeviceDisplayInfo.create(testContainerView.getContext()).getDIPScale();

        final int contentWidthCss = 142;
        final int contentHeightCss = 180;

        final int expectedWidthCss =
            (int) Math.ceil(getRootLayoutWidthOnMainThread() / deviceDIPScale);
        final int expectedHeightCss = contentHeightCss +
            // The second div in the contents is styled to have 150% of the viewport height, hence
            // the 1.5.
            (int) (AwLayoutSizer.FIXED_LAYOUT_HEIGHT * 1.5);

        loadPageOfSizeAndWaitForSizeChange(testContainerView.getAwContents(),
                mOnContentSizeChangedHelper, expectedWidthCss, contentHeightCss, true);

        waitForNoLayoutsPending();
        assertEquals(expectedWidthCss, mOnContentSizeChangedHelper.getWidth());
        assertEquals(expectedHeightCss, mOnContentSizeChangedHelper.getHeight());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testReceivingSizeAfterLoadUpdatesLayout() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView = createDetachedTestContainerViewOnMainSync(
                contentsClient);
        final AwContents awContents = testContainerView.getAwContents();

        final double deviceDIPScale =
            DeviceDisplayInfo.create(testContainerView.getContext()).getDIPScale();
        final int physicalWidth = 600;
        final int spanWidth = 42;
        final int expectedWidthCss =
            (int) Math.ceil(physicalWidth / deviceDIPScale);

        StringBuilder htmlBuilder = new StringBuilder("<html><body style='margin:0px;'>");
        final String spanBlock =
            "<span style='width: " + spanWidth + "px; display: inline-block;'>a</span>";
        for (int i = 0; i < 10; ++i) {
            htmlBuilder.append(spanBlock);
        }
        htmlBuilder.append("</body></html>");

        int contentSizeChangeCallCount = mOnContentSizeChangedHelper.getCallCount();
        loadDataAsync(awContents, htmlBuilder.toString(), "text/html", false);
        // Because we're loading the contents into a detached WebView its layout size is 0x0 and as
        // a result of that the paragraph will be formated such that each word is on a separate
        // line.
        waitForContentSizeToChangeTo(mOnContentSizeChangedHelper, contentSizeChangeCallCount,
                spanWidth, -1);

        final int narrowLayoutHeight = mOnContentSizeChangedHelper.getHeight();

        contentSizeChangeCallCount = mOnContentSizeChangedHelper.getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                testContainerView.onSizeChanged(physicalWidth, 0, 0, 0);
            }
        });
        mOnContentSizeChangedHelper.waitForCallback(contentSizeChangeCallCount);

        // As a result of calling the onSizeChanged method the layout size should be updated to
        // match the width of the webview and the text we previously loaded should reflow making the
        // contents width match the WebView width.
        assertEquals(expectedWidthCss, mOnContentSizeChangedHelper.getWidth());
        assertTrue(mOnContentSizeChangedHelper.getHeight() < narrowLayoutHeight);
        assertTrue(mOnContentSizeChangedHelper.getHeight() > 0);
    }
}
