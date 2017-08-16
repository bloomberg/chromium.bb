// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;
import android.view.View;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.DOMUtils;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test for AwContentClient.GetVideoLoadingProgressView.
 *
 * This test takes advantage of onViewAttachedToWindow, and assume our progress view
 * is shown once it attached to window.
 *
 * As it needs user gesture to switch to the full screen mode video, A large button
 * used to trigger switch occupies almost the whole WebView so the simulated click event
 * can't miss it.
 */
public class AwContentsClientGetVideoLoadingProgressViewTest extends AwTestBase
        implements View.OnAttachStateChangeListener {
    private static final String VIDEO_TEST_URL =
            "file:///android_asset/full_screen_video_test_not_preloaded.html";
    // These values must be kept in sync with the element ids in the above file.
    private static final String CUSTOM_PLAY_CONTROL_ID = "playControl";
    private static final String CUSTOM_FULLSCREEN_CONTROL_ID = "fullscreenControl";

    private CallbackHelper mViewAttachedCallbackHelper = new CallbackHelper();

    @Override
    public void onViewAttachedToWindow(View view) {
        mViewAttachedCallbackHelper.notifyCalled();
        view.removeOnAttachStateChangeListener(this);
    }

    @Override
    public void onViewDetachedFromWindow(View arg0) {
    }

    private void waitForViewAttached() throws InterruptedException, TimeoutException {
        mViewAttachedCallbackHelper.waitForCallback(0, 1, WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testGetVideoLoadingProgressView() throws Throwable {
        TestAwContentsClient contentsClient =
                new FullScreenVideoTestAwContentsClient(
                        getActivity(), isHardwareAcceleratedTest()) {
                    @Override
                    protected View getVideoLoadingProgressView() {
                        View view = new View(getInstrumentation().getTargetContext());
                        view.addOnAttachStateChangeListener(
                                AwContentsClientGetVideoLoadingProgressViewTest.this);
                        return view;
                    }
                };
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        awContents.getSettings().setFullscreenSupported(true);
        enableJavaScriptOnUiThread(awContents);
        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), VIDEO_TEST_URL);
        Thread.sleep(5 * 1000);
        DOMUtils.clickNode(awContents.getContentViewCore(), CUSTOM_FULLSCREEN_CONTROL_ID);
        Thread.sleep(1 * 1000);
        DOMUtils.clickNode(awContents.getContentViewCore(), CUSTOM_PLAY_CONTROL_ID);
        waitForViewAttached();
    }
}
