// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContents.DependencyFactory;
import org.chromium.android_webview.AwContents.InternalAccessDelegate;
import org.chromium.android_webview.AwContents.NativeDrawGLFunctorFactory;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwSettings;
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
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
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
    private MockAwContents mAwContents;
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

    // Blank blue pages
    private static final String SAFE_HTML_PATH = RESOURCE_PATH + "/safe.html";
    private static final String PHISHING_HTML_PATH = RESOURCE_PATH + "/phishing.html";
    private static final String MALWARE_HTML_PATH = RESOURCE_PATH + "/malware.html";
    private static final String UNWANTED_SOFTWARE_HTML_PATH =
            RESOURCE_PATH + "/unwanted_software.html";

    // A gray page with an iframe to MALWARE_HTML_PATH
    private static final String IFRAME_HTML_PATH = RESOURCE_PATH + "/iframe.html";

    private static final String INTERSTITIAL_PAGE_TITLE = "Security error";

    /**
     * A fake SafeBrowsingApiHandler which treats URLs ending in MALWARE_HTML_PATH as malicious URLs
     * that should be blocked.
     */
    public static class MockSafeBrowsingApiHandler implements SafeBrowsingApiHandler {
        private Observer mObserver;
        private static final String SAFE_METADATA = "{}";
        private static final String PHISHING_METADATA = "{\"matches\":[{\"threat_type\":\"5\"}]}";
        private static final String MALWARE_METADATA = "{\"matches\":[{\"threat_type\":\"4\"}]}";
        private static final String UNWANTED_SOFTWARE_METADATA =
                "{\"matches\":[{\"threat_type\":\"3\"}]}";

        @Override
        public boolean init(Context context, Observer result) {
            mObserver = result;
            return true;
        }

        @Override
        public void startUriLookup(final long callbackId, String uri, int[] threatsOfInterest) {
            final String metadata;
            if (uri.endsWith(PHISHING_HTML_PATH)) {
                metadata = PHISHING_METADATA;
            } else if (uri.endsWith(MALWARE_HTML_PATH)) {
                metadata = MALWARE_METADATA;
            } else if (uri.endsWith(UNWANTED_SOFTWARE_HTML_PATH)) {
                metadata = UNWANTED_SOFTWARE_METADATA;
            } else {
                metadata = SAFE_METADATA;
            }

            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mObserver.onUrlCheckDone(callbackId, STATUS_SUCCESS, metadata);
                }
            });
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

    private static class MockAwContents extends AwContents {
        private boolean mCanShowInterstitial;
        private boolean mCanShowBigInterstitial;

        public MockAwContents(AwBrowserContext browserContext, ViewGroup containerView,
                Context context, InternalAccessDelegate internalAccessAdapter,
                NativeDrawGLFunctorFactory nativeDrawGLFunctorFactory,
                AwContentsClient contentsClient, AwSettings settings,
                DependencyFactory dependencyFactory) {
            super(browserContext, containerView, context, internalAccessAdapter,
                    nativeDrawGLFunctorFactory, contentsClient, settings, dependencyFactory);
            mCanShowInterstitial = true;
            mCanShowBigInterstitial = true;
        }

        public void setCanShowInterstitial(boolean able) {
            mCanShowInterstitial = able;
        }

        public void setCanShowBigInterstitial(boolean able) {
            mCanShowBigInterstitial = able;
        }

        @Override
        protected boolean canShowInterstitial() {
            return mCanShowInterstitial;
        }

        @Override
        protected boolean canShowBigInterstitial() {
            return mCanShowBigInterstitial;
        }
    }

    private static class SafeBrowsingDependencyFactory extends AwTestBase.TestDependencyFactory {
        @Override
        public AwContents createAwContents(AwBrowserContext browserContext, ViewGroup containerView,
                Context context, InternalAccessDelegate internalAccessAdapter,
                NativeDrawGLFunctorFactory nativeDrawGLFunctorFactory,
                AwContentsClient contentsClient, AwSettings settings,
                DependencyFactory dependencyFactory) {
            return new MockAwContents(browserContext, containerView, context, internalAccessAdapter,
                    nativeDrawGLFunctorFactory, contentsClient, settings, dependencyFactory);
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mContainerView = createAwTestContainerViewOnMainSync(
                mContentsClient, false, new SafeBrowsingDependencyFactory());
        mAwContents = (MockAwContents) mContainerView.getAwContents();

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

    private void waitForInterstitialToChangeTitle() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return INTERSTITIAL_PAGE_TITLE.equals(mAwContents.getTitle());
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
    public void testSafeBrowsingDoesNotBlockUnwantedSoftwarePages() throws Throwable {
        // TODO(ntfschr): this is a temporary check until we add support for Unwanted Software
        // warnings
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(UNWANTED_SOFTWARE_HTML_PATH);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        waitForVisualStateCallback(mAwContents);
        assertEquals("Target page should be visible", MALWARE_PAGE_BACKGROUND_COLOR,
                GraphicsTestUtils.getPixelColorAtCenterOfView(mAwContents, mContainerView));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingBlocksPhishingPages() throws Throwable {
        loadGreenPage();
        int count = mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        final String responseUrl = mTestServer.getURL(PHISHING_HTML_PATH);
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
        assertEquals(
                ErrorCodeConversionHelper.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
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
        assertEquals(
                ErrorCodeConversionHelper.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
        final String subresourceUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        assertEquals(subresourceUrl, errorHelper.getRequest().url);
        assertFalse(errorHelper.getRequest().isMainFrame);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingDontProceedNavigatesBackForMainFrame() throws Throwable {
        loadGreenPage();
        final String originalTitle = getTitleOnUiThread(mAwContents);
        int interstitialCount =
                mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        loadUrlAsync(mAwContents, responseUrl);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(interstitialCount);
        waitForInterstitialToChangeTitle();
        dontProceedThroughInterstitial();

        // Make sure we navigate back to previous page
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return originalTitle.equals(mAwContents.getTitle());
            }
        });
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingCanBeDisabledPerWebview() throws Throwable {
        getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(false);

        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        waitForVisualStateCallback(mAwContents);
        assertEquals("Target page should be visible", MALWARE_PAGE_BACKGROUND_COLOR,
                GraphicsTestUtils.getPixelColorAtCenterOfView(mAwContents, mContainerView));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingCanBeEnabledPerWebview() throws Throwable {
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        waitForVisualStateCallback(mAwContents);
        assertEquals("Target page should be visible", MALWARE_PAGE_BACKGROUND_COLOR,
                GraphicsTestUtils.getPixelColorAtCenterOfView(mAwContents, mContainerView));

        getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(true);

        loadGreenPage();
        int count = mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
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
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingShowsNetworkErrorForInvisibleViews() throws Throwable {
        mAwContents.setCanShowInterstitial(false);
        mAwContents.setCanShowBigInterstitial(false);
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        OnReceivedError2Helper errorHelper = mContentsClient.getOnReceivedError2Helper();
        int errorCount = errorHelper.getCallCount();
        loadUrlAsync(mAwContents, responseUrl);
        errorHelper.waitForCallback(errorCount);
        assertEquals(
                ErrorCodeConversionHelper.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
        assertEquals("Network error is for the malicious page", responseUrl,
                errorHelper.getRequest().url);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingShowsQuietInterstitialForOddSizedViews() throws Throwable {
        mAwContents.setCanShowBigInterstitial(false);
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
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingCanShowQuietPhishingInterstitial() throws Throwable {
        mAwContents.setCanShowBigInterstitial(false);
        loadGreenPage();
        int count = mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        final String responseUrl = mTestServer.getURL(PHISHING_HTML_PATH);
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
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingProceedQuietInterstitial() throws Throwable {
        mAwContents.setCanShowBigInterstitial(false);
        int interstitialCount =
                mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        final String responseUrl = mTestServer.getURL(PHISHING_HTML_PATH);
        loadUrlAsync(mAwContents, responseUrl);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(interstitialCount);
        proceedThroughInterstitial();
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        waitForVisualStateCallback(mAwContents);
        assertEquals("Target page should be visible", MALWARE_PAGE_BACKGROUND_COLOR,
                GraphicsTestUtils.getPixelColorAtCenterOfView(mAwContents, mContainerView));
    }
}
