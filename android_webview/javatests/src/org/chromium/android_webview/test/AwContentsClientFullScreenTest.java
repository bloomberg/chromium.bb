// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.android_webview.test.util.JavascriptEventObserver;
import org.chromium.android_webview.test.util.VideoTestWebServer;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TouchCommon;

/**
 * Test the fullscreen API (WebChromeClient::onShow/HideCustomView).
 *
 * <p>Fullscreen support follows a different code path depending on whether
 * the element is a video or not, so we test we both. For non-video elements
 * we pick a div containing a video and custom html controls since this is a
 * very common use case.
 */
public class AwContentsClientFullScreenTest extends AwTestBase {
    private FullScreenVideoTestAwContentsClient mContentsClient;
    private ContentViewCore mContentViewCore;
    private VideoTestWebServer mWebServer;
    private AwTestContainerView mTestContainerView;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContentsClient = new FullScreenVideoTestAwContentsClient(getActivity());
        mTestContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mContentViewCore = mTestContainerView.getContentViewCore();
        enableJavaScriptOnUiThread(mTestContainerView.getAwContents());
        mTestContainerView.getAwContents().getSettings().setFullscreenSupported(true);
        mWebServer = new VideoTestWebServer(
                getInstrumentation().getTargetContext());
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        if (mWebServer != null) mWebServer.getTestWebServer().shutdown();
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowAndHideCustomViewWithCallback_video() throws Throwable {
        doTestOnShowAndHideCustomViewWithCallback(getFullScreenVideoTestUrl());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowAndHideCustomViewWithCallback_videoInsideDiv() throws Throwable {
        doTestOnShowAndHideCustomViewWithCallback(getFullScreenVideoInsideDivTestUrl());
    }

    public void doTestOnShowAndHideCustomViewWithCallback(String videoTestUrl) throws Throwable {
        doOnShowAndHideCustomViewTest(videoTestUrl, new Runnable() {
            @Override
            public void run() {
                mContentsClient.getExitCallback().onCustomViewHidden();
            }
        });
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowAndHideCustomViewWithJavascript_video() throws Throwable {
        doTestOnShowAndHideCustomViewWithJavascript(getFullScreenVideoTestUrl());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowAndHideCustomViewWithJavascript_videoInsideDiv()
            throws Throwable {
        doTestOnShowAndHideCustomViewWithJavascript(getFullScreenVideoInsideDivTestUrl());
    }

    public void doTestOnShowAndHideCustomViewWithJavascript(String videoTestUrl) throws Throwable {
        doOnShowAndHideCustomViewTest(videoTestUrl, new Runnable() {
            @Override
            public void run() {
                DOMUtils.exitFullscreen(mContentViewCore.getWebContents());
            }
        });
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowCustomViewAndPlayWithHtmlControl_video() throws Throwable {
        doTestOnShowCustomViewAndPlayWithHtmlControl(getFullScreenVideoTestUrl());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowCustomViewAndPlayWithHtmlControl_videoInsideDiv() throws Throwable {
        doTestOnShowCustomViewAndPlayWithHtmlControl(getFullScreenVideoInsideDivTestUrl());
    }

    public void doTestOnShowCustomViewAndPlayWithHtmlControl(String videoTestUrl) throws Throwable {
        final JavascriptEventObserver onPlayObserver = registerObserver("javaOnPlayObserver");

        doOnShowCustomViewTest(videoTestUrl);

        // Click the html play button that is rendered above the video right in the middle
        // of the custom view. Note that we're not able to get the precise location of the
        // control since it is a shadow element, so this test might break if the location
        // ever moves.
        TouchCommon touchCommon = new TouchCommon(
                AwContentsClientFullScreenTest.this);
        touchCommon.singleClickView(mContentsClient.getCustomView());

        assertTrue(onPlayObserver.waitForEvent(WAIT_TIMEOUT_MS));
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testFullscreenNotSupported_video() throws Throwable {
        doTestFullscreenNotSupported(getFullScreenVideoTestUrl());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testFullscreenNotSupported_videoInsideDiv() throws Throwable {
        doTestFullscreenNotSupported(getFullScreenVideoInsideDivTestUrl());
    }

    public void doTestFullscreenNotSupported(String videoTestUrl) throws Throwable {
        mTestContainerView.getAwContents().getSettings().setFullscreenSupported(false);

        final JavascriptEventObserver fullScreenErrorObserver = registerObserver(
                "javaFullScreenErrorObserver");

        loadTestPageAndClickFullscreen(videoTestUrl);

        assertTrue(fullScreenErrorObserver.waitForEvent(WAIT_TIMEOUT_MS));
        assertFalse(mContentsClient.wasCustomViewShownCalled());
    }

    private JavascriptEventObserver registerObserver(final String observerName) {
        final JavascriptEventObserver observer = new JavascriptEventObserver();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                observer.register(mContentViewCore, observerName);
            }
        });
        return observer;
    }

    private void doOnShowAndHideCustomViewTest(String videoTestUrl, final Runnable existFullscreen)
            throws Throwable {
        doOnShowCustomViewTest(videoTestUrl);
        getInstrumentation().runOnMainSync(existFullscreen);
        mContentsClient.waitForCustomViewHidden();
    }

    private void doOnShowCustomViewTest(String videoTestUrl) throws Exception {
        final JavascriptEventObserver onEnterFullscreenObserver =
                registerObserver("javaOnEnterFullscreen");
        loadTestPageAndClickFullscreen(videoTestUrl);
        mContentsClient.waitForCustomViewShown();
        assertTrue(onEnterFullscreenObserver.waitForEvent(WAIT_TIMEOUT_MS));
    }

    private void loadTestPageAndClickFullscreen(String videoTestUrl) throws Exception {
        loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), videoTestUrl);

        // Click the button in full_screen_video_test.html to enter fullscreen.
        TouchCommon touchCommon = new TouchCommon(this);
        touchCommon.singleClickView(mTestContainerView);
    }

    private String getFullScreenVideoTestUrl() {
        return mWebServer.getFullScreenVideoTestURL();
    }

    private String getFullScreenVideoInsideDivTestUrl() {
        return mWebServer.getFullScreenVideoInsideDivTestURL();
    }
}
