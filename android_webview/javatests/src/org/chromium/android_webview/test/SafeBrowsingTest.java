// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.support.test.filters.SmallTest;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSwitches;
import org.chromium.android_webview.AwWebContentsObserver;
import org.chromium.android_webview.ErrorCodeConversionHelper;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedError2Helper;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.components.safe_browsing.SafeBrowsingApiHandler;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Test suite for SafeBrowsing.
 *
 * Ensures that interstitials can be successfully created for malicous pages.
 */
public class SafeBrowsingTest extends AwTestBase {
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mContainerView;
    private AwContents mAwContents;
    private TestAwWebContentsObserver mWebContentsObserver;

    private EmbeddedTestServer mTestServer;

    // These colors correspond to the body.background attribute in GREEN_HTML_PATH, SAFE_HTML_PATH,
    // MALWARE_HTML_PATH, and IFRAME_HTML_PATH. They should only be changed if those values are
    // changed as well
    private static final int GREEN_PAGE_BACKGROUND_COLOR = Color.rgb(0, 255, 0);
    private static final int MALWARE_PAGE_BACKGROUND_COLOR = Color.rgb(0, 0, 255);
    private static final int IFRAME_EMBEDDER_BACKGROUND_COLOR = Color.rgb(10, 10, 10);

    private static final String RESOURCE_PATH = "/android_webview/test/data";

    // A blank green page
    private static final String GREEN_HTML_PATH = RESOURCE_PATH + "/green.html";

    // Two blank blue pages, one which we treat as a malicious page
    private static final String SAFE_HTML_PATH = RESOURCE_PATH + "/safe.html";
    private static final String MALWARE_HTML_PATH = RESOURCE_PATH + "/malware.html";

    // A gray page with an iframe to MALWARE_HTML_PATH
    private static final String IFRAME_HTML_PATH = RESOURCE_PATH + "/iframe.html";

    /**
     * A fake SafeBrowsingApiHandler which treats URLs ending in MALWARE_HTML_PATH as malicious URLs
     * that should be blocked.
     */
    public static class MockSafeBrowsingApiHandler implements SafeBrowsingApiHandler {
        private Observer mObserver;
        private static final String SAFE_METADATA = "{}";
        private static final String MALWARE_METADATA = "{\"matches\":[{\"threat_type\":\"5\"}]}";

        @Override
        public boolean init(Context context, Observer result) {
            mObserver = result;
            return true;
        }

