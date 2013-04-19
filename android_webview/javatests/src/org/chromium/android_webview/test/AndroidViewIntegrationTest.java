// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;
import android.util.Log;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwLayoutSizer;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.TimeoutException;

/**
 * Tests for certain edge cases related to integrating with the Android view system.
 */
public class AndroidViewIntegrationTest extends AwTestBase {
    private static class OnContentSizeChangedHelper extends CallbackHelper {
        private int mWidth;
        private int mHeight;

        public int getWidth() {
            assert(getCallCount() > 0);
            return mWidth;
        }

        public int getHeight() {
            assert(getCallCount() > 0);
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
            mOnContentSizeChangedHelper.onContentSizeChanged(widthCss, heightCss);
        }

        @Override
        public void onPageScaleChanged(double pageScaleFactor) {
            super.onPageScaleChanged(pageScaleFactor);
            mOnPageScaleChangedHelper.notifyCalled();
        }
    }

    private class DependencyFactory extends TestDependencyFactory {
        @Override
        public AwLayoutSizer createLayoutSizer() {
            return new TestAwLayoutSizer();
        }
    }

    final LinearLayout.LayoutParams wrapContentLayoutParams =
        new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);

    private AwTestContainerView createCustomTestContainerViewOnMainSync(
            final AwContentsClient awContentsClient, final int visibility) throws Exception {
        final AtomicReference<AwTestContainerView> testContainerView =
                new AtomicReference<AwTestContainerView>();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                testContainerView.set(createAwTestContainerView(awContentsClient,
                        new DependencyFactory()));
                testContainerView.get().setLayoutParams(wrapContentLayoutParams);
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
                testContainerView.set(createDetachedAwTestContainerView(awContentsClient,
                        new DependencyFactory()));
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
}
