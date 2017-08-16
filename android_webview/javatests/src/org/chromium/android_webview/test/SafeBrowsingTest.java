// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertNotEquals;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;
import android.webkit.ValueCallback;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContents.DependencyFactory;
import org.chromium.android_webview.AwContents.InternalAccessDelegate;
import org.chromium.android_webview.AwContents.NativeDrawGLFunctorFactory;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
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
import org.chromium.base.LocaleUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.components.safe_browsing.SafeBrowsingApiHandler;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.Callable;

/**
 * Test suite for SafeBrowsing.
 *
 * Ensures that interstitials can be successfully created for malicous pages.
 */
public class SafeBrowsingTest extends AwTestBase {
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
            assertEquals(Arrays.binarySearch(threatsOfInterest, UNWANTED_SOFTWARE_CODE), -1);

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
                ValueCallback<AwSafeBrowsingResponse> callback) {
            mLastRequest = request;
            mLastThreatType = threatType;
            callback.onReceiveValue(new AwSafeBrowsingResponse(mAction, mReporting));
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

    private static class JavaScriptHelper extends CallbackHelper {
        private String mValue;

        public void setValue(String s) {
            mValue = s;
        }

        public String getValue() {
            return mValue;
        }
    }

    private static class WhitelistHelper extends CallbackHelper implements ValueCallback<Boolean> {
        public boolean success;

        public void onReceiveValue(Boolean success) {
            this.success = success;
            notifyCalled();
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new SafeBrowsingContentsClient();
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
    public AwBrowserContext createAwBrowserContextOnUiThread(
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

    private void evaluateJavaScriptOnInterstitialOnUiThread(
            final String script, final ValueCallback<String> callback) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAwContents.evaluateJavaScriptOnInterstitialForTesting(script, callback);
            }
        });
    }

    private void waitForInterstitialToLoad() throws Exception {
        final String script = "document.readyState;";
        final JavaScriptHelper helper = new JavaScriptHelper();

        final ValueCallback<String> callback = new ValueCallback<String>() {
            @Override
            public void onReceiveValue(String value) {
                helper.setValue(value);
                helper.notifyCalled();
            }
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
        loadUrlAsync(mAwContents, responseUrl);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(interstitialCount);
    }

    private void assertTargetPageHasLoaded(int pageColor) throws Exception {
        waitForVisualStateCallback(mAwContents);
        assertEquals("Target page should be visible", pageColor,
                GraphicsTestUtils.getPixelColorAtCenterOfView(mAwContents, mContainerView));
    }

    private void assertGreenPageNotShowing() throws Exception {
        assertNotEquals("Original page should not be showing", GREEN_PAGE_BACKGROUND_COLOR,
                GraphicsTestUtils.getPixelColorAtCenterOfView(mAwContents, mContainerView));
    }

    private void assertTargetPageNotShowing(int pageColor) throws Exception {
        assertNotEquals("Target page should not be showing", pageColor,
                GraphicsTestUtils.getPixelColorAtCenterOfView(mAwContents, mContainerView));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingDoesNotBlockSafePages() throws Throwable {
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(SAFE_HTML_PATH);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(SAFE_PAGE_BACKGROUND_COLOR);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingDoesNotBlockUnwantedSoftwarePages() throws Throwable {
        // TODO(ntfschr): this is a temporary check until we add support for Unwanted Software
        // warnings (crbug/729272)
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(UNWANTED_SOFTWARE_HTML_PATH);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(UNWANTED_SOFTWARE_PAGE_BACKGROUND_COLOR);
    }

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

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingWhitelistedUnsafePagesDontShowInterstitial() throws Throwable {
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                String host = Uri.parse(responseUrl).getHost();
                ArrayList<String> s = new ArrayList<String>();
                s.add(host);
                AwContentsStatics.setSafeBrowsingWhitelist(s, null);
            }
        });
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testCallbackCalledOnSafeBrowsingBadWhitelistRule() throws Throwable {
        verifyWhiteListRule("http://www.google.com", false);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testCallbackCalledOnSafeBrowsingGoodWhitelistRule() throws Throwable {
        verifyWhiteListRule("www.google.com", true);
    }

    private void verifyWhiteListRule(final String rule, boolean expected) throws Throwable {
        final WhitelistHelper helper = new WhitelistHelper();
        final int count = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ArrayList<String> s = new ArrayList<String>();
                s.add(rule);
                AwContentsStatics.setSafeBrowsingWhitelist(s, helper);
            }
        });
        helper.waitForCallback(count);
        assertEquals(expected, helper.success);
    }

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
        assertEquals(
                ErrorCodeConversionHelper.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        assertEquals("Network error is for the malicious page", responseUrl,
                errorHelper.getRequest().url);
    }

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

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingDontProceedNavigatesBackForSubResource() throws Throwable {
        loadGreenPage();
        final String originalTitle = getTitleOnUiThread(mAwContents);
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

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingCanBeDisabledPerWebview() throws Throwable {
        getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(false);

        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingCanBeEnabledPerWebview() throws Throwable {
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);

        getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(true);

        loadGreenPage();
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(MALWARE_PAGE_BACKGROUND_COLOR);
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
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(MALWARE_PAGE_BACKGROUND_COLOR);
    }

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
        assertEquals(responseUrl, mContentsClient.getLastRequest().url);
        assertEquals(AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_PHISHING,
                mContentsClient.getLastThreatType());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingOnSafeBrowsingHitProceed() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.PROCEED);

        loadGreenPage();
        final String responseUrl = mTestServer.getURL(PHISHING_HTML_PATH);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        waitForVisualStateCallback(mAwContents);
        assertTargetPageHasLoaded(PHISHING_PAGE_BACKGROUND_COLOR);

        // Check onSafeBrowsingHit arguments
        assertEquals(responseUrl, mContentsClient.getLastRequest().url);
        assertEquals(AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_PHISHING,
                mContentsClient.getLastThreatType());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingOnSafeBrowsingHitBackToSafety() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.BACK_TO_SAFETY);

        loadGreenPage();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        loadUrlAsync(mAwContents, responseUrl);
        OnReceivedError2Helper errorHelper = mContentsClient.getOnReceivedError2Helper();
        int errorCount = errorHelper.getCallCount();
        errorHelper.waitForCallback(errorCount);
        assertEquals(
                ErrorCodeConversionHelper.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
        assertEquals("Network error is for the malicious page", responseUrl,
                errorHelper.getRequest().url);

        assertEquals("Original page should be showing", GREEN_PAGE_BACKGROUND_COLOR,
                GraphicsTestUtils.getPixelColorAtCenterOfView(mAwContents, mContainerView));

        // Check onSafeBrowsingHit arguments
        assertEquals(responseUrl, mContentsClient.getLastRequest().url);
        assertEquals(AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_MALWARE,
                mContentsClient.getLastThreatType());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingOnSafeBrowsingHitForSubresourceNoPreviousPage() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.BACK_TO_SAFETY);
        final String responseUrl = mTestServer.getURL(IFRAME_HTML_PATH);
        final String subresourceUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        loadUrlAsync(mAwContents, responseUrl);

        // We'll successfully load IFRAME_HTML_PATH, and will soon call onSafeBrowsingHit
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);

        // Wait for the onSafeBrowsingHit to call BACK_TO_SAFETY and navigate back
        pollUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL.equals(mAwContents.getUrl());
            }
        });

        // Check onSafeBrowsingHit arguments
        assertFalse(mContentsClient.getLastRequest().isMainFrame);
        assertEquals(subresourceUrl, mContentsClient.getLastRequest().url);
        assertEquals(AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_MALWARE,
                mContentsClient.getLastThreatType());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingOnSafeBrowsingHitForSubresource() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.BACK_TO_SAFETY);
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(IFRAME_HTML_PATH);
        final String subresourceUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        loadUrlAsync(mAwContents, responseUrl);

        // We'll successfully load IFRAME_HTML_PATH, and will soon call onSafeBrowsingHit
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);

        // Wait for the onSafeBrowsingHit to call BACK_TO_SAFETY and navigate back
        pollUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return GREEN_PAGE_BACKGROUND_COLOR
                        == GraphicsTestUtils.getPixelColorAtCenterOfView(
                                   mAwContents, mContainerView);
            }
        });

        // Check onSafeBrowsingHit arguments
        assertFalse(mContentsClient.getLastRequest().isMainFrame);
        assertEquals(subresourceUrl, mContentsClient.getLastRequest().url);
        assertEquals(AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_MALWARE,
                mContentsClient.getLastThreatType());

        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.PROCEED);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        waitForVisualStateCallback(mAwContents);
        assertTargetPageHasLoaded(IFRAME_EMBEDDER_BACKGROUND_COLOR);

        assertFalse(mContentsClient.getLastRequest().isMainFrame);
        assertEquals(subresourceUrl, mContentsClient.getLastRequest().url);
        assertEquals(AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_MALWARE,
                mContentsClient.getLastThreatType());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingUserOptOutOverridesManifest() throws Throwable {
        AwSafeBrowsingConfigHelper.setSafeBrowsingUserOptIn(false);
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingUserOptOutOverridesPerWebView() throws Throwable {
        AwSafeBrowsingConfigHelper.setSafeBrowsingUserOptIn(false);
        getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(true);
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingHardcodedMalwareUrl() throws Throwable {
        loadGreenPage();
        int interstitialCount =
                mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        loadUrlAsync(mAwContents, WEB_UI_MALWARE_URL);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(interstitialCount);
        waitForInterstitialToLoad();
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingHardcodedPhishingUrl() throws Throwable {
        loadGreenPage();
        int interstitialCount =
                mWebContentsObserver.getAttachedInterstitialPageHelper().getCallCount();
        loadUrlAsync(mAwContents, WEB_UI_PHISHING_URL);
        mWebContentsObserver.getAttachedInterstitialPageHelper().waitForCallback(interstitialCount);
        waitForInterstitialToLoad();
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingClickLearnMoreLink() throws Throwable {
        loadInterstitialAndClickLink(PHISHING_HTML_PATH, "learn-more-link",
                appendLocale("https://support.google.com/chrome/?p=cpn_safe_browsing_wv"));
    }

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
        assertEquals(linkUrl, mAwContents.getUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
    public void testInitSafeBrowsingCallbackOnUIThread() throws Throwable {
        Context ctx = getInstrumentation().getTargetContext().getApplicationContext();
        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();
        mOnUiThread = false;
        AwContentsStatics.initSafeBrowsing(ctx, new ValueCallback<Boolean>() {
            @Override
            public void onReceiveValue(Boolean b) {
                mOnUiThread = ThreadUtils.runningOnUiThread();
                helper.notifyCalled();
            }
        });
        helper.waitForCallback(count);
        // Don't run the assert on the callback's thread, since the test runner loses the stack
        // trace unless on the instrumentation thread.
        assertTrue("Callback should run on UI Thread", mOnUiThread);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetSafeBrowsingPrivacyPolicyUrl() throws Throwable {
        final Uri privacyPolicyUrl = Uri.parse("https://www.google.com/chrome/browser/privacy/")
                                             .buildUpon()
                                             .fragment("safe-browsing-policies")
                                             .build();
        assertEquals(privacyPolicyUrl, AwContentsStatics.getSafeBrowsingPrivacyPolicyUrl());
        assertNotNull(AwContentsStatics.getSafeBrowsingPrivacyPolicyUrl());
    }
}