        @Override
        public void startUriLookup(final long callbackId, String uri, int[] threatsOfInterest) {
            final int resultStatus = STATUS_SUCCESS;
            final String metadata = isMaliciousUrl(uri) ? MALWARE_METADATA : SAFE_METADATA;

            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mObserver.onUrlCheckDone(callbackId, resultStatus, metadata);
                }
            });
        }

        private static boolean isMaliciousUrl(String uri) {
            return uri.endsWith(MALWARE_HTML_PATH);
        }
    }

    /**
     * A fake AwBrowserContext which loads the MockSafeBrowsingApiHandler instead of the real one.
     */
    private static class MockAwBrowserContext extends AwBrowserContext {
        public MockAwBrowserContext(
                SharedPreferences sharedPreferences, Context applicationContext) {
            super(sharedPreferences, applicationContext);
            SafeBrowsingApiBridge.setSafeBrowsingHandlerType(MockSafeBrowsingApiHandler.class);
        }
    }

    private static class TestAwWebContentsObserver extends AwWebContentsObserver {
        private CallbackHelper mDidAttachInterstitialPageHelper;

        public TestAwWebContentsObserver(WebContents webContents, AwContents awContents,
                TestAwContentsClient contentsClient) {
            super(webContents, awContents, contentsClient);
            mDidAttachInterstitialPageHelper = new CallbackHelper();
        }

        public CallbackHelper getAttachedInterstitialPageHelper() {
            return mDidAttachInterstitialPageHelper;
        }

        @Override
        public void didAttachInterstitialPage() {
            mDidAttachInterstitialPageHelper.notifyCalled();
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mContainerView.getAwContents();

        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mWebContentsObserver = new TestAwWebContentsObserver(
                        mContainerView.getContentViewCore().getWebContents(), mAwContents,
                        mContentsClient) {};
            }
        });
    }

    @Override
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Creates a special BrowserContext that has a safebrowsing api handler which always says
     * sites are malicious
     */
    @Override
    protected AwBrowserContext createAwBrowserContextOnUiThread(
            InMemorySharedPreferences prefs, Context appContext) {
        return new MockAwBrowserContext(prefs, appContext);
    }

    private int getPageColor() {
        Bitmap bitmap = GraphicsTestUtils.drawAwContentsOnUiThread(
                mAwContents, mContainerView.getWidth(), mContainerView.getHeight());
        return bitmap.getPixel(0, 0);
    }

    private void loadGreenPage() throws Exception {
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                mTestServer.getURL(GREEN_HTML_PATH));

        // Make sure we actually wait for the page to be visible
        waitForVisualStateCallback(mAwContents);
    }

    private void proceedThroughInterstitial() {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAwContents.callProceedOnInterstitial();
            }
        });
    }

    private void dontProceedThroughInterstitial() {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAwContents.callDontProceedOnInterstitial();
            }
        });
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingDoesNotBlockSafePages() throws Throwable {
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(SAFE_HTML_PATH);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        waitForVisualStateCallback(mAwContents);
        assertEquals("Target page should be visible", MALWARE_PAGE_BACKGROUND_COLOR,
                GraphicsTestUtils.getPixelColorAtCenterOfView(mAwContents, mContainerView));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingShowsInterstitialForMainFrame() throws Throwable {
        loadGreenPage();
        int count = mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        loadUrlAsync(mAwContents, responseUrl);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(count);
        assertTrue("Original page should not be showing",
                GREEN_PAGE_BACKGROUND_COLOR
                        != GraphicsTestUtils.getPixelColorAtCenterOfView(
                                   mAwContents, mContainerView));
        assertTrue("Target page should not be visible",
                MALWARE_PAGE_BACKGROUND_COLOR
                        != GraphicsTestUtils.getPixelColorAtCenterOfView(
                                   mAwContents, mContainerView));
        // Assume that we are rendering the interstitial, since we see neither the previous page nor
        // the target page
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingShowsInterstitialForSubresource() throws Throwable {
        loadGreenPage();
        int count = mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        final String responseUrl = mTestServer.getURL(IFRAME_HTML_PATH);
        loadUrlAsync(mAwContents, responseUrl);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(count);
        assertTrue("Original page should not be showing",
                GREEN_PAGE_BACKGROUND_COLOR
                        != GraphicsTestUtils.getPixelColorAtCenterOfView(
                                   mAwContents, mContainerView));
        assertTrue("Target page should not be visible",
                IFRAME_EMBEDDER_BACKGROUND_COLOR
                        != GraphicsTestUtils.getPixelColorAtCenterOfView(
                                   mAwContents, mContainerView));
        // Assume that we are rendering the interstitial, since we see neither the previous page nor
        // the target page
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingProceedThroughInterstitialForMainFrame() throws Throwable {
        int interstitialCount =
                mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        int pageFinishedCount =
                mContentsClient.getOnPageFinishedHelper().getCallCount();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        loadUrlAsync(mAwContents, responseUrl);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(interstitialCount);
        proceedThroughInterstitial();
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        waitForVisualStateCallback(mAwContents);
        assertEquals("Target page should be visible", MALWARE_PAGE_BACKGROUND_COLOR,
                GraphicsTestUtils.getPixelColorAtCenterOfView(mAwContents, mContainerView));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingCanProceedThroughInterstitialForSubresource() throws Throwable {
        int interstitialCount =
                mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        final String responseUrl = mTestServer.getURL(IFRAME_HTML_PATH);
        loadUrlAsync(mAwContents, responseUrl);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(interstitialCount);
        proceedThroughInterstitial();
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        waitForVisualStateCallback(mAwContents);
        assertEquals("Target page should be visible", IFRAME_EMBEDDER_BACKGROUND_COLOR,
                GraphicsTestUtils.getPixelColorAtCenterOfView(mAwContents, mContainerView));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingDontProceedCausesNetworkErrorForMainFrame() throws Throwable {
        int interstitialCount =
                mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        loadUrlAsync(mAwContents, responseUrl);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(interstitialCount);
        OnReceivedError2Helper errorHelper = mContentsClient.getOnReceivedError2Helper();
        int errorCount = errorHelper.getCallCount();
        dontProceedThroughInterstitial();
        errorHelper.waitForCallback(errorCount);
        assertEquals(ErrorCodeConversionHelper.ERROR_UNKNOWN, errorHelper.getError().errorCode);
        assertEquals("Network error is for the malicious page", responseUrl,
                errorHelper.getRequest().url);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingDontProceedCausesNetworkErrorForSubresource() throws Throwable {
        int interstitialCount =
                mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        final String responseUrl = mTestServer.getURL(IFRAME_HTML_PATH);
        loadUrlAsync(mAwContents, responseUrl);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(interstitialCount);
        OnReceivedError2Helper errorHelper = mContentsClient.getOnReceivedError2Helper();
        int errorCount = errorHelper.getCallCount();
        dontProceedThroughInterstitial();
        errorHelper.waitForCallback(errorCount);
        assertEquals(ErrorCodeConversionHelper.ERROR_UNKNOWN, errorHelper.getError().errorCode);
        final String subresourceUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        assertEquals(subresourceUrl, errorHelper.getRequest().url);
        assertFalse(errorHelper.getRequest().isMainFrame);
    }
}
