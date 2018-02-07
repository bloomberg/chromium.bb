// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr_shell.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr_shell.util.VrInfoBarUtils;
import org.chromium.chrome.browser.vr_shell.util.XrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/**
 * End-to-end tests for testing navigation transitions (e.g. link clicking) in VR Browser mode, aka
 * "VR Shell". This is a temporary class for testing navigation with WebXR - it will be merged with
 * VrShellNavigationTest once WebVR is replaced with WebXR.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=WebXR"})
@Restriction(RESTRICTION_TYPE_DEVICE_DAYDREAM)
public class VrShellNavigationTestWebXr {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    private XrTestFramework mXrTestFramework;

    private static final String TEST_PAGE_2D_URL =
            XrTestFramework.getHtmlTestFile("test_navigation_2d_page");
    private static final String TEST_PAGE_WEBXR_URL =
            XrTestFramework.getHtmlTestFile("test_navigation_webxr_page");

    private enum Page { PAGE_2D, PAGE_WEBXR }
    private enum PresentationMode { NON_PRESENTING, PRESENTING }
    private enum FullscreenMode { NON_FULLSCREENED, FULLSCREENED }

    @Before
    public void setUp() throws Exception {
        mXrTestFramework = new XrTestFramework(mVrTestRule);
        XrTransitionUtils.forceEnterVr();
        XrTransitionUtils.waitForVrEntry(POLL_TIMEOUT_LONG_MS);
    }

    private String getUrl(Page page) {
        switch (page) {
            case PAGE_2D:
                return TEST_PAGE_2D_URL + "?id=0";
            case PAGE_WEBXR:
                return TEST_PAGE_WEBXR_URL + "?id=0";
            default:
                throw new UnsupportedOperationException("Don't know page type " + page);
        }
    }

    /**
     * Triggers navigation to either a 2D or WebXR page. Similar to
     * {@link ChromeActivityTestRule#loadUrl loadUrl} but makes sure page initiates the
     * navigation. This is desirable since we are testing navigation transitions end-to-end.
     */
    private void navigateTo(final Page to) throws InterruptedException {
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), new Runnable() {
                    @Override
                    public void run() {
                        XrTestFramework.runJavaScriptOrFail(
                                "window.location.href = '" + getUrl(to) + "';",
                                POLL_TIMEOUT_SHORT_MS, mXrTestFramework.getFirstTabWebContents());
                    }
                }, POLL_TIMEOUT_LONG_MS);
    }

    private void enterFullscreenOrFail(ContentViewCore cvc)
            throws InterruptedException, TimeoutException {
        DOMUtils.clickNode(cvc, "fullscreen", false /* goThroughRootAndroidView */);
        XrTestFramework.waitOnJavaScriptStep(cvc.getWebContents());
        Assert.assertTrue(DOMUtils.isFullscreen(cvc.getWebContents()));
    }

    private void assertState(WebContents wc, Page page, PresentationMode presentationMode,
            FullscreenMode fullscreenMode) throws InterruptedException, TimeoutException {
        Assert.assertTrue("Browser is in VR", VrShellDelegate.isInVr());
        Assert.assertEquals("Browser is on correct web site", getUrl(page), wc.getVisibleUrl());
        Assert.assertEquals("Browser is in VR Presentation Mode",
                presentationMode == PresentationMode.PRESENTING,
                TestVrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());
        Assert.assertEquals("Browser is in fullscreen",
                fullscreenMode == FullscreenMode.FULLSCREENED, DOMUtils.isFullscreen(wc));
        // Feedback infobar should never show up during navigations.
        VrInfoBarUtils.expectInfoBarPresent(mXrTestFramework, false);
    }

    /**
     * Tests navigation from a 2D to a WebXR page.
     */
    @Test
    @MediumTest
    public void test2dToWebXr()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        mXrTestFramework.loadUrlAndAwaitInitialization(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);

        navigateTo(Page.PAGE_WEBXR);

        assertState(mXrTestFramework.getFirstTabWebContents(), Page.PAGE_WEBXR,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a fullscreened 2D to a WebXR page.
     */
    @Test
    @MediumTest
    public void test2dFullscreenToWebXr()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        mXrTestFramework.loadUrlAndAwaitInitialization(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);
        enterFullscreenOrFail(mXrTestFramework.getFirstTabCvc());

        navigateTo(Page.PAGE_WEBXR);

        assertState(mXrTestFramework.getFirstTabWebContents(), Page.PAGE_WEBXR,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a WebXR to a 2D page.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testWebXrTo2d()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        mXrTestFramework.loadUrlAndAwaitInitialization(TEST_PAGE_WEBXR_URL, PAGE_LOAD_TIMEOUT_S);

        navigateTo(Page.PAGE_2D);

        assertState(mXrTestFramework.getFirstTabWebContents(), Page.PAGE_2D,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a WebVR to a WebVR page.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testWebXrToWebXr()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        mXrTestFramework.loadUrlAndAwaitInitialization(TEST_PAGE_WEBXR_URL, PAGE_LOAD_TIMEOUT_S);

        navigateTo(Page.PAGE_WEBXR);

        assertState(mXrTestFramework.getFirstTabWebContents(), Page.PAGE_WEBXR,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a presenting WebXR to a 2D page.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testWebXrPresentingTo2d()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        mXrTestFramework.loadUrlAndAwaitInitialization(TEST_PAGE_WEBXR_URL, PAGE_LOAD_TIMEOUT_S);
        XrTransitionUtils.enterPresentationOrFail(mXrTestFramework.getFirstTabCvc());

        navigateTo(Page.PAGE_2D);

        assertState(mXrTestFramework.getFirstTabWebContents(), Page.PAGE_2D,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a presenting WebXR to a WebXR page.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testWebXrPresentingToWebXr()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        mXrTestFramework.loadUrlAndAwaitInitialization(TEST_PAGE_WEBXR_URL, PAGE_LOAD_TIMEOUT_S);
        XrTransitionUtils.enterPresentationOrFail(mXrTestFramework.getFirstTabCvc());

        navigateTo(Page.PAGE_WEBXR);

        assertState(mXrTestFramework.getFirstTabWebContents(), Page.PAGE_WEBXR,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a fullscreened WebXR to a 2D page.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testWebXrFullscreenTo2d()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        mXrTestFramework.loadUrlAndAwaitInitialization(TEST_PAGE_WEBXR_URL, PAGE_LOAD_TIMEOUT_S);
        enterFullscreenOrFail(mXrTestFramework.getFirstTabCvc());

        navigateTo(Page.PAGE_2D);

        assertState(mXrTestFramework.getFirstTabWebContents(), Page.PAGE_2D,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a fullscreened WebXR to a WebXR page.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testWebXrFullscreenToWebXr()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        mXrTestFramework.loadUrlAndAwaitInitialization(TEST_PAGE_WEBXR_URL, PAGE_LOAD_TIMEOUT_S);
        enterFullscreenOrFail(mXrTestFramework.getFirstTabCvc());

        navigateTo(Page.PAGE_WEBXR);

        assertState(mXrTestFramework.getFirstTabWebContents(), Page.PAGE_WEBXR,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }
}
