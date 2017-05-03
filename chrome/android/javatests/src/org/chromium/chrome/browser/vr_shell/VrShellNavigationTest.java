// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/**
 * End-to-end tests for testing navigation transitions (e.g. link clicking) in VR Browser mode, aka
 * "VR Shell". This may require interacting with WebVR in addition to the VR browser, so inherit
 * from VrTestBase for the WebVR test framework.
 */
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
@CommandLineFlags.Add("enable-features=VrShell")
public class VrShellNavigationTest extends VrTestBase {
    private static final String TEST_PAGE_2D_URL =
            VrTestBase.getHtmlTestFile("test_navigation_2d_page");
    private static final String TEST_PAGE_WEBVR_URL =
            VrTestBase.getHtmlTestFile("test_navigation_webvr_page");

    private enum Page { PAGE_2D, PAGE_WEBVR }
    private enum PresentationMode { NON_PRESENTING, PRESENTING }
    private enum FullscreenMode { NON_FULLSCREENED, FULLSCREENED }

    private String getUrl(Page page) {
        switch (page) {
            case PAGE_2D:
                return TEST_PAGE_2D_URL + "?id=0";
            case PAGE_WEBVR:
                return TEST_PAGE_WEBVR_URL + "?id=0";
            default:
                throw new UnsupportedOperationException("Don't know page type " + page);
        }
    }

    /**
     * Triggers navigation to either a 2D or WebVR page. Similar to
     * {@link ChromeActivityTestCaseBase#loadUrl loadUrl} but makes sure page initiates the
     * navigation. This is desirable since we are testing navigation transitions end-to-end.
     */
    private void navigateTo(final Page to) throws InterruptedException {
        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), new Runnable() {
            @Override
            public void run() {
                runJavaScriptOrFail("window.location.href = '" + getUrl(to) + "';",
                        POLL_TIMEOUT_SHORT_MS, mWebContents);
            }
        }, POLL_TIMEOUT_LONG_MS);
    }

    private void enterFullscreen(ContentViewCore cvc)
            throws InterruptedException, TimeoutException {
        DOMUtils.clickNode(cvc, "fullscreen");
        waitOnJavaScriptStep(cvc.getWebContents());
        assertTrue(DOMUtils.isFullscreen(cvc.getWebContents()));
    }

    private void enterPresentation(ContentViewCore cvc)
            throws InterruptedException, TimeoutException {
        // TODO(bsheedy): check if we could use DOMUtils.clickNode in VrTestBase#enterVrTap and
        // then use VrTestBase#enterVrTap here.
        DOMUtils.clickNode(cvc, "webgl-canvas");
        waitOnJavaScriptStep(mWebContents);
        assertTrue(VrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());
    }

    private void assertState(WebContents wc, Page page, PresentationMode presentationMode,
            FullscreenMode fullscreenMode) throws InterruptedException, TimeoutException {
        assertTrue("Browser is in VR", VrShellDelegate.isInVr());
        assertEquals("Browser is on correct web site", getUrl(page), wc.getVisibleUrl());
        assertEquals("Browser is in VR Presentation Mode",
                presentationMode == PresentationMode.PRESENTING,
                VrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());
        assertEquals("Browser is in fullscreen", fullscreenMode == FullscreenMode.FULLSCREENED,
                DOMUtils.isFullscreen(wc));
    }

    @Override
    public int loadUrl(String url, long secondsToWait)
            throws IllegalArgumentException, InterruptedException {
        int result = super.loadUrl(url, secondsToWait);
        waitOnJavaScriptStep(getActivity().getActivityTab().getWebContents());
        return result;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        VrUtils.forceEnterVr();
        VrUtils.waitForVrSupported(POLL_TIMEOUT_LONG_MS);
    }

    /**
     * Tests navigation from a 2D to a 2D page.
     */
    @MediumTest
    public void test2dTo2d() throws InterruptedException, TimeoutException {
        loadUrl(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);

        navigateTo(Page.PAGE_2D);

        assertState(mWebContents, Page.PAGE_2D, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a 2D to a WebVR page.
     */
    @CommandLineFlags.Add("enable-webvr")
    @MediumTest
    public void test2dToWebVr()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        loadUrl(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);

        navigateTo(Page.PAGE_WEBVR);

        assertState(mWebContents, Page.PAGE_WEBVR, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a fullscreened 2D to a WebVR page.
     */
    @CommandLineFlags.Add("enable-webvr")
    @MediumTest
    public void test2dFullscreenToWebVr()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        loadUrl(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);
        enterFullscreen(getActivity().getActivityTab().getContentViewCore());

        navigateTo(Page.PAGE_WEBVR);

        assertState(mWebContents, Page.PAGE_WEBVR, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a WebVR to a 2D page.
     */
    @CommandLineFlags.Add("enable-webvr")
    @MediumTest
    public void testWebVrTo2d()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        loadUrl(TEST_PAGE_WEBVR_URL, PAGE_LOAD_TIMEOUT_S);

        navigateTo(Page.PAGE_2D);

        assertState(mWebContents, Page.PAGE_2D, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a WebVR to a WebVR page.
     */
    @CommandLineFlags.Add("enable-webvr")
    @MediumTest
    public void testWebVrToWebVr()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        loadUrl(TEST_PAGE_WEBVR_URL, PAGE_LOAD_TIMEOUT_S);

        navigateTo(Page.PAGE_WEBVR);

        assertState(mWebContents, Page.PAGE_WEBVR, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a presenting WebVR to a 2D page.
     */
    @CommandLineFlags.Add("enable-webvr")
    @MediumTest
    public void testWebVrPresentingTo2d()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        loadUrl(TEST_PAGE_WEBVR_URL, PAGE_LOAD_TIMEOUT_S);
        enterPresentation(getActivity().getActivityTab().getContentViewCore());

        navigateTo(Page.PAGE_2D);

        assertState(mWebContents, Page.PAGE_2D, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a presenting WebVR to a WebVR page.
     */
    @CommandLineFlags.Add("enable-webvr")
    @MediumTest
    public void testWebVrPresentingToWebVr()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        loadUrl(TEST_PAGE_WEBVR_URL, PAGE_LOAD_TIMEOUT_S);
        enterPresentation(getActivity().getActivityTab().getContentViewCore());

        navigateTo(Page.PAGE_WEBVR);

        assertState(mWebContents, Page.PAGE_WEBVR, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a fullscreened WebVR to a 2D page.
     */
    @CommandLineFlags.Add("enable-webvr")
    @MediumTest
    public void testWebVrFullscreenTo2d()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        loadUrl(TEST_PAGE_WEBVR_URL, PAGE_LOAD_TIMEOUT_S);
        enterFullscreen(getActivity().getActivityTab().getContentViewCore());

        navigateTo(Page.PAGE_2D);

        assertState(mWebContents, Page.PAGE_2D, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a fullscreened WebVR to a WebVR page.
     */
    @CommandLineFlags.Add("enable-webvr")
    @MediumTest
    public void testWebVrFullscreenToWebVr()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        loadUrl(TEST_PAGE_WEBVR_URL, PAGE_LOAD_TIMEOUT_S);
        enterFullscreen(getActivity().getActivityTab().getContentViewCore());

        navigateTo(Page.PAGE_WEBVR);

        assertState(mWebContents, Page.PAGE_WEBVR, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }
}
