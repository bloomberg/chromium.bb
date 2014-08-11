// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.VideoTestWebServer;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.TouchCommon;

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
                new FullScreenVideoTestAwContentsClient(getActivity()) {
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
        VideoTestWebServer webServer = new VideoTestWebServer(
                getInstrumentation().getTargetContext());
        try {
            loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                    webServer.getFullScreenVideoTestURL());
            Thread.sleep(5 * 1000);
            TouchCommon touchCommon = new TouchCommon(this);
            touchCommon.singleClickView(testContainerView);
            waitForViewAttached();
        } finally {
            if (webServer != null) webServer.getTestWebServer().shutdown();
        }
    }
}
