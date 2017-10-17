// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertNotEquals;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContents.DependencyFactory;
import org.chromium.android_webview.AwContents.InternalAccessDelegate;
import org.chromium.android_webview.AwContents.NativeDrawGLFunctorFactory;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwSafeBrowsingConfigHelper;
import org.chromium.android_webview.AwSafeBrowsingConversionHelper;
import org.chromium.android_webview.AwSafeBrowsingResponse;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.AwSwitches;
import org.chromium.android_webview.AwWebContentsObserver;
import org.chromium.android_webview.ErrorCodeConversionHelper;
import org.chromium.android_webview.SafeBrowsingAction;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedError2Helper;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.base.Callback;
import org.chromium.base.LocaleUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.components.safe_browsing.SafeBrowsingApiHandler;
import org.chromium.components.safe_browsing.SafeBrowsingApiHandler.Observer;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Test suite for SafeBrowsing.
 *
 * Ensures that interstitials can be successfully created for malicous pages.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class SafeBrowsingTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule() {

        /**
         * Creates a special BrowserContext that has a safebrowsing api handler which always says
         * sites are malicious
         */
        @Override
        public AwBrowserContext createAwBrowserContextOnUiThread(
                InMemorySharedPreferences prefs, Context appContext) {
            return new MockAwBrowserContext(prefs, appContext);
        }
    };

    private SafeBrowsingContentsClient mContentsClient;
    private AwTestContainerView mContainerView;
    private MockAwContents mAwContents;
    private TestAwWebContentsObserver mWebContentsObserver;

    private EmbeddedTestServer mTestServer;

    // Used to check which thread a callback is invoked on.
    private volatile boolean mOnUiThread;

    // These colors correspond to the body.background attribute in GREEN_HTML_PATH, SAFE_HTML_PATH,
    // MALWARE_HTML_PATH, IFRAME_HTML_PATH, etc. They should only be changed if those values are
    // changed as well
    private static final int GREEN_PAGE_BACKGROUND_COLOR = Color.rgb(0, 255, 0);
    private static final int SAFE_PAGE_BACKGROUND_COLOR = Color.rgb(0, 0, 255);
    private static final int PHISHING_PAGE_BACKGROUND_COLOR = Color.rgb(0, 0, 255);
    private static final int MALWARE_PAGE_BACKGROUND_COLOR = Color.rgb(0, 0, 255);
    private static final int UNWANTED_SOFTWARE_PAGE_BACKGROUND_COLOR = Color.rgb(0, 0, 255);
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

    // These URLs will be CTS-tested and should not be changed.
    private static final String WEB_UI_MALWARE_URL = "chrome://safe-browsing/match?type=malware";
    private static final String WEB_UI_PHISHING_URL = "chrome://safe-browsing/match?type=phishing";

    /**
     * A fake SafeBrowsingApiHandler which treats URLs ending in MALWARE_HTML_PATH as malicious URLs
     * that should be blocked.
     */
    public static class MockSafeBrowsingApiHandler implements SafeBrowsingApiHandler {
        private Observer mObserver;
        private static final String SAFE_METADATA = "{}";
        private static final int PHISHING_CODE = 5;
        private static final int MALWARE_CODE = 4;
        private static final int UNWANTED_SOFTWARE_CODE = 3;

        @Override
        public boolean init(Context context, Observer result) {
            mObserver = result;
            return true;
        }

        private String buildMetadataFromCode(int code) {
            return "{\"matches\":[{\"threat_type\":\"" + code + "\"}]}";
        }

        @Override
        public void startUriLookup(final long callbackId, String uri, int[] threatsOfInterest) {
            final String metadata;
            Arrays.sort(threatsOfInterest);

            // TODO(ntfschr): remove this assert once we support Unwanted Software warnings
            // (crbug/729272)
            Assert.assertEquals(Arrays.binarySearch(threatsOfInterest, UNWANTED_SOFTWARE_CODE), -1);

            if (uri.endsWith(PHISHING_HTML_PATH)
                    && Arrays.binarySearch(threatsOfInterest, PHISHING_CODE) >= 0) {
                metadata = buildMetadataFromCode(PHISHING_CODE);
            } else if (uri.endsWith(MALWARE_HTML_PATH)
                    && Arrays.binarySearch(threatsOfInterest, MALWARE_CODE) >= 0) {
                metadata = buildMetadataFromCode(MALWARE_CODE);
            } else if (uri.endsWith(UNWANTED_SOFTWARE_HTML_PATH)
                    && Arrays.binarySearch(threatsOfInterest, UNWANTED_SOFTWARE_CODE) >= 0) {
                metadata = buildMetadataFromCode(UNWANTED_SOFTWARE_CODE);
            } else {
                metadata = SAFE_METADATA;
            }

            ThreadUtils.runOnUiThread(
                    (Runnable) () -> mObserver.onUrlCheckDone(callbackId, STATUS_SUCCESS,
                            metadata));
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

    /**
     * An AwContentsClient with customizable behavior for onSafeBrowsingHit().
     */
    private static class SafeBrowsingContentsClient extends TestAwContentsClient {
        private AwWebResourceRequest mLastRequest;
        private int mLastThreatType;
        private int mAction = SafeBrowsingAction.SHOW_INTERSTITIAL;
        private boolean mReporting = true;

        @Override
        public void onSafeBrowsingHit(AwWebResourceRequest request, int threatType,
                Callback<AwSafeBrowsingResponse> callback) {
            mLastRequest = request;
            mLastThreatType = threatType;
            callback.onResult(new AwSafeBrowsingResponse(mAction, mReporting));
        }

        public AwWebResourceRequest getLastRequest() {
            return mLastRequest;
        }

        public int getLastThreatType() {
            return mLastThreatType;
        }

        public void setSafeBrowsingAction(int action) {
            mAction = action;
        }

        public void setReporting(boolean value) {
            mReporting = value;
        }
    }

    private static class SafeBrowsingDependencyFactory
            extends AwActivityTestRule.TestDependencyFactory {
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

    private static class JavaScriptHelper extends CallbackHelper {
        private String mValue;

        public void setValue(String s) {
            mValue = s;
        }

        public String getValue() {
            return mValue;
        }
    }

    private static class WhitelistHelper extends CallbackHelper implements Callback<Boolean> {
        public boolean success;

        public void onResult(Boolean success) {
            this.success = success;
            notifyCalled();
        }
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new SafeBrowsingContentsClient();
        mContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(
                mContentsClient, false, new SafeBrowsingDependencyFactory());
        mAwContents = (MockAwContents) mContainerView.getAwContents();

        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mWebContentsObserver = new TestAwWebContentsObserver(
                        mContainerView.getContentViewCore().getWebContents(), mAwContents,
                        mContentsClient) {
                });
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    private int getPageColor() {
        Bitmap bitmap = GraphicsTestUtils.drawAwContentsOnUiThread(
                mAwContents, mContainerView.getWidth(), mContainerView.getHeight());
        return bitmap.getPixel(0, 0);
    }

    private void loadGreenPage() throws Exception {
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                mTestServer.getURL(GREEN_HTML_PATH));

        // Make sure we actually wait for the page to be visible
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
    }

    private void evaluateJavaScriptOnInterstitialOnUiThread(
            final String script, final Callback<String> callback) {
        ThreadUtils.runOnUiThread(
                () -> mAwContents.evaluateJavaScriptOnInterstitialForTesting(script, callback));
    }

    private void waitForInterstitialToLoad() throws Exception {
        final String script = "document.readyState;";
        final JavaScriptHelper helper = new JavaScriptHelper();

        final Callback<String> callback = value -> {
            helper.setValue(value);
            helper.notifyCalled();
        };

        final String expected = "\"complete\"";
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    final int count = helper.getCallCount();
                    evaluateJavaScriptOnInterstitialOnUiThread(script, callback);
                    helper.waitForCallback(count);
                    return expected.equals(helper.getValue());
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        });
    }

    private void clickBackToSafety() {
        clickLinkById("primary-button");
    }

    private void clickVisitUnsafePageQuietInterstitial() {
        clickLinkById("details-link");
        clickLinkById("proceed-link");
    }

    private void clickVisitUnsafePage() {
        clickLinkById("details-button");
        clickLinkById("proceed-link");
    }

    private void clickLinkById(String id) {
        final String script = "document.getElementById('" + id + "').click();";
        evaluateJavaScriptOnInterstitialOnUiThread(script, null);
    }

    private void waitForInterstitialToChangeTitle() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return INTERSTITIAL_PAGE_TITLE.equals(mAwContents.getTitle());
            }
        });
    }

    private void loadPathAndWaitForInterstitial(final String path) throws Exception {
        int interstitialCount =
                mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        final String responseUrl = mTestServer.getURL(path);
        mActivityTestRule.loadUrlAsync(mAwContents, responseUrl);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(interstitialCount);
    }

    private void assertTargetPageHasLoaded(int pageColor) throws Exception {
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
        Assert.assertEquals("Target page should be visible", colorToString(pageColor),
                colorToString(GraphicsTestUtils.getPixelColorAtCenterOfView(
                        mAwContents, mContainerView)));
    }

    private void assertGreenPageShowing() throws Exception {
        Assert.assertEquals("Original page should be showing",
                colorToString(GREEN_PAGE_BACKGROUND_COLOR),
                colorToString(GraphicsTestUtils.getPixelColorAtCenterOfView(
                        mAwContents, mContainerView)));
    }

    private void assertGreenPageNotShowing() throws Exception {
        assertNotEquals("Original page should not be showing",
                colorToString(GREEN_PAGE_BACKGROUND_COLOR),
                colorToString(GraphicsTestUtils.getPixelColorAtCenterOfView(
                        mAwContents, mContainerView)));
    }

    private void assertTargetPageNotShowing(int pageColor) throws Exception {
        assertNotEquals("Target page should not be showing", colorToString(pageColor),
                colorToString(GraphicsTestUtils.getPixelColorAtCenterOfView(
                        mAwContents, mContainerView)));
    }

    /**
     * Converts a color from the confusing integer representation to a more readable string
     * respresentation. There is a 1:1 mapping between integer and string representations, so it's
     * valid to compare strings directly. The string representation is better for assert output.
     *
     * @param color integer representation of the color
     * @return a String representation of the color in RGBA format
     */
    private String colorToString(int color) {
        return "(" + Color.red(color) + "," + Color.green(color) + "," + Color.blue(color) + ","
                + Color.alpha(color) + ")";
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingDoesNotBlockSafePages() throws Throwable {
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(SAFE_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(SAFE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingDoesNotBlockUnwantedSoftwarePages() throws Throwable {
        // TODO(ntfschr): this is a temporary check until we add support for Unwanted Software
        // warnings (crbug/729272)
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(UNWANTED_SOFTWARE_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(UNWANTED_SOFTWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingBlocksPhishingPages() throws Throwable {
        loadGreenPage();
        loadPathAndWaitForInterstitial(PHISHING_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(PHISHING_PAGE_BACKGROUND_COLOR);
        // Assume that we are rendering the interstitial, since we see neither the previous page nor
        // the target page
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingWhitelistedUnsafePagesDontShowInterstitial() throws Throwable {
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            String host = Uri.parse(responseUrl).getHost();
            ArrayList<String> s = new ArrayList<String>();
            s.add(host);
            AwContentsStatics.setSafeBrowsingWhitelist(s, null);
        });
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testCallbackCalledOnSafeBrowsingBadWhitelistRule() throws Throwable {
        verifyWhiteListRule("http://www.google.com", false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testCallbackCalledOnSafeBrowsingGoodWhitelistRule() throws Throwable {
        verifyWhiteListRule("www.google.com", true);
    }

    private void verifyWhiteListRule(final String rule, boolean expected) throws Throwable {
        final WhitelistHelper helper = new WhitelistHelper();
        final int count = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ArrayList<String> s = new ArrayList<String>();
            s.add(rule);
            AwContentsStatics.setSafeBrowsingWhitelist(s, helper);
        });
        helper.waitForCallback(count);
        Assert.assertEquals(expected, helper.success);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingShowsInterstitialForMainFrame() throws Throwable {
        loadGreenPage();
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(MALWARE_PAGE_BACKGROUND_COLOR);
        // Assume that we are rendering the interstitial, since we see neither the previous page nor
        // the target page
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingShowsInterstitialForSubresource() throws Throwable {
        loadGreenPage();
        loadPathAndWaitForInterstitial(IFRAME_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(IFRAME_EMBEDDER_BACKGROUND_COLOR);

        // Assume that we are rendering the interstitial, since we see neither the previous page nor
        // the target page
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingProceedThroughInterstitialForMainFrame() throws Throwable {
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        waitForInterstitialToLoad();
        clickVisitUnsafePage();
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingProceedThroughInterstitialForSubresource() throws Throwable {
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        loadPathAndWaitForInterstitial(IFRAME_HTML_PATH);
        waitForInterstitialToLoad();
        clickVisitUnsafePage();
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        assertTargetPageHasLoaded(IFRAME_EMBEDDER_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingDontProceedCausesNetworkErrorForMainFrame() throws Throwable {
        loadGreenPage();
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        OnReceivedError2Helper errorHelper = mContentsClient.getOnReceivedError2Helper();
        int errorCount = errorHelper.getCallCount();
        waitForInterstitialToLoad();
        clickBackToSafety();
        errorHelper.waitForCallback(errorCount);
        Assert.assertEquals(
                ErrorCodeConversionHelper.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        Assert.assertEquals("Network error is for the malicious page", responseUrl,
                errorHelper.getRequest().url);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    @DisabledTest(message = "crbug/737820")
    public void testSafeBrowsingDontProceedCausesNetworkErrorForSubresource() throws Throwable {
        loadPathAndWaitForInterstitial(IFRAME_HTML_PATH);
        OnReceivedError2Helper errorHelper = mContentsClient.getOnReceivedError2Helper();
        int errorCount = errorHelper.getCallCount();
        waitForInterstitialToLoad();
        clickBackToSafety();
        errorHelper.waitForCallback(errorCount);
        Assert.assertEquals(
                ErrorCodeConversionHelper.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
        final String subresourceUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        Assert.assertEquals(subresourceUrl, errorHelper.getRequest().url);
        Assert.assertFalse(errorHelper.getRequest().isMainFrame);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingDontProceedNavigatesBackForMainFrame() throws Throwable {
        loadGreenPage();
        final String originalTitle = mActivityTestRule.getTitleOnUiThread(mAwContents);
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        waitForInterstitialToChangeTitle();
        waitForInterstitialToLoad();
        clickBackToSafety();

        // Make sure we navigate back to previous page
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return originalTitle.equals(mAwContents.getTitle());
            }
        });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingDontProceedNavigatesBackForSubResource() throws Throwable {
        loadGreenPage();
        final String originalTitle = mActivityTestRule.getTitleOnUiThread(mAwContents);
        loadPathAndWaitForInterstitial(IFRAME_HTML_PATH);
        waitForInterstitialToChangeTitle();
        waitForInterstitialToLoad();
        clickBackToSafety();

        // Make sure we navigate back to previous page
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return originalTitle.equals(mAwContents.getTitle());
            }
        });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingCanBeDisabledPerWebview() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(false);

        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingCanBeEnabledPerWebview() throws Throwable {
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);

        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(true);

        loadGreenPage();
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingShowsNetworkErrorForInvisibleViews() throws Throwable {
        mAwContents.setCanShowInterstitial(false);
        mAwContents.setCanShowBigInterstitial(false);
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        OnReceivedError2Helper errorHelper = mContentsClient.getOnReceivedError2Helper();
        int errorCount = errorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, responseUrl);
        errorHelper.waitForCallback(errorCount);
        Assert.assertEquals(
                ErrorCodeConversionHelper.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
        Assert.assertEquals("Network error is for the malicious page", responseUrl,
                errorHelper.getRequest().url);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingShowsQuietInterstitialForOddSizedViews() throws Throwable {
        mAwContents.setCanShowBigInterstitial(false);
        loadGreenPage();
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingCanShowQuietPhishingInterstitial() throws Throwable {
        mAwContents.setCanShowBigInterstitial(false);
        loadGreenPage();
        loadPathAndWaitForInterstitial(PHISHING_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(PHISHING_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingProceedQuietInterstitial() throws Throwable {
        mAwContents.setCanShowBigInterstitial(false);
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        loadPathAndWaitForInterstitial(PHISHING_HTML_PATH);
        waitForInterstitialToLoad();
        clickVisitUnsafePageQuietInterstitial();
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        assertTargetPageHasLoaded(PHISHING_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingOnSafeBrowsingHitShowInterstitial() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.SHOW_INTERSTITIAL);

        loadGreenPage();
        loadPathAndWaitForInterstitial(PHISHING_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(PHISHING_PAGE_BACKGROUND_COLOR);
        // Assume that we are rendering the interstitial, since we see neither the previous page nor
        // the target page

        // Check onSafeBrowsingHit arguments
        final String responseUrl = mTestServer.getURL(PHISHING_HTML_PATH);
        Assert.assertEquals(responseUrl, mContentsClient.getLastRequest().url);
        Assert.assertEquals(AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_PHISHING,
                mContentsClient.getLastThreatType());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingOnSafeBrowsingHitProceed() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.PROCEED);

        loadGreenPage();
        final String responseUrl = mTestServer.getURL(PHISHING_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
        assertTargetPageHasLoaded(PHISHING_PAGE_BACKGROUND_COLOR);

        // Check onSafeBrowsingHit arguments
        Assert.assertEquals(responseUrl, mContentsClient.getLastRequest().url);
        Assert.assertEquals(AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_PHISHING,
                mContentsClient.getLastThreatType());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingOnSafeBrowsingHitBackToSafety() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.BACK_TO_SAFETY);

        loadGreenPage();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        mActivityTestRule.loadUrlAsync(mAwContents, responseUrl);
        OnReceivedError2Helper errorHelper = mContentsClient.getOnReceivedError2Helper();
        int errorCount = errorHelper.getCallCount();
        errorHelper.waitForCallback(errorCount);
        Assert.assertEquals(
                ErrorCodeConversionHelper.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
        Assert.assertEquals("Network error is for the malicious page", responseUrl,
                errorHelper.getRequest().url);

        assertGreenPageShowing();

        // Check onSafeBrowsingHit arguments
        Assert.assertEquals(responseUrl, mContentsClient.getLastRequest().url);
        Assert.assertEquals(AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_MALWARE,
                mContentsClient.getLastThreatType());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingOnSafeBrowsingHitForSubresourceNoPreviousPage() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.BACK_TO_SAFETY);
        final String responseUrl = mTestServer.getURL(IFRAME_HTML_PATH);
        final String subresourceUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, responseUrl);

        // We'll successfully load IFRAME_HTML_PATH, and will soon call onSafeBrowsingHit
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);

        // Wait for the onSafeBrowsingHit to call BACK_TO_SAFETY and navigate back
        mActivityTestRule.pollUiThread(
                () -> ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL.equals(mAwContents.getUrl()));

        // Check onSafeBrowsingHit arguments
        Assert.assertFalse(mContentsClient.getLastRequest().isMainFrame);
        Assert.assertEquals(subresourceUrl, mContentsClient.getLastRequest().url);
        Assert.assertEquals(AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_MALWARE,
                mContentsClient.getLastThreatType());
    }

    @Test
    @SmallTest
    @RetryOnFailure // crbug/765932
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingOnSafeBrowsingHitForSubresource() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.BACK_TO_SAFETY);
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(IFRAME_HTML_PATH);
        final String subresourceUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, responseUrl);

        // We'll successfully load IFRAME_HTML_PATH, and will soon call onSafeBrowsingHit
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);

        // Wait for the onSafeBrowsingHit to call BACK_TO_SAFETY and navigate back
        // clang-format off
        mActivityTestRule.pollUiThread(() -> colorToString(GREEN_PAGE_BACKGROUND_COLOR).equals(
                colorToString(GraphicsTestUtils.getPixelColorAtCenterOfView(mAwContents,
                        mContainerView))));
        // clang-format on

        // Check onSafeBrowsingHit arguments
        Assert.assertFalse(mContentsClient.getLastRequest().isMainFrame);
        Assert.assertEquals(subresourceUrl, mContentsClient.getLastRequest().url);
        Assert.assertEquals(AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_MALWARE,
                mContentsClient.getLastThreatType());

        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.PROCEED);

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
        assertTargetPageHasLoaded(IFRAME_EMBEDDER_BACKGROUND_COLOR);

        Assert.assertFalse(mContentsClient.getLastRequest().isMainFrame);
        Assert.assertEquals(subresourceUrl, mContentsClient.getLastRequest().url);
        Assert.assertEquals(AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_MALWARE,
                mContentsClient.getLastThreatType());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingUserOptOutOverridesManifest() throws Throwable {
        AwSafeBrowsingConfigHelper.setSafeBrowsingUserOptIn(false);
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingUserOptOutOverridesPerWebView() throws Throwable {
        AwSafeBrowsingConfigHelper.setSafeBrowsingUserOptIn(false);
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(true);
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingHardcodedMalwareUrl() throws Throwable {
        loadGreenPage();
        int interstitialCount =
                mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, WEB_UI_MALWARE_URL);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(interstitialCount);
        waitForInterstitialToLoad();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingHardcodedPhishingUrl() throws Throwable {
        loadGreenPage();
        int interstitialCount =
                mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, WEB_UI_PHISHING_URL);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(interstitialCount);
        waitForInterstitialToLoad();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingClickLearnMoreLink() throws Throwable {
        loadInterstitialAndClickLink(PHISHING_HTML_PATH, "learn-more-link",
                appendLocale("https://support.google.com/chrome/?p=cpn_safe_browsing_wv"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingClickReportErrorLink() throws Throwable {
        // Only phishing interstitials have the report-error-link
        loadInterstitialAndClickLink(PHISHING_HTML_PATH, "report-error-link",
                appendLocale("https://www.google.com/safebrowsing/report_error/"));
    }

    private String appendLocale(String url) throws Exception {
        return Uri.parse(url)
                .buildUpon()
                .appendQueryParameter("hl", LocaleUtils.getDefaultLocaleString())
                .toString();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingClickDiagnosticLink() throws Throwable {
        // Only malware interstitials have the diagnostic-link
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        final String diagnosticUrl =
                Uri.parse("https://www.google.com/safebrowsing/diagnostic")
                        .buildUpon()
                        .appendQueryParameter("site", responseUrl)
                        .appendQueryParameter("client", "chromium")
                        .appendQueryParameter("hl", LocaleUtils.getDefaultLocaleString())
                        .toString();
        loadInterstitialAndClickLink(MALWARE_HTML_PATH, "diagnostic-link", diagnosticUrl);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingClickWhitePaperLink() throws Throwable {
        final String whitepaperUrl =
                Uri.parse("https://www.google.com/chrome/browser/privacy/whitepaper.html")
                        .buildUpon()
                        .appendQueryParameter("hl", LocaleUtils.getDefaultLocaleString())
                        .fragment("extendedreport")
                        .toString();
        loadInterstitialAndClickLink(PHISHING_HTML_PATH, "whitepaper-link", whitepaperUrl);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingClickPrivacyPolicy() throws Throwable {
        final String privacyPolicyUrl =
                Uri.parse("https://www.google.com/chrome/browser/privacy/")
                        .buildUpon()
                        .appendQueryParameter("hl", LocaleUtils.getDefaultLocaleString())
                        .fragment("safe-browsing-policies")
                        .toString();
        loadInterstitialAndClickLink(PHISHING_HTML_PATH, "privacy-link", privacyPolicyUrl);
    }

    private void loadInterstitialAndClickLink(String path, String linkId, String linkUrl)
            throws Exception {
        loadPathAndWaitForInterstitial(path);
        waitForInterstitialToLoad();
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        clickLinkById(linkId);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        Assert.assertEquals(linkUrl, mAwContents.getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testInitSafeBrowsingCallbackOnUIThread() throws Throwable {
        Context ctx = InstrumentationRegistry.getInstrumentation()
                              .getTargetContext()
                              .getApplicationContext();
        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();
        mOnUiThread = false;
        AwContentsStatics.initSafeBrowsing(ctx, b -> {
            mOnUiThread = ThreadUtils.runningOnUiThread();
            helper.notifyCalled();
        });
        helper.waitForCallback(count);
        // Don't run the assert on the callback's thread, since the test runner loses the stack
        // trace unless on the instrumentation thread.
        Assert.assertTrue("Callback should run on UI Thread", mOnUiThread);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testInitSafeBrowsingUsesAppContext() throws Throwable {
        MockContext ctx =
                new MockContext(InstrumentationRegistry.getInstrumentation().getTargetContext());
        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();

        AwContentsStatics.initSafeBrowsing(ctx, b -> helper.notifyCalled());
        helper.waitForCallback(count);
        Assert.assertTrue(
                "Should only use application context", ctx.wasGetApplicationContextCalled());
    }

    private static class MockContext extends ContextWrapper {
        private boolean mGetApplicationContextWasCalled;

        public MockContext(Context context) {
            super(context);
        }

        public Context getApplicationContext() {
            mGetApplicationContextWasCalled = true;
            return super.getApplicationContext();
        }

        public boolean wasGetApplicationContextCalled() {
            return mGetApplicationContextWasCalled;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetSafeBrowsingPrivacyPolicyUrl() throws Throwable {
        final Uri privacyPolicyUrl =
                Uri.parse("https://www.google.com/chrome/browser/privacy/")
                        .buildUpon()
                        .appendQueryParameter("hl", LocaleUtils.getDefaultLocaleString())
                        .fragment("safe-browsing-policies")
                        .build();
        Assert.assertEquals(privacyPolicyUrl, AwContentsStatics.getSafeBrowsingPrivacyPolicyUrl());
        Assert.assertNotNull(AwContentsStatics.getSafeBrowsingPrivacyPolicyUrl());
    }
}
