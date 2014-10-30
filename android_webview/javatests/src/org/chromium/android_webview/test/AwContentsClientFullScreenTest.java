// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.android_webview.test.util.JavascriptEventObserver;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TouchCommon;

import java.util.concurrent.TimeoutException;

/**
 * Test the fullscreen API (WebChromeClient::onShow/HideCustomView).
 *
 * <p>Fullscreen support follows a different code path depending on whether
 * the element is a video or not, so we test we both. For non-video elements
 * we pick a div containing a video and custom html controls since this is a
 * very common use case.
 */
public class AwContentsClientFullScreenTest extends AwTestBase {
    private static final String VIDEO_TEST_URL =
            "file:///android_asset/full_screen_video_test.html";
    private static final String VIDEO_INSIDE_DIV_TEST_URL =
            "file:///android_asset/full_screen_video_inside_div_test.html";

    // These values must be kept in sync with the strings in
    // full_screen_video_test.html, full_screen_video_inside_div_test.html and
    // full_screen_video.js.
    private static final String VIDEO_ID = "video";
    private static final String CUSTOM_PLAY_CONTROL_ID = "playControl";
    private static final String CUSTOM_FULLSCREEN_CONTROL_ID = "fullscreenControl";
    private static final String FULLSCREEN_ERROR_OBSERVER = "javaFullScreenErrorObserver";

