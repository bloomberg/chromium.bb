// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.annotation.SuppressLint;
import android.support.test.filters.SmallTest;
import android.util.Pair;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnReceivedErrorHelper;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the WebViewClient.shouldOverrideUrlLoading() method.
 */
public class AwContentsClientShouldOverrideUrlLoadingTest extends AwTestBase {
    private static final String ABOUT_BLANK_URL = "about:blank";
    private static final String DATA_URL = "data:text/html,<div/>";
    private static final String REDIRECT_TARGET_PATH = "/redirect_target.html";
    private static final String TITLE = "TITLE";
    private static final String TAG = "AwContentsClientShouldOverrideUrlLoadingTest";
    private static final String TEST_EMAIL = "nobody@example.org";
    private static final String TEST_EMAIL_URI = "mailto:" + TEST_EMAIL.replace("@", "%40");
    private static final String TEST_PHONE = "+16503336000";
    private static final String TEST_PHONE_URI = "tel:" + TEST_PHONE.replace("+", "%2B");
    // Use the shortest possible address to ensure it fits into one line.
    // Otherwise, click on the center of the HTML element may get into empty space.
    private static final String TEST_ADDRESS = "1 st. long enough, CA 90000";
    private static final String TEST_ADDRESS_URI = "geo:0,0?q="
            + TEST_ADDRESS.replace(" ", "+").replace(",", "%2C");

