// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnReceivedErrorHelper;
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

    private TestWebServer mWebServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mWebServer = new TestWebServer(false);
    }

    @Override
    protected void tearDown() throws Exception {
        mWebServer.shutdown();
        super.tearDown();
    }

    private void clickOnLinkUsingJs(final AwContents awContents,
            final TestAwContentsClient contentsClient) throws Throwable {
        enableJavaScriptOnUiThread(awContents);
        JSUtils.clickOnLinkUsingJs(this, awContents,
                contentsClient.getOnEvaluateJavaScriptResultHelper(), "link");
    }

    // Since this value is read on the UI thread, it's simpler to set it there too.
    void setShouldOverrideUrlLoadingReturnValueOnUiThread(
            final TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideHelper,
            final boolean value) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                shouldOverrideHelper.setShouldOverrideUrlLoadingReturnValue(value);
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

    private String getHtmlForPageWithJsRedirectTo(String url, String method, int timeout) {
        return makeHtmlPageFrom(
                "<script>" +
                  "function doRedirectAssign() {" +
                    "location.href = '" + url + "';" +
                  "} " +
                  "function doRedirectReplace() {" +
                    "location.replace('" + url + "');" +
                  "} " +
                "</script>",
                String.format("<iframe onLoad=\"setTimeout('doRedirect%s()', %d);\" />",
                    method, timeout));
    }

    private String addPageToTestServer(TestWebServer webServer, String httpPath, String html) {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html"));
        headers.add(Pair.create("Cache-Control", "no-store"));
        return webServer.setResponse(httpPath, html, headers);
    }

    private String createRedirectTargetPage(TestWebServer webServer) {
        return addPageToTestServer(webServer, REDIRECT_TARGET_PATH,
                makeHtmlPageFrom("", "<div>This is the end of the redirect chain</div>"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledOnLoadUrl() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
            contentsClient.getShouldOverrideUrlLoadingHelper();

        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        assertEquals(0, shouldOverrideUrlLoadingHelper.getCallCount());
    }

    private void waitForNavigationRunnableAndAssertTitleChanged(AwContents awContents,
            CallbackHelper onPageFinishedHelper,
            Runnable navigationRunnable) throws Exception {
        final int callCount = onPageFinishedHelper.getCallCount();
        final String oldTitle = getTitleOnUiThread(awContents);
        getInstrumentation().runOnMainSync(navigationRunnable);
        onPageFinishedHelper.waitForCallback(callCount);
        assertFalse(oldTitle.equals(getTitleOnUiThread(awContents)));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledOnBackForwardNavigation() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
            contentsClient.getShouldOverrideUrlLoadingHelper();
        final String[] pageTitles = new String[] { "page1", "page2", "page3" };

        for (String title : pageTitles) {
            loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                    CommonResources.makeHtmlPageFrom("<title>" + title + "</title>", ""),
                    "text/html", false);
        }
        assertEquals(0, shouldOverrideUrlLoadingHelper.getCallCount());

        waitForNavigationRunnableAndAssertTitleChanged(awContents,
                contentsClient.getOnPageFinishedHelper(), new Runnable() {
            @Override
            public void run() {
                awContents.goBack();
            }
        });
        assertEquals(0, shouldOverrideUrlLoadingHelper.getCallCount());

        waitForNavigationRunnableAndAssertTitleChanged(awContents,
                contentsClient.getOnPageFinishedHelper(), new Runnable() {
            @Override
            public void run() {
                awContents.goForward();
            }
        });
        assertEquals(0, shouldOverrideUrlLoadingHelper.getCallCount());

        waitForNavigationRunnableAndAssertTitleChanged(awContents,
                contentsClient.getOnPageFinishedHelper(), new Runnable() {
            @Override
            public void run() {
                awContents.goBackOrForward(-2);
            }
        });
        assertEquals(0, shouldOverrideUrlLoadingHelper.getCallCount());

        waitForNavigationRunnableAndAssertTitleChanged(awContents,
                contentsClient.getOnPageFinishedHelper(), new Runnable() {
            @Override
            public void run() {
                awContents.goBackOrForward(1);
            }
        });
        assertEquals(0, shouldOverrideUrlLoadingHelper.getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCantBlockLoads() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
            contentsClient.getShouldOverrideUrlLoadingHelper();

        setShouldOverrideUrlLoadingReturnValueOnUiThread(shouldOverrideUrlLoadingHelper, true);

        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(getTestPageCommonHeaders(),
                    DATA_URL), "text/html", false);

        assertEquals(TITLE, getTitleOnUiThread(awContents));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledBeforeOnPageStarted() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
            contentsClient.getShouldOverrideUrlLoadingHelper();
        OnPageStartedHelper onPageStartedHelper = contentsClient.getOnPageStartedHelper();

        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        final int shouldOverrideUrlLoadingCallCount = shouldOverrideUrlLoadingHelper.getCallCount();
        final int onPageStartedCallCount = onPageStartedHelper.getCallCount();
        setShouldOverrideUrlLoadingReturnValueOnUiThread(shouldOverrideUrlLoadingHelper, true);
        clickOnLinkUsingJs(awContents, contentsClient);

        shouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);
        assertEquals(onPageStartedCallCount, onPageStartedHelper.getCallCount());
    }


    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testDoesNotCauseOnReceivedError() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
            contentsClient.getShouldOverrideUrlLoadingHelper();
        OnReceivedErrorHelper onReceivedErrorHelper = contentsClient.getOnReceivedErrorHelper();
        final int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();

        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        final int shouldOverrideUrlLoadingCallCount = shouldOverrideUrlLoadingHelper.getCallCount();

        setShouldOverrideUrlLoadingReturnValueOnUiThread(shouldOverrideUrlLoadingHelper, true);

        clickOnLinkUsingJs(awContents, contentsClient);

        shouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);

        setShouldOverrideUrlLoadingReturnValueOnUiThread(shouldOverrideUrlLoadingHelper, false);

        // After we load this URL we're certain that any in-flight callbacks for the previous
        // navigation have been delivered.
        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), ABOUT_BLANK_URL);

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
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
            contentsClient.getShouldOverrideUrlLoadingHelper();

        final String anchorLinkPath = "/anchor_link.html";
        final String anchorLinkUrl = mWebServer.getResponseUrl(anchorLinkPath);
        addPageToTestServer(mWebServer, anchorLinkPath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(anchorLinkUrl + "#anchor"));

        if (useLoadData) {
            loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                    CommonResources.makeHtmlPageWithSimpleLinkTo("#anchor"), "text/html", false);
        } else {
            loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), anchorLinkUrl);
        }

        final int shouldOverrideUrlLoadingCallCount =
            shouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs(awContents, contentsClient);

        // After we load this URL we're certain that any in-flight callbacks for the previous
        // navigation have been delivered.
        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), ABOUT_BLANK_URL);

        assertEquals(shouldOverrideUrlLoadingCallCount,
                shouldOverrideUrlLoadingHelper.getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenLinkClicked() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                contentsClient.getShouldOverrideUrlLoadingHelper();

        // We can't go to about:blank from here because we'd get a cross-origin error.
        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        int callCount = shouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs(awContents, contentsClient);

        shouldOverrideUrlLoadingHelper.waitForCallback(callCount);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenTopLevelAboutBlankNavigation() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                contentsClient.getShouldOverrideUrlLoadingHelper();

        final String httpPath = "/page_with_about_blank_navigation";
        final String httpPathOnServer = mWebServer.getResponseUrl(httpPath);
        addPageToTestServer(mWebServer, httpPath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(ABOUT_BLANK_URL));

        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                httpPathOnServer);

        int callCount = shouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs(awContents, contentsClient);

        shouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        assertEquals(ABOUT_BLANK_URL,
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenSelfLinkClicked() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                contentsClient.getShouldOverrideUrlLoadingHelper();

        final String httpPath = "/page_with_link_to_self.html";
        final String httpPathOnServer = mWebServer.getResponseUrl(httpPath);
        addPageToTestServer(mWebServer, httpPath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(httpPathOnServer));

        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                httpPathOnServer);

        int callCount = shouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs(awContents, contentsClient);

        shouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        assertEquals(httpPathOnServer,
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenNavigatingFromJavaScriptUsingAssign()
            throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        enableJavaScriptOnUiThread(awContents);
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                contentsClient.getShouldOverrideUrlLoadingHelper();

        final String redirectTargetUrl = createRedirectTargetPage(mWebServer);
        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                getHtmlForPageWithJsAssignLinkTo(redirectTargetUrl), "text/html", false);

        int callCount = shouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs(awContents, contentsClient);

        shouldOverrideUrlLoadingHelper.waitForCallback(callCount);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenNavigatingFromJavaScriptUsingReplace()
            throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        enableJavaScriptOnUiThread(awContents);
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                contentsClient.getShouldOverrideUrlLoadingHelper();

        final String redirectTargetUrl = createRedirectTargetPage(mWebServer);
        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                getHtmlForPageWithJsReplaceLinkTo(redirectTargetUrl), "text/html", false);

        int callCount = shouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs(awContents, contentsClient);
        shouldOverrideUrlLoadingHelper.waitForCallback(callCount);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testPassesCorrectUrl() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                contentsClient.getShouldOverrideUrlLoadingHelper();

        final String redirectTargetUrl = createRedirectTargetPage(mWebServer);
        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(redirectTargetUrl), "text/html",
                false);

        int callCount = shouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs(awContents, contentsClient);
        shouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        assertEquals(redirectTargetUrl,
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCanIgnoreLoading() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                contentsClient.getShouldOverrideUrlLoadingHelper();

        final String redirectTargetUrl = createRedirectTargetPage(mWebServer);
        final String pageWithLinkToIgnorePath = "/page_with_link_to_ignore.html";
        final String pageWithLinkToIgnoreUrl = addPageToTestServer(mWebServer,
                pageWithLinkToIgnorePath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(redirectTargetUrl));
        final String synchronizationPath = "/sync.html";
        final String synchronizationUrl = addPageToTestServer(mWebServer,
                synchronizationPath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(redirectTargetUrl));

        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                pageWithLinkToIgnoreUrl);

        setShouldOverrideUrlLoadingReturnValueOnUiThread(shouldOverrideUrlLoadingHelper, true);

        int callCount = shouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs(awContents, contentsClient);
        // Some time around here true should be returned from the shouldOverrideUrlLoading
        // callback causing the navigation caused by calling clickOnLinkUsingJs to be ignored.
        // We validate this by checking which pages were loaded on the server.
        shouldOverrideUrlLoadingHelper.waitForCallback(callCount);

        setShouldOverrideUrlLoadingReturnValueOnUiThread(shouldOverrideUrlLoadingHelper, false);

        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), synchronizationUrl);

        assertEquals(1, mWebServer.getRequestCount(pageWithLinkToIgnorePath));
        assertEquals(1, mWebServer.getRequestCount(synchronizationPath));
        assertEquals(0, mWebServer.getRequestCount(REDIRECT_TARGET_PATH));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledForDataUrl() throws Throwable {
        final String dataUrl =
                "data:text/html;base64," +
                "PGh0bWw+PGhlYWQ+PHRpdGxlPmRhdGFVcmxUZXN0QmFzZTY0PC90aXRsZT48" +
                "L2hlYWQ+PC9odG1sPg==";
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                contentsClient.getShouldOverrideUrlLoadingHelper();
        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(dataUrl), "text/html", false);

        int callCount = shouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs(awContents, contentsClient);

        shouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        assertTrue("Expected URL that starts with 'data:' but got: <" +
                   shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl() + "> instead.",
                   shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl().startsWith(
                           "data:"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledForUnsupportedSchemes() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                contentsClient.getShouldOverrideUrlLoadingHelper();
        final String unsupportedSchemeUrl = "foobar://resource/1";
        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(unsupportedSchemeUrl), "text/html",
                false);

        int callCount = shouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs(awContents, contentsClient);

        shouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        assertEquals(unsupportedSchemeUrl,
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledForPostNavigations() throws Throwable {
        // The reason POST requests are excluded is BUG 155250.
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
            contentsClient.getShouldOverrideUrlLoadingHelper();

        final String redirectTargetUrl = createRedirectTargetPage(mWebServer);
        final String postLinkUrl = addPageToTestServer(mWebServer, "/page_with_post_link.html",
                CommonResources.makeHtmlPageWithSimplePostFormTo(redirectTargetUrl));

        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), postLinkUrl);

        final int shouldOverrideUrlLoadingCallCount =
            shouldOverrideUrlLoadingHelper.getCallCount();

        assertEquals(0, mWebServer.getRequestCount(REDIRECT_TARGET_PATH));
        clickOnLinkUsingJs(awContents, contentsClient);

        // Wait for the target URL to be fetched from the server.
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mWebServer.getRequestCount(REDIRECT_TARGET_PATH) == 1;
            }
        });

        // Since the targetURL was loaded from the test server it means all processing related
        // to dispatching a shouldOverrideUrlLoading callback had finished and checking the call
        // is stable.
        assertEquals(shouldOverrideUrlLoadingCallCount,
                shouldOverrideUrlLoadingHelper.getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledFor302AfterPostNavigations() throws Throwable {
        // The reason POST requests are excluded is BUG 155250.
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
            contentsClient.getShouldOverrideUrlLoadingHelper();

        final String redirectTargetUrl = createRedirectTargetPage(mWebServer);
        final String postToGetRedirectUrl = mWebServer.setRedirect("/302.html", redirectTargetUrl);
        final String postLinkUrl = addPageToTestServer(mWebServer, "/page_with_post_link.html",
                CommonResources.makeHtmlPageWithSimplePostFormTo(postToGetRedirectUrl));

        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), postLinkUrl);

        final int shouldOverrideUrlLoadingCallCount =
            shouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs(awContents, contentsClient);

        shouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);

        // Wait for the target URL to be fetched from the server.
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mWebServer.getRequestCount(REDIRECT_TARGET_PATH) == 1;
            }
        });

        assertEquals(redirectTargetUrl,
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledForIframeHttpNavigations() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
            contentsClient.getShouldOverrideUrlLoadingHelper();

        final String iframeRedirectTargetUrl = createRedirectTargetPage(mWebServer);
        final String iframeRedirectUrl =
            mWebServer.setRedirect("/302.html", iframeRedirectTargetUrl);
        final String pageWithIframeUrl =
            addPageToTestServer(mWebServer, "/iframe_intercept.html",
                makeHtmlPageFrom("", "<iframe src=\"" + iframeRedirectUrl + "\" />"));

        final int shouldOverrideUrlLoadingCallCount =
            shouldOverrideUrlLoadingHelper.getCallCount();

        assertEquals(0, mWebServer.getRequestCount(REDIRECT_TARGET_PATH));
        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), pageWithIframeUrl);

        // Wait for the redirect target URL to be fetched from the server.
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mWebServer.getRequestCount(REDIRECT_TARGET_PATH) == 1;
            }
        });

        assertEquals(shouldOverrideUrlLoadingCallCount,
                shouldOverrideUrlLoadingHelper.getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledForIframeUnsupportedSchemeNavigations() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
            contentsClient.getShouldOverrideUrlLoadingHelper();

        final String unsupportedSchemeUrl = "foobar://resource/1";
        final String pageWithIframeUrl =
            addPageToTestServer(mWebServer, "/iframe_intercept.html",
                makeHtmlPageFrom("", "<iframe src=\"" + unsupportedSchemeUrl + "\" />"));

        final int shouldOverrideUrlLoadingCallCount =
            shouldOverrideUrlLoadingHelper.getCallCount();

        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), pageWithIframeUrl);

        shouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);
        assertEquals(unsupportedSchemeUrl,
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
    }

    /**
     * Worker method for the various redirect tests.
     *
     * Calling this will first load the redirect URL built from redirectFilePath, query and
     * locationFilePath and assert that we get a override callback for the destination.
     * The second part of the test loads a page that contains a link which points at the redirect
     * URL. We expect two callbacks - one for the redirect link and another for the destination.
     */
    private void doTestCalledOnRedirect(TestWebServer webServer,
            String redirectUrl, String redirectTarget) throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final String pageWithLinkToRedirectUrl = addPageToTestServer(webServer,
                "/page_with_link_to_redirect.html",
                CommonResources.makeHtmlPageWithSimpleLinkTo(redirectUrl));
        enableJavaScriptOnUiThread(awContents);

        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                contentsClient.getShouldOverrideUrlLoadingHelper();
        int directLoadCallCount = shouldOverrideUrlLoadingHelper.getCallCount();
        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), redirectUrl);

        shouldOverrideUrlLoadingHelper.waitForCallback(directLoadCallCount, 1);
        assertEquals(redirectTarget,
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());

        // There is a slight difference between navigations caused by calling load and navigations
        // caused by clicking on a link:
        //  * when using load the navigation is treated as if it came from the URL bar (has the
        //    navigation type TYPED and doesn't have the has_user_gesture flag)
        //  * when clicking on a link the navigation has the LINK type and has_user_gesture is
        //    true.
        // Both of these should yield the same result which is what we're verifying here.
        int indirectLoadCallCount = shouldOverrideUrlLoadingHelper.getCallCount();
        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                pageWithLinkToRedirectUrl);

        assertEquals(indirectLoadCallCount, shouldOverrideUrlLoadingHelper.getCallCount());

        clickOnLinkUsingJs(awContents, contentsClient);

        shouldOverrideUrlLoadingHelper.waitForCallback(indirectLoadCallCount, 2);
        assertEquals(redirectTarget,
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        assertEquals(redirectUrl,
                shouldOverrideUrlLoadingHelper.getPreviousShouldOverrideUrlLoadingUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOn302Redirect() throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage(mWebServer);
        final String redirectUrl = mWebServer.setRedirect("/302.html", redirectTargetUrl);

        doTestCalledOnRedirect(mWebServer, redirectUrl, redirectTargetUrl);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnMetaRefreshRedirect() throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage(mWebServer);
        final String redirectUrl = addPageToTestServer(mWebServer, "/meta_refresh.html",
                getHtmlForPageWithMetaRefreshRedirectTo(redirectTargetUrl));
        doTestCalledOnRedirect(mWebServer, redirectUrl, redirectTargetUrl);
    }


    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnJavaScriptLocationImmediateAssignRedirect()
            throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage(mWebServer);
        final String redirectUrl = addPageToTestServer(mWebServer, "/js_immediate_assign.html",
                getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Assign", 0));
        doTestCalledOnRedirect(mWebServer, redirectUrl, redirectTargetUrl);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnJavaScriptLocationImmediateReplaceRedirect()
            throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage(mWebServer);
        final String redirectUrl = addPageToTestServer(mWebServer, "/js_immediate_replace.html",
                getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Replace", 0));
        doTestCalledOnRedirect(mWebServer, redirectUrl, redirectTargetUrl);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnJavaScriptLocationDelayedAssignRedirect()
            throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage(mWebServer);
        final String redirectUrl = addPageToTestServer(mWebServer, "/js_delayed_assign.html",
                getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Assign", 100));
        doTestCalledOnRedirect(mWebServer, redirectUrl, redirectTargetUrl);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnJavaScriptLocationDelayedReplaceRedirect()
            throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage(mWebServer);
        final String redirectUrl = addPageToTestServer(mWebServer, "/js_delayed_replace.html",
                getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Replace", 100));
        doTestCalledOnRedirect(mWebServer, redirectUrl, redirectTargetUrl);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testDoubleNavigateDoesNotSuppressInitialNavigate() throws Throwable {
        final String jsUrl = "javascript:try{console.log('processed js loadUrl');}catch(e){};";
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
            contentsClient.getShouldOverrideUrlLoadingHelper();

        // Do a double navigagtion, the second being an effective no-op, in quick succession (i.e.
        // without yielding the main thread inbetween).
        int currentCallCount = contentsClient.getOnPageFinishedHelper().getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                awContents.loadUrl(LoadUrlParams.createLoadDataParams(
                        CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL), "text/html",
                        false));
                awContents.loadUrl(new LoadUrlParams(jsUrl));
            }
        });
        contentsClient.getOnPageFinishedHelper().waitForCallback(currentCallCount, 1,
                WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        assertEquals(0, shouldOverrideUrlLoadingHelper.getCallCount());
    }
}