    private FullScreenVideoTestAwContentsClient mContentsClient;
    private ContentViewCore mContentViewCore;
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
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowAndHideCustomViewWithCallback_video() throws Throwable {
        doTestOnShowAndHideCustomViewWithCallback(VIDEO_TEST_URL);
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowAndHideCustomViewWithCallback_videoInsideDiv() throws Throwable {
        doTestOnShowAndHideCustomViewWithCallback(VIDEO_INSIDE_DIV_TEST_URL);
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
        doTestOnShowAndHideCustomViewWithJavascript(VIDEO_TEST_URL);
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowAndHideCustomViewWithJavascript_videoInsideDiv()
            throws Throwable {
        doTestOnShowAndHideCustomViewWithJavascript(VIDEO_INSIDE_DIV_TEST_URL);
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
        doTestOnShowCustomViewAndPlayWithHtmlControl(VIDEO_TEST_URL);
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowCustomViewAndPlayWithHtmlControl_videoInsideDiv() throws Throwable {
        doTestOnShowCustomViewAndPlayWithHtmlControl(VIDEO_INSIDE_DIV_TEST_URL);
    }

    public void doTestOnShowCustomViewAndPlayWithHtmlControl(String videoTestUrl) throws Throwable {
        doOnShowCustomViewTest(videoTestUrl);
        assertTrue(DOMUtils.isVideoPaused(mContentViewCore.getWebContents(), VIDEO_ID));

        tapPlayButton();
        assertTrue(DOMUtils.waitForVideoPlay(mContentViewCore.getWebContents(), VIDEO_ID));
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testFullscreenNotSupported_video() throws Throwable {
        doTestFullscreenNotSupported(VIDEO_TEST_URL);
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testFullscreenNotSupported_videoInsideDiv() throws Throwable {
        doTestFullscreenNotSupported(VIDEO_INSIDE_DIV_TEST_URL);
    }

    public void doTestFullscreenNotSupported(String videoTestUrl) throws Throwable {
        mTestContainerView.getAwContents().getSettings().setFullscreenSupported(false);
        final JavascriptEventObserver fullscreenErrorObserver = registerObserver(
                FULLSCREEN_ERROR_OBSERVER);

        loadTestPageAndClickFullscreen(videoTestUrl);

        assertTrue(fullscreenErrorObserver.waitForEvent(WAIT_TIMEOUT_MS));
        assertFalse(mContentsClient.wasCustomViewShownCalled());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPowerSaveBlockerIsEnabledDuringFullscreenPlayback_video()
            throws Throwable {
        doTestPowerSaveBlockerIsEnabledDuringFullscreenPlayback(VIDEO_TEST_URL);
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPowerSaveBlockerIsEnabledDuringFullscreenPlayback_videoInsideDiv()
            throws Throwable {
        doTestPowerSaveBlockerIsEnabledDuringFullscreenPlayback(VIDEO_INSIDE_DIV_TEST_URL);
    }

    public void doTestPowerSaveBlockerIsEnabledDuringFullscreenPlayback(String videoTestUrl)
            throws Throwable {
        // Enter fullscreen.
        doOnShowCustomViewTest(videoTestUrl);
        View customView = mContentsClient.getCustomView();

        // No power save blocker is active before playback starts.
        assertKeepScreenOnActive(customView, false);

        // Play and verify that there is an active power save blocker.
        tapPlayButton();
        assertKeepScreenOnActive(customView, true);

        // Stop the video and verify that the power save blocker is gone.
        DOMUtils.pauseVideo(mContentViewCore.getWebContents(), VIDEO_ID);
        assertKeepScreenOnActive(customView, false);
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPowerSaveBlockerIsEnabledDuringEmbeddedPlayback()
            throws Throwable {
        assertFalse(DOMUtils.isFullscreen(mContentViewCore.getWebContents()));
        loadTestPage(VIDEO_INSIDE_DIV_TEST_URL);

        // No power save blocker is active before playback starts.
        assertKeepScreenOnActive(mTestContainerView, false);

        // Play and verify that there is an active power save blocker.
        tapPlayButton();
        assertKeepScreenOnActive(mTestContainerView, true);

        // Stop the video and verify that the power save blocker is gone.
        DOMUtils.pauseVideo(mContentViewCore.getWebContents(), VIDEO_ID);
        assertKeepScreenOnActive(mTestContainerView, false);
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPowerSaveBlockerIsTransferredToFullscreen()
            throws Throwable {
        assertFalse(DOMUtils.isFullscreen(mContentViewCore.getWebContents()));
        loadTestPage(VIDEO_INSIDE_DIV_TEST_URL);

        // Play and verify that there is an active power save blocker.
        tapPlayButton();
        assertKeepScreenOnActive(mTestContainerView, true);

        // Enter fullscreen and verify that the power save blocker is
        // still there.
        DOMUtils.clickNode(this, mContentViewCore, CUSTOM_FULLSCREEN_CONTROL_ID);
        mContentsClient.waitForCustomViewShown();
        View customView = mContentsClient.getCustomView();
        assertKeepScreenOnActive(customView, true);

        // Pause the video and the power save blocker is gone.
        DOMUtils.pauseVideo(mContentViewCore.getWebContents(), VIDEO_ID);
        assertKeepScreenOnActive(customView, false);

        // Exit fullscreen and the power save blocker is still gone.
        DOMUtils.exitFullscreen(mContentViewCore.getWebContents());
        mContentsClient.waitForCustomViewHidden();
        assertKeepScreenOnActive(mTestContainerView, false);
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPowerSaveBlockerIsTransferredToEmbedded()
            throws Throwable {
        // Enter fullscreen.
        doOnShowCustomViewTest(VIDEO_INSIDE_DIV_TEST_URL);
        View customView = mContentsClient.getCustomView();

        // Play and verify that there is an active power save blocker
        // in fullscreen.
        tapPlayButton();
        assertKeepScreenOnActive(customView, true);

        // Exit fullscreen and verify that the power save blocker is
        // still there.
        DOMUtils.exitFullscreen(mContentViewCore.getWebContents());
        mContentsClient.waitForCustomViewHidden();
        assertKeepScreenOnActive(mTestContainerView, true);
    }

    private void tapPlayButton() throws Exception {
        String testUrl = mTestContainerView.getAwContents().getUrl();
        if (VIDEO_INSIDE_DIV_TEST_URL.equals(testUrl)) {
            // VIDEO_INSIDE_DIV_TEST_URL uses a custom play control with known id.
            DOMUtils.clickNode(this, mContentViewCore, CUSTOM_PLAY_CONTROL_ID);
        } else if (VIDEO_TEST_URL.equals(testUrl)
                && DOMUtils.isFullscreen(mContentViewCore.getWebContents())) {
            // VIDEO_TEST_URL uses the standard html5 video controls. The standard
            // html5 controls are shadow html elements without any ids. In fullscreen we can still
            // tap the play button because this is rendered in the center of the custom view.
            // In embedded mode we don't have an easy way of retrieving its location.
            TouchCommon.singleClickView(mContentsClient.getCustomView());
        } else {
            fail("Unable to tap standard html5 play control in embedded mode");
        }
    }

    /**
     * Asserts that the keep screen on property in the given {@code view} is active as
     * {@code expected}. It also verifies that it is only active when the video is playing.
     */
    private void assertKeepScreenOnActive(final View view, final boolean expected)
            throws InterruptedException {
        // We need to poll because it takes time to synchronize the state between the android
        // views and Javascript.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return getKeepScreenOn(view) == expected
                            && DOMUtils.isVideoPaused(mContentViewCore.getWebContents(), VIDEO_ID)
                                != expected;
                } catch (InterruptedException | TimeoutException e) {
                    fail(e.getMessage());
                    return false;
                }
            }
        }));
    }

    private boolean getKeepScreenOn(View view) {
        // The power save blocker is added to a child anchor view,
        // so we need to traverse the hierarchy.
        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                if (getKeepScreenOn(viewGroup.getChildAt(i))) {
                    return true;
                }
            }
        }
        return view.getKeepScreenOn();
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
        loadTestPageAndClickFullscreen(videoTestUrl);
        mContentsClient.waitForCustomViewShown();
        assertTrue(DOMUtils.isFullscreen(mContentViewCore.getWebContents()));
    }

    private void loadTestPageAndClickFullscreen(String videoTestUrl) throws Exception {
        loadTestPage(videoTestUrl);
        DOMUtils.clickNode(this, mContentViewCore, CUSTOM_FULLSCREEN_CONTROL_ID);
    }

    private void loadTestPage(String videoTestUrl) throws Exception {
        loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), videoTestUrl);
    }
}
