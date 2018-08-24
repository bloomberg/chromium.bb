// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertNotEquals;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.test.util.JavascriptEventObserver;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/**
 * Test the fullscreen API (WebChromeClient::onShow/HideCustomView).
 *
 * <p>Fullscreen support follows a different code path depending on whether
 * the element is a video or not, so we test we both. For non-video elements
 * we pick a div containing a video and custom html controls since this is a
 * very common use case.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwContentsClientFullScreenTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

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
    private AwTestContainerView mTestContainerView;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new FullScreenVideoTestAwContentsClient(
                mActivityTestRule.getActivity(), mActivityTestRule.isHardwareAcceleratedTest());
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwActivityTestRule.enableJavaScriptOnUiThread(mTestContainerView.getAwContents());
        mTestContainerView.getAwContents().getSettings().setFullscreenSupported(true);
    }

    /*
    @MediumTest
    @Feature({"AndroidWebView"})
    @DisableHardwareAccelerationForTest
    */
    @Test
    @DisabledTest(message = "crbug.com/618749")
    public void testFullscreenVideoInSoftwareModeDoesNotDeadlock() throws Throwable {
        // Although fullscreen video is not supported without hardware acceleration
        // we should not deadlock if apps try to use it.
        loadTestPageAndClickFullscreen(VIDEO_TEST_URL);
        mContentsClient.waitForCustomViewShown();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mContentsClient.getExitCallback().onCustomViewHidden());
        mContentsClient.waitForCustomViewHidden();
    }

    /*
    @MediumTest
    @Feature({"AndroidWebView"})
    @DisableHardwareAccelerationForTest
    */
    @Test
    @DisabledTest(message = "crbug.com/618749")
    public void testFullscreenForNonVideoElementIsSupportedInSoftwareMode() throws Throwable {
        // Fullscreen for non-video elements is supported and works as expected. Note that
        // this test is the same as testOnShowAndHideCustomViewWithCallback_videoInsideDiv below.
        doTestOnShowAndHideCustomViewWithCallback(VIDEO_INSIDE_DIV_TEST_URL);
    }

    /*
    @MediumTest
    @Feature({"AndroidWebView"})
    */
    @Test
    @DisabledTest(message = "crbug.com/618749")
    public void testOnShowAndHideCustomViewWithCallback_video() throws Throwable {
        doTestOnShowAndHideCustomViewWithCallback(VIDEO_TEST_URL);
    }

    /*
    @MediumTest
    @Feature({"AndroidWebView"})
    */
    @Test
    @DisabledTest(message = "crbug.com/618749")
    public void testOnShowAndHideCustomViewWithCallback_videoInsideDiv() throws Throwable {
        doTestOnShowAndHideCustomViewWithCallback(VIDEO_INSIDE_DIV_TEST_URL);
    }

    public void doTestOnShowAndHideCustomViewWithCallback(String videoTestUrl) throws Throwable {
        doOnShowAndHideCustomViewTest(videoTestUrl,
                () -> mContentsClient.getExitCallback().onCustomViewHidden());
    }

    /*
    @MediumTest
    @Feature({"AndroidWebView"})
    */
    @Test
    @DisabledTest(message = "crbug.com/618749")
    public void testOnShowAndHideCustomViewWithJavascript_video() throws Throwable {
        doTestOnShowAndHideCustomViewWithJavascript(VIDEO_TEST_URL);
    }

    /*
    @MediumTest
    @Feature({"AndroidWebView"})
    */
    @Test
    @DisabledTest(message = "crbug.com/618749")
    public void testOnShowAndHideCustomViewWithJavascript_videoInsideDiv() throws Throwable {
        doTestOnShowAndHideCustomViewWithJavascript(VIDEO_INSIDE_DIV_TEST_URL);
    }

    public void doTestOnShowAndHideCustomViewWithJavascript(String videoTestUrl) throws Throwable {
        doOnShowAndHideCustomViewTest(
                videoTestUrl, () -> DOMUtils.exitFullscreen(mTestContainerView.getWebContents()));
    }

    /*
    @MediumTest
    @Feature({"AndroidWebView"})
    // Originally flaked only in multi-process mode (http://crbug.com/616501)
    */
    @Test
    @DisabledTest(message = "crbug.com/618749")
    public void testOnShowAndHideCustomViewWithBackKey_video() throws Throwable {
        doTestOnShowAndHideCustomViewWithBackKey(VIDEO_TEST_URL);
    }

    /*
    @MediumTest
    @Feature({"AndroidWebView"})
    */
    @Test
    @DisabledTest(message = "crbug.com/618749")
    public void testOnShowAndHideCustomViewWithBackKey_videoInsideDiv() throws Throwable {
        doTestOnShowAndHideCustomViewWithBackKey(VIDEO_INSIDE_DIV_TEST_URL);
    }

    public void doTestOnShowAndHideCustomViewWithBackKey(String videoTestUrl) throws Throwable {
        doOnShowCustomViewTest(videoTestUrl);

        // The key event should not be propagated to mTestContainerView (the original container
        // view).
        mTestContainerView.setOnKeyListener((v, keyCode, event) -> {
            Assert.fail("mTestContainerView received key event");
            return false;
        });

        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        mContentsClient.waitForCustomViewHidden();
        Assert.assertFalse(mContentsClient.wasOnUnhandledKeyUpEventCalled());
        assertWaitForIsEmbedded();
    }

    @Test
    //@MediumTest
    //@Feature({"AndroidWebView"})
    @DisabledTest(message = "crbug.com/789306")
    public void testExitFullscreenEndsIfAppInvokesCallbackFromOnHideCustomView() throws Throwable {
        mContentsClient.setOnHideCustomViewRunnable(
                () -> mContentsClient.getExitCallback().onCustomViewHidden());
        doTestOnShowAndHideCustomViewWithCallback(VIDEO_TEST_URL);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowCustomViewAndPlayWithHtmlControl_video() throws Throwable {
        doTestOnShowCustomViewAndPlayWithHtmlControl(VIDEO_TEST_URL);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowCustomViewAndPlayWithHtmlControl_videoInsideDiv() throws Throwable {
        doTestOnShowCustomViewAndPlayWithHtmlControl(VIDEO_INSIDE_DIV_TEST_URL);
    }

    public void doTestOnShowCustomViewAndPlayWithHtmlControl(String videoTestUrl) throws Throwable {
        doOnShowCustomViewTest(videoTestUrl);
        Assert.assertTrue(DOMUtils.isMediaPaused(getWebContentsOnUiThread(), VIDEO_ID));

        tapPlayButton();
        DOMUtils.waitForMediaPlay(getWebContentsOnUiThread(), VIDEO_ID);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testFullscreenNotSupported_video() throws Throwable {
        doTestFullscreenNotSupported(VIDEO_TEST_URL);
    }

    @Test
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

        Assert.assertTrue(fullscreenErrorObserver.waitForEvent(WAIT_TIMEOUT_MS));
        Assert.assertFalse(mContentsClient.wasCustomViewShownCalled());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPowerSaveBlockerIsEnabledDuringFullscreenPlayback_video() throws Throwable {
        doTestPowerSaveBlockerIsEnabledDuringFullscreenPlayback(VIDEO_TEST_URL);
    }

    @Test
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
        assertWaitForKeepScreenOnActive(customView, true);

        // Stop the video and verify that the power save blocker is gone.
        DOMUtils.pauseMedia(getWebContentsOnUiThread(), VIDEO_ID);
        assertWaitForKeepScreenOnActive(customView, false);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPowerSaveBlockerIsEnabledDuringEmbeddedPlayback() throws Throwable {
        Assert.assertFalse(DOMUtils.isFullscreen(getWebContentsOnUiThread()));
        loadTestPage(VIDEO_INSIDE_DIV_TEST_URL);

        // No power save blocker is active before playback starts.
        assertKeepScreenOnActive(mTestContainerView, false);

        // Play and verify that there is an active power save blocker.
        tapPlayButton();
        assertWaitForKeepScreenOnActive(mTestContainerView, true);

        // Stop the video and verify that the power save blocker is gone.
        DOMUtils.pauseMedia(getWebContentsOnUiThread(), VIDEO_ID);
        assertWaitForKeepScreenOnActive(mTestContainerView, false);
    }

    /*
    @MediumTest
    @Feature({"AndroidWebView"})
    */
    @Test
    @DisabledTest(message = "crbug.com/808444")
    public void testPowerSaveBlockerIsTransferredToFullscreen() throws Throwable {
        Assert.assertFalse(DOMUtils.isFullscreen(getWebContentsOnUiThread()));
        loadTestPage(VIDEO_INSIDE_DIV_TEST_URL);

        // Play and verify that there is an active power save blocker.
        tapPlayButton();
        assertWaitForKeepScreenOnActive(mTestContainerView, true);

        // Enter fullscreen and verify that the power save blocker is
        // still there.
        DOMUtils.clickNode(mTestContainerView.getWebContents(), CUSTOM_FULLSCREEN_CONTROL_ID);
        mContentsClient.waitForCustomViewShown();
        assertKeepScreenOnActive(mTestContainerView, true);

        // Pause the video and the power save blocker is gone.
        DOMUtils.pauseMedia(getWebContentsOnUiThread(), VIDEO_ID);
        assertWaitForKeepScreenOnActive(mTestContainerView, false);

        // Exit fullscreen and the power save blocker is still gone.
        DOMUtils.exitFullscreen(getWebContentsOnUiThread());
        mContentsClient.waitForCustomViewHidden();
        assertKeepScreenOnActive(mTestContainerView, false);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPowerSaveBlockerIsTransferredToEmbedded() throws Throwable {
        // Enter fullscreen.
        doOnShowCustomViewTest(VIDEO_INSIDE_DIV_TEST_URL);
        View customView = mContentsClient.getCustomView();

        // Play and verify that there is an active power save blocker
        // in fullscreen.
        tapPlayButton();
        assertWaitForKeepScreenOnActive(customView, true);

        // Exit fullscreen and verify that the power save blocker is
        // still there.
        DOMUtils.exitFullscreen(getWebContentsOnUiThread());
        mContentsClient.waitForCustomViewHidden();
        assertKeepScreenOnActive(customView, true);
    }

    private void tapPlayButton() throws Exception {
        String testUrl = mTestContainerView.getAwContents().getUrl();
        if (VIDEO_TEST_URL.equals(testUrl)
                && DOMUtils.isFullscreen(getWebContentsOnUiThread())) {
            // The VIDEO_TEST_URL page goes fullscreen on the <video> element. In fullscreen
            // the button with id CUSTOM_PLAY_CONTROL_ID will not be visible, but the standard
            // html5 video controls are. The standard html5 controls are shadow html elements
            // without any ids so it is difficult to retrieve its precise location. However,
            // a large play control is rendered in the center of the custom view
            // (containing the fullscreen <video>) so we just rely on that fact here.
            TouchCommon.singleClickView(mContentsClient.getCustomView());
        } else {
            DOMUtils.clickNode(mTestContainerView.getWebContents(), CUSTOM_PLAY_CONTROL_ID);
        }
    }

    /**
     * Asserts that the keep screen on property in the given {@code view} is active as
     * {@code expected}. It also verifies that it is only active when the video is playing.
     */
    private void assertWaitForKeepScreenOnActive(final View view, final boolean expected) {
        // We need to poll because it takes time to synchronize the state between the android
        // views and Javascript.
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return getKeepScreenOnOnInstrumentationThread(view) == expected
                            && DOMUtils.isMediaPaused(getWebContentsOnUiThread(), VIDEO_ID)
                            != expected;
                } catch (InterruptedException | TimeoutException e) {
                    Assert.fail(e.getMessage());
                    return false;
                }
            }
        });
    }

    private void assertKeepScreenOnActive(final View view, final boolean expected)
            throws Exception {
        Assert.assertEquals(expected, getKeepScreenOnOnInstrumentationThread(view));
        assertNotEquals(expected, DOMUtils.isMediaPaused(getWebContentsOnUiThread(), VIDEO_ID));
    }

    private boolean getKeepScreenOnOnInstrumentationThread(final View view) {
        try {
            return ThreadUtils.runOnUiThreadBlocking(() -> getKeepScreenOnOnUiThread(view));
        } catch (Exception e) {
            Assert.fail(e.getMessage());
            return false;
        }
    }

    private boolean getKeepScreenOnOnUiThread(View view) {
        // The power save blocker is added to the container view.
        // Search the view hierarchy for it.
        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                if (getKeepScreenOnOnUiThread(viewGroup.getChildAt(i))) {
                    return true;
                }
            }
        }
        return view.getKeepScreenOn();
    }

    private void assertWaitForIsFullscreen() {
        // We need to poll because the Javascript state is updated asynchronously
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.isFullscreen(getWebContentsOnUiThread());
                } catch (InterruptedException | TimeoutException e) {
                    Assert.fail(e.getMessage());
                    return false;
                }
            }
        });
    }

    private void assertWaitForIsEmbedded() {
        // We need to poll because the Javascript state is updated asynchronously
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return !DOMUtils.isFullscreen(getWebContentsOnUiThread());
                } catch (InterruptedException | TimeoutException e) {
                    Assert.fail(e.getMessage());
                    return false;
                }
            }
        });
        // TODO: Test that inline video is actually displayed.
    }

    private JavascriptEventObserver registerObserver(final String observerName) throws Throwable {
        final JavascriptEventObserver observer = new JavascriptEventObserver();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> observer.register(mTestContainerView.getWebContents(), observerName));
        return observer;
    }

    private void doOnShowAndHideCustomViewTest(String videoTestUrl, final Runnable existFullscreen)
            throws Throwable {
        doOnShowCustomViewTest(videoTestUrl);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(existFullscreen);
        mContentsClient.waitForCustomViewHidden();
        assertWaitForIsEmbedded();
    }

    private void doOnShowCustomViewTest(String videoTestUrl) throws Exception {
        loadTestPageAndClickFullscreen(videoTestUrl);
        mContentsClient.waitForCustomViewShown();
        assertWaitForIsFullscreen();
    }

    private void loadTestPageAndClickFullscreen(String videoTestUrl) throws Exception {
        loadTestPage(videoTestUrl);
        DOMUtils.clickNode(mTestContainerView.getWebContents(), CUSTOM_FULLSCREEN_CONTROL_ID);
    }

    private void loadTestPage(String videoTestUrl) throws Exception {
        mActivityTestRule.loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), videoTestUrl);
        // As we are loading a non-trivial page, let's wait until we have something displayed.
        mActivityTestRule.waitForVisualStateCallback(mTestContainerView.getAwContents());
    }

    private WebContents getWebContentsOnUiThread() {
        try {
            return ThreadUtils.runOnUiThreadBlocking(() -> mTestContainerView.getWebContents());
        } catch (Exception e) {
            Assert.fail(e.getMessage());
            return null;
        }
    }
}