    private TestWebServer mWebServer;
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestAwContentsClient.ShouldOverrideUrlLoadingHelper mShouldOverrideUrlLoadingHelper;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mWebServer = TestWebServer.start();
    }

    @Override
    protected void tearDown() throws Exception {
        mWebServer.shutdown();
        super.tearDown();
    }

    private void standardSetup() throws Throwable {
        mContentsClient = new TestAwContentsClient();
        setupWithProvidedContentsClient(mContentsClient);
        mShouldOverrideUrlLoadingHelper = mContentsClient.getShouldOverrideUrlLoadingHelper();
    }

    private void setupWithProvidedContentsClient(AwContentsClient contentsClient) throws Throwable {
        mTestContainerView = createAwTestContainerViewOnMainSync(contentsClient);
        mAwContents = mTestContainerView.getAwContents();
    }

    private void clickOnLinkUsingJs() throws Throwable {
        enableJavaScriptOnUiThread(mAwContents);
        JSUtils.clickOnLinkUsingJs(this, mAwContents,
                mContentsClient.getOnEvaluateJavaScriptResultHelper(), "link");
    }

    // Since this value is read on the UI thread, it's simpler to set it there too.
    void setShouldOverrideUrlLoadingReturnValueOnUiThread(final boolean value) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                mShouldOverrideUrlLoadingHelper.setShouldOverrideUrlLoadingReturnValue(value);
            }
        });
    }

    private String getTestPageCommonHeaders() {
        return "<title>" + TITLE + "</title> ";
    }

    private String makeHtmlPageFrom(String headers, String body) {
        return CommonResources.makeHtmlPageFrom(getTestPageCommonHeaders() + headers, body);
    }

    private String getHtmlForPageWithJsAssignLinkTo(String url) {
        return makeHtmlPageFrom("",
                "<img onclick=\"location.href='" + url + "'\" class=\"big\" id=\"link\" />");
    }

    private String getHtmlForPageWithJsReplaceLinkTo(String url) {
        return makeHtmlPageFrom("",
                "<img onclick=\"location.replace('" + url + "');\" class=\"big\" id=\"link\" />");
    }

    private String getHtmlForPageWithMetaRefreshRedirectTo(String url) {
        return makeHtmlPageFrom("<meta http-equiv=\"refresh\" content=\"0;url=" + url + "\" />",
                "<div>Meta refresh redirect</div>");
    }

    @SuppressLint("DefaultLocale")
    private String getHtmlForPageWithJsRedirectTo(String url, String method, int timeout) {
        return makeHtmlPageFrom(""
                + "<script>"
                +   "function doRedirectAssign() {"
                +     "location.href = '" + url + "';"
                +   "} "
                +   "function doRedirectReplace() {"
                +     "location.replace('" + url + "');"
                +   "} "
                + "</script>",
                String.format("<iframe onLoad=\"setTimeout('doRedirect%s()', %d);\" />",
                    method, timeout));
    }

    private String addPageToTestServer(String httpPath, String html) {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html"));
        headers.add(Pair.create("Cache-Control", "no-store"));
        return mWebServer.setResponse(httpPath, html, headers);
    }

    private String createRedirectTargetPage() {
        return addPageToTestServer(REDIRECT_TARGET_PATH,
                makeHtmlPageFrom("", "<div>This is the end of the redirect chain</div>"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledOnLoadUrl() throws Throwable {
        standardSetup();
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledOnReload() throws Throwable {
        standardSetup();
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        int callCountBeforeReload = mShouldOverrideUrlLoadingHelper.getCallCount();
        reloadSync(mAwContents, mContentsClient.getOnPageFinishedHelper());
        assertEquals(callCountBeforeReload, mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    private void waitForNavigationRunnableAndAssertTitleChanged(
            Runnable navigationRunnable) throws Exception {
        CallbackHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        final int callCount = onPageFinishedHelper.getCallCount();
        final String oldTitle = getTitleOnUiThread(mAwContents);
        getInstrumentation().runOnMainSync(navigationRunnable);
        onPageFinishedHelper.waitForCallback(callCount);
        assertFalse(oldTitle.equals(getTitleOnUiThread(mAwContents)));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledOnBackForwardNavigation() throws Throwable {
        standardSetup();
        final String[] pageTitles = new String[] { "page1", "page2", "page3" };

        for (String title : pageTitles) {
            loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                    CommonResources.makeHtmlPageFrom("<title>" + title + "</title>", ""),
                    "text/html", false);
        }
        assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());

        waitForNavigationRunnableAndAssertTitleChanged(new Runnable() {
                    @Override
                    public void run() {
                        mAwContents.goBack();
                    }
                });
        assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());

        waitForNavigationRunnableAndAssertTitleChanged(new Runnable() {
                    @Override
                    public void run() {
                        mAwContents.goForward();
                    }
                });
        assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());

        waitForNavigationRunnableAndAssertTitleChanged(new Runnable() {
                    @Override
                    public void run() {
                        mAwContents.goBackOrForward(-2);
                    }
                });
        assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());

        waitForNavigationRunnableAndAssertTitleChanged(new Runnable() {
                    @Override
                    public void run() {
                        mAwContents.goBackOrForward(1);
                    }
                });
        assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCantBlockLoads() throws Throwable {
        standardSetup();
        setShouldOverrideUrlLoadingReturnValueOnUiThread(true);

        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(getTestPageCommonHeaders(),
                    DATA_URL), "text/html", false);

        assertEquals(TITLE, getTitleOnUiThread(mAwContents));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledBeforeOnPageStarted() throws Throwable {
        standardSetup();
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();

        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();
        final int onPageStartedCallCount = onPageStartedHelper.getCallCount();
        setShouldOverrideUrlLoadingReturnValueOnUiThread(true);
        clickOnLinkUsingJs();

        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);
        assertEquals(onPageStartedCallCount, onPageStartedHelper.getCallCount());
    }


    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testDoesNotCauseOnReceivedError() throws Throwable {
        standardSetup();
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        final int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();

        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();
        setShouldOverrideUrlLoadingReturnValueOnUiThread(true);
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);
        setShouldOverrideUrlLoadingReturnValueOnUiThread(false);

        // After we load this URL we're certain that any in-flight callbacks for the previous
        // navigation have been delivered.
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), ABOUT_BLANK_URL);

        assertEquals(onReceivedErrorCallCount, onReceivedErrorHelper.getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledForAnchorNavigations() throws Throwable {
        doTestNotCalledForAnchorNavigations(false);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledForAnchorNavigationsWithNonHierarchicalScheme() throws Throwable {
        doTestNotCalledForAnchorNavigations(true);
    }

    private void doTestNotCalledForAnchorNavigations(boolean useLoadData) throws Throwable {
        standardSetup();

        final String anchorLinkPath = "/anchor_link.html";
        final String anchorLinkUrl = mWebServer.getResponseUrl(anchorLinkPath);
        addPageToTestServer(anchorLinkPath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(anchorLinkUrl + "#anchor"));

        if (useLoadData) {
            loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                    CommonResources.makeHtmlPageWithSimpleLinkTo("#anchor"), "text/html", false);
        } else {
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), anchorLinkUrl);
        }

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs();

        // After we load this URL we're certain that any in-flight callbacks for the previous
        // navigation have been delivered.
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), ABOUT_BLANK_URL);

        assertEquals(shouldOverrideUrlLoadingCallCount,
                mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenLinkClicked() throws Throwable {
        standardSetup();

        // We can't go to about:blank from here because we'd get a cross-origin error.
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs();

        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        assertEquals(DATA_URL, mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertTrue(mShouldOverrideUrlLoadingHelper.isMainFrame());
    }

    /*
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    */
    @DisabledTest(message = "crbug.com/462306")
    public void testCalledWhenTopLevelAboutBlankNavigation() throws Throwable {
        standardSetup();

        final String httpPath = "/page_with_about_blank_navigation";
        final String httpPathOnServer = mWebServer.getResponseUrl(httpPath);
        addPageToTestServer(httpPath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(ABOUT_BLANK_URL));

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                httpPathOnServer);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs();

        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        assertEquals(ABOUT_BLANK_URL,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenSelfLinkClicked() throws Throwable {
        standardSetup();

        final String httpPath = "/page_with_link_to_self.html";
        final String httpPathOnServer = mWebServer.getResponseUrl(httpPath);
        addPageToTestServer(httpPath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(httpPathOnServer));

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                httpPathOnServer);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs();

        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        assertEquals(httpPathOnServer,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertTrue(mShouldOverrideUrlLoadingHelper.isMainFrame());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenNavigatingFromJavaScriptUsingAssign()
            throws Throwable {
        standardSetup();
        enableJavaScriptOnUiThread(mAwContents);

        final String redirectTargetUrl = createRedirectTargetPage();
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                getHtmlForPageWithJsAssignLinkTo(redirectTargetUrl), "text/html", false);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenNavigatingFromJavaScriptUsingReplace()
            throws Throwable {
        standardSetup();
        enableJavaScriptOnUiThread(mAwContents);

        final String redirectTargetUrl = createRedirectTargetPage();
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                getHtmlForPageWithJsReplaceLinkTo(redirectTargetUrl), "text/html", false);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        // It's not a server-side redirect.
        assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertTrue(mShouldOverrideUrlLoadingHelper.isMainFrame());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testPassesCorrectUrl() throws Throwable {
        standardSetup();

        final String redirectTargetUrl = createRedirectTargetPage();
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(redirectTargetUrl), "text/html",
                false);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        assertEquals(redirectTargetUrl,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        // It's not a server-side redirect.
        assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertTrue(mShouldOverrideUrlLoadingHelper.isMainFrame());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCanIgnoreLoading() throws Throwable {
        standardSetup();

        final String redirectTargetUrl = createRedirectTargetPage();
        final String pageWithLinkToIgnorePath = "/page_with_link_to_ignore.html";
        final String pageWithLinkToIgnoreUrl = addPageToTestServer(pageWithLinkToIgnorePath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(redirectTargetUrl));
        final String synchronizationPath = "/sync.html";
        final String synchronizationUrl = addPageToTestServer(synchronizationPath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(redirectTargetUrl));

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                pageWithLinkToIgnoreUrl);

        setShouldOverrideUrlLoadingReturnValueOnUiThread(true);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();
        // Some time around here true should be returned from the shouldOverrideUrlLoading
        // callback causing the navigation caused by calling clickOnLinkUsingJs to be ignored.
        // We validate this by checking which pages were loaded on the server.
        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);

        setShouldOverrideUrlLoadingReturnValueOnUiThread(false);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), synchronizationUrl);

        assertEquals(1, mWebServer.getRequestCount(pageWithLinkToIgnorePath));
        assertEquals(1, mWebServer.getRequestCount(synchronizationPath));
        assertEquals(0, mWebServer.getRequestCount(REDIRECT_TARGET_PATH));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledForDataUrl() throws Throwable {
        standardSetup();
        final String dataUrl =
                "data:text/html;base64,"
                        + "PGh0bWw+PGhlYWQ+PHRpdGxlPmRhdGFVcmxUZXN0QmFzZTY0PC90aXRsZT48"
                        + "L2hlYWQ+PC9odG1sPg==";
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(dataUrl), "text/html", false);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();

        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        assertTrue("Expected URL that starts with 'data:' but got: <"
                + mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl() + "> instead.",
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl().startsWith(
                        "data:"));
        assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertTrue(mShouldOverrideUrlLoadingHelper.isMainFrame());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledForUnsupportedSchemes() throws Throwable {
        standardSetup();
        final String unsupportedSchemeUrl = "foobar://resource/1";
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(unsupportedSchemeUrl), "text/html",
                false);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();

        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        assertEquals(unsupportedSchemeUrl,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertTrue(mShouldOverrideUrlLoadingHelper.isMainFrame());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledForPostNavigations() throws Throwable {
        // The reason POST requests are excluded is BUG 155250.
        standardSetup();

        final String redirectTargetUrl = createRedirectTargetPage();
        final String postLinkUrl = addPageToTestServer("/page_with_post_link.html",
                CommonResources.makeHtmlPageWithSimplePostFormTo(redirectTargetUrl));

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), postLinkUrl);

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();

        assertEquals(0, mWebServer.getRequestCount(REDIRECT_TARGET_PATH));
        clickOnLinkUsingJs();

        // Wait for the target URL to be fetched from the server.
        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mWebServer.getRequestCount(REDIRECT_TARGET_PATH) == 1;
            }
        });

        // Since the targetURL was loaded from the test server it means all processing related
        // to dispatching a shouldOverrideUrlLoading callback had finished and checking the call
        // is stable.
        assertEquals(shouldOverrideUrlLoadingCallCount,
                mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledFor302AfterPostNavigations() throws Throwable {
        // The reason POST requests are excluded is BUG 155250.
        standardSetup();

        final String redirectTargetUrl = createRedirectTargetPage();
        final String postToGetRedirectUrl = mWebServer.setRedirect("/302.html", redirectTargetUrl);
        final String postLinkUrl = addPageToTestServer("/page_with_post_link.html",
                CommonResources.makeHtmlPageWithSimplePostFormTo(postToGetRedirectUrl));

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), postLinkUrl);

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);

        // Wait for the target URL to be fetched from the server.
        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mWebServer.getRequestCount(REDIRECT_TARGET_PATH) == 1;
            }
        });

        assertEquals(redirectTargetUrl,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        assertTrue(mShouldOverrideUrlLoadingHelper.isRedirect());
        assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertTrue(mShouldOverrideUrlLoadingHelper.isMainFrame());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledForIframeHttpNavigations() throws Throwable {
        standardSetup();

        final String iframeRedirectTargetUrl = createRedirectTargetPage();
        final String iframeRedirectUrl =
                mWebServer.setRedirect("/302.html", iframeRedirectTargetUrl);
        final String pageWithIframeUrl =
                addPageToTestServer("/iframe_intercept.html",
                        makeHtmlPageFrom("", "<iframe src=\"" + iframeRedirectUrl + "\" />"));

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();

        assertEquals(0, mWebServer.getRequestCount(REDIRECT_TARGET_PATH));
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithIframeUrl);

        // Wait for the redirect target URL to be fetched from the server.
        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mWebServer.getRequestCount(REDIRECT_TARGET_PATH) == 1;
            }
        });

        assertEquals(shouldOverrideUrlLoadingCallCount,
                mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledForIframeUnsupportedSchemeNavigations() throws Throwable {
        standardSetup();

        final String unsupportedSchemeUrl = "foobar://resource/1";
        final String pageWithIframeUrl =
                addPageToTestServer("/iframe_intercept.html",
                        makeHtmlPageFrom("", "<iframe src=\"" + unsupportedSchemeUrl + "\" />"));

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithIframeUrl);

        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);
        assertEquals(unsupportedSchemeUrl,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertFalse(mShouldOverrideUrlLoadingHelper.isMainFrame());
    }

    /**
     * Worker method for the various redirect tests.
     *
     * Calling this will first load the redirect URL built from redirectFilePath, query and
     * locationFilePath and assert that we get a override callback for the destination.
     * The second part of the test loads a page that contains a link which points at the redirect
     * URL. We expect two callbacks - one for the redirect link and another for the destination.
     */
    private void doTestCalledOnRedirect(String redirectUrl, String redirectTarget,
            boolean serverSideRedirect) throws Throwable {
        standardSetup();
        final String pageTitle = "doTestCalledOnRedirect page";
        final String pageWithLinkToRedirectUrl = addPageToTestServer(
                "/page_with_link_to_redirect.html", CommonResources.makeHtmlPageWithSimpleLinkTo(
                        "<title>" + pageTitle + "</title>", redirectUrl));
        enableJavaScriptOnUiThread(mAwContents);

        // There is a slight difference between navigations caused by calling load and navigations
        // caused by clicking on a link:
        //
        //  * when using load the navigation is treated as if it came from the URL bar (has the
        //    navigation type TYPED, doesn't have the has_user_gesture flag); thus the navigation
        //    itself is not reported via shouldOverrideUrlLoading, but then if it has caused a
        //    redirect, the redirect itself is reported;
        //
        //  * when clicking on a link the navigation has the LINK type and has_user_gesture depends
        //    on whether it was a real click done by the user, or has it been done by JS; on click,
        //    both the initial navigation and the redirect are reported via
        //    shouldOverrideUrlLoading.
        int directLoadCallCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), redirectUrl);

        mShouldOverrideUrlLoadingHelper.waitForCallback(directLoadCallCount, 1);
        assertEquals(redirectTarget,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        assertEquals(serverSideRedirect, mShouldOverrideUrlLoadingHelper.isRedirect());
        assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertTrue(mShouldOverrideUrlLoadingHelper.isMainFrame());

        // Test clicking with JS, hasUserGesture must be false.
        int indirectLoadCallCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                pageWithLinkToRedirectUrl);
        assertEquals(indirectLoadCallCount, mShouldOverrideUrlLoadingHelper.getCallCount());

        clickOnLinkUsingJs();

        mShouldOverrideUrlLoadingHelper.waitForCallback(indirectLoadCallCount, 1);
        assertEquals(redirectUrl,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertTrue(mShouldOverrideUrlLoadingHelper.isMainFrame());
        mShouldOverrideUrlLoadingHelper.waitForCallback(indirectLoadCallCount + 1, 1);
        assertEquals(redirectTarget,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        assertEquals(serverSideRedirect, mShouldOverrideUrlLoadingHelper.isRedirect());
        assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertTrue(mShouldOverrideUrlLoadingHelper.isMainFrame());

        // Make sure the redirect target page has finished loading.
        pollUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return !mAwContents.getTitle().equals(pageTitle);
            }
        });
        indirectLoadCallCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        loadUrlAsync(mAwContents, pageWithLinkToRedirectUrl);
        pollUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mAwContents.getTitle().equals(pageTitle);
            }
        });
        assertEquals(indirectLoadCallCount, mShouldOverrideUrlLoadingHelper.getCallCount());

        // Simulate touch, hasUserGesture must be true only on the first call.
        DOMUtils.clickNode(mAwContents.getContentViewCore(), "link");

        mShouldOverrideUrlLoadingHelper.waitForCallback(indirectLoadCallCount, 1);
        assertEquals(redirectUrl,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        assertTrue(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertTrue(mShouldOverrideUrlLoadingHelper.isMainFrame());
        mShouldOverrideUrlLoadingHelper.waitForCallback(indirectLoadCallCount + 1, 1);
        assertEquals(redirectTarget,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        assertEquals(serverSideRedirect, mShouldOverrideUrlLoadingHelper.isRedirect());
        assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertTrue(mShouldOverrideUrlLoadingHelper.isMainFrame());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOn302Redirect() throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage();
        final String redirectUrl = mWebServer.setRedirect("/302.html", redirectTargetUrl);
        doTestCalledOnRedirect(redirectUrl, redirectTargetUrl, true);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnMetaRefreshRedirect() throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage();
        final String redirectUrl = addPageToTestServer("/meta_refresh.html",
                getHtmlForPageWithMetaRefreshRedirectTo(redirectTargetUrl));
        doTestCalledOnRedirect(redirectUrl, redirectTargetUrl, false);
    }


    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnJavaScriptLocationImmediateAssignRedirect()
            throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage();
        final String redirectUrl = addPageToTestServer("/js_immediate_assign.html",
                getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Assign", 0));
        doTestCalledOnRedirect(redirectUrl, redirectTargetUrl, false);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnJavaScriptLocationImmediateReplaceRedirect()
            throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage();
        final String redirectUrl = addPageToTestServer("/js_immediate_replace.html",
                getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Replace", 0));
        doTestCalledOnRedirect(redirectUrl, redirectTargetUrl, false);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    @RetryOnFailure
    public void testCalledOnJavaScriptLocationDelayedAssignRedirect()
            throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage();
        final String redirectUrl = addPageToTestServer("/js_delayed_assign.html",
                getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Assign", 100));
        doTestCalledOnRedirect(redirectUrl, redirectTargetUrl, false);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    @RetryOnFailure
    public void testCalledOnJavaScriptLocationDelayedReplaceRedirect()
            throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage();
        final String redirectUrl = addPageToTestServer("/js_delayed_replace.html",
                getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Replace", 100));
        doTestCalledOnRedirect(redirectUrl, redirectTargetUrl, false);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testDoubleNavigateDoesNotSuppressInitialNavigate() throws Throwable {
        final String jsUrl = "javascript:try{console.log('processed js loadUrl');}catch(e){};";
        standardSetup();

        // Do a double navigagtion, the second being an effective no-op, in quick succession (i.e.
        // without yielding the main thread inbetween).
        int currentCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.loadUrl(LoadUrlParams.createLoadDataParams(
                        CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL), "text/html",
                        false));
                mAwContents.loadUrl(new LoadUrlParams(jsUrl));
            }
        });
        mContentsClient.getOnPageFinishedHelper().waitForCallback(currentCallCount, 1,
                WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    @SuppressFBWarnings("DLS_DEAD_LOCAL_STORE")
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCallDestroyInCallback() throws Throwable {
        class DestroyInCallbackClient extends TestAwContentsClient {
            @Override
            public boolean shouldOverrideUrlLoading(AwContentsClient.AwWebResourceRequest request) {
                mAwContents.destroy();
                return super.shouldOverrideUrlLoading(request);
            }
        }

        mContentsClient = new DestroyInCallbackClient();
        setupWithProvidedContentsClient(mContentsClient);
        mShouldOverrideUrlLoadingHelper = mContentsClient.getShouldOverrideUrlLoadingHelper();

        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();

        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        int shouldOverrideUrlLoadingCallCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        setShouldOverrideUrlLoadingReturnValueOnUiThread(true);
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);

        pollUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return AwContents.getNativeInstanceCount() == 0;
            }
        });
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullContentsClientWithServerRedirect() throws Throwable {
        try {
            // The test will fire real intents through the test activity.
            // Need to temporarily suppress startActivity otherwise there will be a
            // handler selection window and the test can't dismiss that.
            getActivity().setIgnoreStartActivity(true);
            final String testUrl = mWebServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));
            setupWithProvidedContentsClient(new NullContentsClient() {
                @Override
                public boolean hasWebViewClient() {
                    return false;
                }
            });
            loadUrlAsync(mAwContents, testUrl);
            pollUiThread(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return mAwContents.getTitle().equals(CommonResources.ABOUT_TITLE);
                }
            });

            assertNull(getActivity().getLastSentIntent());

            // Now the server will redirect path1 to path2. Path2 will load ABOUT_HTML.
            // AwContents should create an intent for the server initiated redirection.
            final String path1 = "/from.html";
            final String path2 = "/to.html";
            final String fromUrl = mWebServer.setRedirect(path1, path2);
            final String toUrl = mWebServer.setResponse(
                    path2, CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));
            loadUrlAsync(mAwContents, fromUrl);

            pollUiThread(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return getActivity().getLastSentIntent() != null;
                }
            });
            assertEquals(toUrl, getActivity().getLastSentIntent().getData().toString());
        } finally {
            getActivity().setIgnoreStartActivity(false);
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullContentsClientOpenLink() throws Throwable {
        try {
            // The test will fire real intents through the test activity.
            // Need to temporarily suppress startActivity otherwise there will be a
            // handler selection window and the test can't dismiss that.
            getActivity().setIgnoreStartActivity(true);
            final String testUrl = mWebServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));
            setupWithProvidedContentsClient(new NullContentsClient() {
                @Override
                public boolean hasWebViewClient() {
                    return false;
                }
            });
            enableJavaScriptOnUiThread(mAwContents);
            final String pageTitle = "Click Title";
            final String htmlWithLink = "<html><title>" + pageTitle + "</title>"
                    + "<body><a id='link' href='" + testUrl + "'>Click this!</a></body></html>";
            final String urlWithLink = mWebServer.setResponse(
                    "/html_with_link.html", htmlWithLink, CommonResources.getTextHtmlHeaders(true));

            loadUrlAsync(mAwContents, urlWithLink);
            pollUiThread(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return mAwContents.getTitle().equals(pageTitle);
                }
            });
            // Executing JS code that tries to navigate somewhere should not create an intent.
            assertEquals("\"" + testUrl + "\"", JSUtils.executeJavaScriptAndWaitForResult(
                            this, mAwContents, new OnEvaluateJavaScriptResultHelper(),
                            "document.location.href='" + testUrl + "'"));
            assertNull(getActivity().getLastSentIntent());

            // Clicking on a link should create an intent.
            DOMUtils.clickNode(mAwContents.getContentViewCore(), "link");
            pollUiThread(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return getActivity().getLastSentIntent() != null;
                }
            });
            assertEquals(testUrl, getActivity().getLastSentIntent().getData().toString());
        } finally {
            getActivity().setIgnoreStartActivity(false);
        }
    }

    private String setupForContentClickTest(final String content, boolean inMainFrame)
            throws Exception {
        final String contentId = "content";
        final String findContentJs = inMainFrame
                ? "document.getElementById(\"" + contentId + "\")"
                : "window.frames[0].document.getElementById(\"" + contentId + "\")";
        final String pageHtml = inMainFrame
                ? "<html><body onload='document.title=" + findContentJs + ".innerText'>"
                + "<span id='" + contentId + "'>" + content + "</span></body></html>"
                : "<html>"
                + "<body style='margin:0;' onload='document.title=" + findContentJs + ".innerText'>"
                + " <iframe style='border:none;width:100%;' srcdoc=\""
                + "   <body style='margin:0;'><span id='" + contentId + "'>"
                + content + "</span></body>"
                + "\" src='iframe.html'></iframe>"
                + "</body></html>";
        final String testUrl = mWebServer.setResponse("/content_test.html", pageHtml, null);

        enableJavaScriptOnUiThread(mAwContents);
        loadUrlAsync(mAwContents, testUrl);
        pollUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mAwContents.getTitle().equals(content);
            }
        });
        return findContentJs;
    }

    private void doTestNullContentsClientClickableContent(String pageContent,
            String intentContent) throws Throwable {
        try {
            // The test will fire real intents through the test activity.
            // Need to temporarily suppress startActivity otherwise there will be a
            // handler selection window and the test can't dismiss that.
            getActivity().setIgnoreStartActivity(true);
            setupWithProvidedContentsClient(new NullContentsClient() {
                @Override
                public boolean hasWebViewClient() {
                    return false;
                }
            });

            final String findContentJs = setupForContentClickTest(pageContent, true);
            // Clicking on the content should create an intent.
            DOMUtils.clickNodeByJs(mAwContents.getContentViewCore(), findContentJs);
            pollUiThread(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return getActivity().getLastSentIntent() != null;
                }
            });
            assertEquals(intentContent,
                    getActivity().getLastSentIntent().getData().toString());
        } finally {
            getActivity().setIgnoreStartActivity(false);
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.ENABLE_CONTENT_INTENT_DETECTION})
    public void testNullContentsClientClickableEmail() throws Throwable {
        doTestNullContentsClientClickableContent(TEST_EMAIL, TEST_EMAIL_URI);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.NETWORK_COUNTRY_ISO + "=us",
            ContentSwitches.ENABLE_CONTENT_INTENT_DETECTION})
    public void testNullContentsClientClickablePhone() throws Throwable {
        doTestNullContentsClientClickableContent(TEST_PHONE, TEST_PHONE_URI);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.ENABLE_CONTENT_INTENT_DETECTION})
    public void testNullContentsClientClickableAddress() throws Throwable {
        doTestNullContentsClientClickableContent(TEST_ADDRESS, TEST_ADDRESS_URI);
    }

    private void doTestClickableContent(String pageContent, String intentContent,
            boolean inMainFrame) throws Throwable {
        standardSetup();

        final String findContentJs = setupForContentClickTest(pageContent, inMainFrame);
        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        DOMUtils.clickNodeByJs(mAwContents.getContentViewCore(), findContentJs);
        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        assertEquals(intentContent,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        assertTrue(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        assertEquals(inMainFrame, mShouldOverrideUrlLoadingHelper.isMainFrame());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.ENABLE_CONTENT_INTENT_DETECTION})
    public void testClickableEmail() throws Throwable {
        doTestClickableContent(TEST_EMAIL, TEST_EMAIL_URI, true);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.NETWORK_COUNTRY_ISO + "=us",
            ContentSwitches.ENABLE_CONTENT_INTENT_DETECTION})
    public void testClickablePhone() throws Throwable {
        doTestClickableContent(TEST_PHONE, TEST_PHONE_URI, true);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.ENABLE_CONTENT_INTENT_DETECTION})
    public void testClickableAddress() throws Throwable {
        doTestClickableContent(TEST_ADDRESS, TEST_ADDRESS_URI, true);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.ENABLE_CONTENT_INTENT_DETECTION})
    public void testClickableEmailInIframe() throws Throwable {
        doTestClickableContent(TEST_EMAIL, TEST_EMAIL_URI, false);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.NETWORK_COUNTRY_ISO + "=us",
            ContentSwitches.ENABLE_CONTENT_INTENT_DETECTION})
    public void testClickablePhoneInIframe() throws Throwable {
        doTestClickableContent(TEST_PHONE, TEST_PHONE_URI, false);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.ENABLE_CONTENT_INTENT_DETECTION})
    public void testClickableAddressInIframe() throws Throwable {
        doTestClickableContent(TEST_ADDRESS, TEST_ADDRESS_URI, false);
    }
}
