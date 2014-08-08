// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.MediumTest;
import android.view.KeyEvent;
import android.view.View;

import junit.framework.Assert;

import org.chromium.android_webview.test.util.VideoTestWebServer;
import org.chromium.base.CommandLine;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content.common.ContentSwitches;

/**
 * Test WebChromeClient::onShow/HideCustomView.
 */
public class AwContentsClientFullScreenVideoTest extends AwTestBase {
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
    public void testOnShowAndHideCustomViewWithCallback() throws Throwable {
        doOnShowAndHideCustomViewTest(new Runnable() {
            @Override
            public void run() {
                mContentsClient.getExitCallback().onCustomViewHidden();
            }
        });
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowAndHideCustomViewWithBackKeyLegacy() throws Throwable {
        // When html controls are enabled we skip this test because pressing the back key
        // moves away from the current activity instead of exiting fullscreen mode.
        if (areHtmlControlsEnabled())
            return;

        doOnShowAndHideCustomViewTest(new Runnable() {
            @Override
            public void run() {
                View customView = mContentsClient.getCustomView();
                customView.dispatchKeyEvent(
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK));
                customView.dispatchKeyEvent(
                        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK));
            }
        });
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowAndHideCustomViewWithJavascript() throws Throwable {
        doOnShowAndHideCustomViewTest(new Runnable() {
            @Override
            public void run() {
                DOMUtils.exitFullscreen(mContentViewCore);
            }
        });
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowCustomViewAndPlayWithHtmlControl() throws Throwable {
        if (!areHtmlControlsEnabled())
            return;

        doOnShowCustomViewTest();
        Assert.assertFalse(DOMUtils.hasVideoEnded(
                mContentViewCore, VideoTestWebServer.VIDEO_ID));

        // Click the html play button that is rendered above the video right in the middle
        // of the custom view. Note that we're not able to get the precise location of the
        // control since it is a shadow element, so this test might break if the location
        // ever moves.
        TouchCommon touchCommon = new TouchCommon(
                AwContentsClientFullScreenVideoTest.this);
        touchCommon.singleClickView(mContentsClient.getCustomView());

        Assert.assertTrue(DOMUtils.waitForEndOfVideo(
                mContentViewCore, VideoTestWebServer.VIDEO_ID));
    }

    private static boolean areHtmlControlsEnabled() {
        return !CommandLine.getInstance().hasSwitch(
                ContentSwitches.DISABLE_OVERLAY_FULLSCREEN_VIDEO_SUBTITLE);
    }

    private void doOnShowAndHideCustomViewTest(final Runnable existFullscreen)
            throws Throwable {
        doOnShowCustomViewTest();
        getInstrumentation().runOnMainSync(existFullscreen);
        mContentsClient.waitForCustomViewHidden();
    }

    private void doOnShowCustomViewTest() throws Exception {
        loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                mWebServer.getFullScreenVideoTestURL());

        // Click the button in full_screen_video_test.html to enter fullscreen.
        TouchCommon touchCommon = new TouchCommon(this);
        touchCommon.singleClickView(mTestContainerView);
        mContentsClient.waitForCustomViewShown();
    }
}
