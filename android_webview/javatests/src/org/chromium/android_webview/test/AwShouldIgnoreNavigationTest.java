// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Bundle;
import android.os.SystemClock;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;
import android.view.MotionEvent;
import android.util.Log;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.TestWebServer;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnReceivedErrorHelper;

import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests for the WebViewClient.shouldOverrideUrlLoading() method.
 */
public class AwShouldIgnoreNavigationTest extends AndroidWebViewTestBase {
    private final static String ABOUT_BLANK_URL = "about:blank";
    private final static String DATA_URL = "data:text/html,<div/>";
    private final static String REDIRECT_TARGET_PATH = "/redirect_target.html";

    private static final long TEST_TIMEOUT = 20000L;
    private static final int CHECK_INTERVAL = 100;

    private static class TestAwContentsClient
            extends org.chromium.android_webview.test.TestAwContentsClient {

        public class ShouldIgnoreNavigationHelper extends CallbackHelper {
            private String mShouldIgnoreNavigationUrl;
            private String mPreviousShouldIgnoreNavigationUrl;
            private boolean mShouldIgnoreNavigationReturnValue = false;
            void setShouldIgnoreNavigationUrl(String url) {
                mShouldIgnoreNavigationUrl = url;
            }
            void setPreviousShouldIgnoreNavigationUrl(String url) {
                mPreviousShouldIgnoreNavigationUrl = url;
            }
            void setShouldIgnoreNavigationReturnValue(boolean value) {
                mShouldIgnoreNavigationReturnValue = value;
            }
            public String getShouldIgnoreNavigationUrl() {
                assert getCallCount() > 0;
                return mShouldIgnoreNavigationUrl;
            }
            public String getPreviousShouldIgnoreNavigationUrl() {
                assert getCallCount() > 1;
                return mPreviousShouldIgnoreNavigationUrl;
            }
            public boolean getShouldIgnoreNavigationReturnValue() {
                return mShouldIgnoreNavigationReturnValue;
            }
            public void notifyCalled(String url) {
                mPreviousShouldIgnoreNavigationUrl = mShouldIgnoreNavigationUrl;
                mShouldIgnoreNavigationUrl = url;
                notifyCalled();
            }
        }

        @Override
        public boolean shouldIgnoreNavigation(String url) {
            super.shouldIgnoreNavigation(url);
            boolean returnValue =
                mShouldIgnoreNavigationHelper.getShouldIgnoreNavigationReturnValue();
            mShouldIgnoreNavigationHelper.notifyCalled(url);
            return returnValue;
        }

        private ShouldIgnoreNavigationHelper mShouldIgnoreNavigationHelper;

        public TestAwContentsClient() {
            super();
            mShouldIgnoreNavigationHelper = new ShouldIgnoreNavigationHelper();
        }

        public ShouldIgnoreNavigationHelper getShouldIgnoreNavigationHelper() {
            return mShouldIgnoreNavigationHelper;
        }
    }

    private void clickOnLinkUsingJs(final AwContents awContents,
            final TestAwContentsClient contentsClient) throws Throwable {
        enableJavaScriptOnUiThread(awContents);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    String linkIsNotNull = executeJavaScriptAndWaitForResult(awContents,
                        contentsClient, "document.getElementById('link') != null");
                    return linkIsNotNull.equals("true");
                } catch (Throwable t) {
                    t.printStackTrace();
                    fail("Failed to check if DOM is loaded: " + t.toString());
                    return false;
                }
            }
        }, WAIT_TIMEOUT_SECONDS * 1000, CHECK_INTERVAL));

        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                awContents.getContentViewCore().evaluateJavaScript(
                    "var evObj = document.createEvent('Events'); " +
                    "evObj.initEvent('click', true, false); " +
                    "document.getElementById('link').dispatchEvent(evObj);" +
                    "console.log('element with id link clicked');");
            }
        });
    }

    // Since this value is read on the UI thread, it's simpler to set it there too.
    void setShouldIgnoreNavigationReturnValueOnUiThread(
            final TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper,
            final boolean value) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                shouldIgnoreNavigationHelper.setShouldIgnoreNavigationReturnValue(value);
            }
        });
    }

    private String makeHtmlPageFrom(String headers, String body) {
        return "<html>" +
                    "<head>" +
                        "<title>Title</title>" +
                        "<style type=\"text/css\">" +
                            // Make the image take up all of the page so that we don't have to do
                            // any fancy hit target calculations when synthesizing the touch event
                            // to click it.
                            "img.big { width:100%; height:100%; background-color:blue; }" +
                        "</style>" +
                        headers +
                    "</head>" +
                    "<body>" +
                        body +
                    "</body>" +
                "</html>";
    }

    private String getHtmlForPageWithSimpleLinkTo(String destination) {
        return makeHtmlPageFrom("",
                        "<a href=\"" + destination + "\" id=\"link\">" +
                           "<img class=\"big\" />" +
                        "</a>");
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
                  "} "+
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
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationNotCalledOnLoadUrl() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
            contentsClient.getShouldIgnoreNavigationHelper();

        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                getHtmlForPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        assertEquals(0, shouldIgnoreNavigationHelper.getCallCount());
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCantBlockLoads() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
            contentsClient.getShouldIgnoreNavigationHelper();

        setShouldIgnoreNavigationReturnValueOnUiThread(shouldIgnoreNavigationHelper, true);

        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                getHtmlForPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        assertEquals("Title", getTitleOnUiThread(awContents));
    }

    /**
     * @SmallTest
     * @Feature({"Android-WebView", "Navigation"})
     * BUG=154292
     */
    @DisabledTest
    public void testShouldIgnoreNavigationCalledBeforeOnPageStarted() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
            contentsClient.getShouldIgnoreNavigationHelper();
        OnPageStartedHelper onPageStartedHelper = contentsClient.getOnPageStartedHelper();

        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                getHtmlForPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        final int shouldIgnoreNavigationCallCount = shouldIgnoreNavigationHelper.getCallCount();
        final int onPageStartedCallCount = onPageStartedHelper.getCallCount();
        setShouldIgnoreNavigationReturnValueOnUiThread(shouldIgnoreNavigationHelper, true);
        clickOnLinkUsingJs(awContents, contentsClient);

        shouldIgnoreNavigationHelper.waitForCallback(shouldIgnoreNavigationCallCount);
        assertEquals(onPageStartedCallCount, onPageStartedHelper.getCallCount());
    }


    /**
     * @SmallTest
     * @Feature({"Android-WebView", "Navigation"})
     * BUG=154292
     */
    @DisabledTest
    public void testShouldIgnoreNavigationDoesNotCauseOnReceivedError() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
            contentsClient.getShouldIgnoreNavigationHelper();
        OnReceivedErrorHelper onReceivedErrorHelper = contentsClient.getOnReceivedErrorHelper();
        final int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();

        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                getHtmlForPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        final int shouldIgnoreNavigationCallCount = shouldIgnoreNavigationHelper.getCallCount();

        setShouldIgnoreNavigationReturnValueOnUiThread(shouldIgnoreNavigationHelper, true);

        clickOnLinkUsingJs(awContents, contentsClient);

        shouldIgnoreNavigationHelper.waitForCallback(shouldIgnoreNavigationCallCount);

        setShouldIgnoreNavigationReturnValueOnUiThread(shouldIgnoreNavigationHelper, false);

        // After we load this URL we're certain that any in-flight callbacks for the previous
        // navigation have been delivered.
        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), ABOUT_BLANK_URL);

        assertEquals(onReceivedErrorCallCount, onReceivedErrorHelper.getCallCount());
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationNotCalledForAnchorNavigations() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
            contentsClient.getShouldIgnoreNavigationHelper();

        TestWebServer webServer = null;
        try {
            // Set up the HTML page.
            webServer = new TestWebServer(false);
            final String anchorLinkPath = "/anchor_link.html";
            final String anchorLinkUrl = webServer.getResponseUrl(anchorLinkPath);
            addPageToTestServer(webServer, anchorLinkPath,
                    getHtmlForPageWithSimpleLinkTo(anchorLinkUrl + "#anchor"));

            loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), anchorLinkUrl);

            final int shouldIgnoreNavigationCallCount =
                shouldIgnoreNavigationHelper.getCallCount();

            clickOnLinkUsingJs(awContents, contentsClient);

            // After we load this URL we're certain that any in-flight callbacks for the previous
            // navigation have been delivered.
            loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), ABOUT_BLANK_URL);

            assertEquals(shouldIgnoreNavigationCallCount,
                    shouldIgnoreNavigationHelper.getCallCount());
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCalledWhenLinkClicked() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
                contentsClient.getShouldIgnoreNavigationHelper();

        // We can't go to about:blank from here because we'd get a cross-origin error.
        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                getHtmlForPageWithSimpleLinkTo(DATA_URL), "text/html", false);

        int callCount = shouldIgnoreNavigationHelper.getCallCount();

        clickOnLinkUsingJs(awContents, contentsClient);

        shouldIgnoreNavigationHelper.waitForCallback(callCount);
    }


    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCalledWhenSelfLinkClicked() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
                contentsClient.getShouldIgnoreNavigationHelper();

        TestWebServer webServer = null;
        try {
            // Set up the HTML page.
            webServer = new TestWebServer(false);
            final String httpPath = "/page_with_link_to_self.html";
            final String httpPathOnServer = webServer.getResponseUrl(httpPath);
            addPageToTestServer(webServer, httpPath,
                    getHtmlForPageWithSimpleLinkTo(httpPathOnServer));

            loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                    httpPathOnServer);

            int callCount = shouldIgnoreNavigationHelper.getCallCount();

            clickOnLinkUsingJs(awContents, contentsClient);

            shouldIgnoreNavigationHelper.waitForCallback(callCount);
            assertEquals(httpPathOnServer,
                    shouldIgnoreNavigationHelper.getShouldIgnoreNavigationUrl());
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCalledWhenNavigatingFromJavaScriptUsingAssign()
            throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        enableJavaScriptOnUiThread(awContents);
        TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
                contentsClient.getShouldIgnoreNavigationHelper();

        TestWebServer webServer = null;
        try {
            // Set up the HTML page.
            webServer = new TestWebServer(false);
            final String redirectTargetUrl = createRedirectTargetPage(webServer);
            loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                    getHtmlForPageWithJsAssignLinkTo(redirectTargetUrl), "text/html", false);

            int callCount = shouldIgnoreNavigationHelper.getCallCount();

            clickOnLinkUsingJs(awContents, contentsClient);

            shouldIgnoreNavigationHelper.waitForCallback(callCount);
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCalledWhenNavigatingFromJavaScriptUsingReplace()
            throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        enableJavaScriptOnUiThread(awContents);
        TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
                contentsClient.getShouldIgnoreNavigationHelper();

        TestWebServer webServer = null;
        try {
            // Set up the HTML page.
            webServer = new TestWebServer(false);
            final String redirectTargetUrl = createRedirectTargetPage(webServer);
            loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                    getHtmlForPageWithJsReplaceLinkTo(redirectTargetUrl), "text/html", false);

            int callCount = shouldIgnoreNavigationHelper.getCallCount();
            clickOnLinkUsingJs(awContents, contentsClient);
            shouldIgnoreNavigationHelper.waitForCallback(callCount);
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationPassesCorrectUrl() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
                contentsClient.getShouldIgnoreNavigationHelper();

        TestWebServer webServer = null;
        try {
            // Set up the HTML page.
            webServer = new TestWebServer(false);
            final String redirectTargetUrl = createRedirectTargetPage(webServer);
            loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                    getHtmlForPageWithSimpleLinkTo(redirectTargetUrl), "text/html", false);

            int callCount = shouldIgnoreNavigationHelper.getCallCount();
            clickOnLinkUsingJs(awContents, contentsClient);
            shouldIgnoreNavigationHelper.waitForCallback(callCount);
            assertEquals(redirectTargetUrl,
                    shouldIgnoreNavigationHelper.getShouldIgnoreNavigationUrl());
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCanOverrideLoading() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
                contentsClient.getShouldIgnoreNavigationHelper();

        TestWebServer webServer = null;
        try {
            // Set up the HTML page.
            webServer = new TestWebServer(false);
            final String redirectTargetUrl = createRedirectTargetPage(webServer);
            loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                    getHtmlForPageWithSimpleLinkTo(redirectTargetUrl), "text/html", false);

            setShouldIgnoreNavigationReturnValueOnUiThread(shouldIgnoreNavigationHelper, true);

            OnPageFinishedHelper onPageFinishedHelper = contentsClient.getOnPageFinishedHelper();
            int onPageFinishedCountBeforeClickingOnLink = onPageFinishedHelper.getCallCount();
            int callCount = shouldIgnoreNavigationHelper.getCallCount();
            clickOnLinkUsingJs(awContents, contentsClient);
            // Some time around here true should be returned from the shouldIgnoreNavigation
            // callback causing the navigation caused by calling clickOnLinkUsingJs to be ignored.
            // We validate this by indirectly checking that an onPageFinished callback was not
            // issued after this point.
            shouldIgnoreNavigationHelper.waitForCallback(callCount);

            setShouldIgnoreNavigationReturnValueOnUiThread(shouldIgnoreNavigationHelper, false);

            final String synchronizationUrl = ABOUT_BLANK_URL;
            loadUrlSync(awContents, onPageFinishedHelper, synchronizationUrl);

            assertEquals(synchronizationUrl, onPageFinishedHelper.getUrl());
            assertEquals(onPageFinishedHelper.getCallCount(),
                    onPageFinishedCountBeforeClickingOnLink + 1);
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCalledForDataUrl() throws Throwable {
        final String dataUrl =
                "data:text/html;base64," +
                "PGh0bWw+PGhlYWQ+PHRpdGxlPmRhdGFVcmxUZXN0QmFzZTY0PC90aXRsZT48" +
                "L2hlYWQ+PC9odG1sPg==";
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
                contentsClient.getShouldIgnoreNavigationHelper();
        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                getHtmlForPageWithSimpleLinkTo(dataUrl), "text/html", false);

        int callCount = shouldIgnoreNavigationHelper.getCallCount();
        clickOnLinkUsingJs(awContents, contentsClient);

        shouldIgnoreNavigationHelper.waitForCallback(callCount);
        assertTrue("Expected URL that starts with 'data:' but got: <" +
                   shouldIgnoreNavigationHelper.getShouldIgnoreNavigationUrl() + "> instead.",
                   shouldIgnoreNavigationHelper.getShouldIgnoreNavigationUrl().startsWith(
                           "data:"));
    }

    /**
     * Worker method for the various redirect tests.
     *
     * Calling this will first load the redirect URL built from redirectFilePath, query and
     * locationFilePath and assert that we get a override callback for the destination.
     * The second part of the test loads a page that contains a link which points at the redirect
     * URL. We expect two callbacks - one for the redirect link and another for the destination.
     */
    private void doTestShouldIgnoreNavigationCalledOnRedirect(TestWebServer webServer,
            String redirectUrl, String redirectTarget) throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final String pageWithLinkToRedirectUrl = addPageToTestServer(webServer,
                "/page_with_link_to_redirect.html",
                getHtmlForPageWithSimpleLinkTo(redirectUrl));
        enableJavaScriptOnUiThread(awContents);

        TestAwContentsClient.ShouldIgnoreNavigationHelper shouldIgnoreNavigationHelper =
                contentsClient.getShouldIgnoreNavigationHelper();
        int directLoadCallCount = shouldIgnoreNavigationHelper.getCallCount();
        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), redirectUrl);

        shouldIgnoreNavigationHelper.waitForCallback(directLoadCallCount, 1);
        assertEquals(redirectTarget,
                shouldIgnoreNavigationHelper.getShouldIgnoreNavigationUrl());

        // There is a slight difference between navigations caused by calling load and navigations
        // caused by clicking on a link:
        //  * when using load the navigation is treated as if it came from the URL bar (has the
        //    navigation type TYPED and doesn't have the has_user_gesture flag)
        //  * when clicking on a link the navigation has the LINK type and has_user_gesture is
        //    true.
        // Both of these should yield the same result which is what we're verifying here.
        int indirectLoadCallCount = shouldIgnoreNavigationHelper.getCallCount();
        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                pageWithLinkToRedirectUrl);

        assertEquals(indirectLoadCallCount, shouldIgnoreNavigationHelper.getCallCount());

        clickOnLinkUsingJs(awContents, contentsClient);

        shouldIgnoreNavigationHelper.waitForCallback(indirectLoadCallCount, 2);
        assertEquals(redirectTarget,
                shouldIgnoreNavigationHelper.getShouldIgnoreNavigationUrl());
        assertEquals(redirectUrl,
                shouldIgnoreNavigationHelper.getPreviousShouldIgnoreNavigationUrl());
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCalledOn302Redirect() throws Throwable {
        TestWebServer webServer = null;
        try {
            // Set up the HTML page.
            webServer = new TestWebServer(false);
            final String redirectTargetUrl = createRedirectTargetPage(webServer);
            final String redirectUrl = webServer.setRedirect("/302.html", redirectTargetUrl);

            doTestShouldIgnoreNavigationCalledOnRedirect(webServer, redirectUrl,
                    redirectTargetUrl);
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCalledOnMetaRefreshRedirect() throws Throwable {
        TestWebServer webServer = null;
        try {
            // Set up the HTML page.
            webServer = new TestWebServer(false);
            final String redirectTargetUrl = createRedirectTargetPage(webServer);
            final String redirectUrl = addPageToTestServer(webServer, "/meta_refresh.html",
                    getHtmlForPageWithMetaRefreshRedirectTo(redirectTargetUrl));
            doTestShouldIgnoreNavigationCalledOnRedirect(webServer, redirectUrl,
                    redirectTargetUrl);
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }


    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCalledOnJavaScriptLocationImmediateAssignRedirect()
            throws Throwable {
        TestWebServer webServer = null;
        try {
            // Set up the HTML page.
            webServer = new TestWebServer(false);
            final String redirectTargetUrl = createRedirectTargetPage(webServer);
            final String redirectUrl = addPageToTestServer(webServer, "/js_immediate_assign.html",
                    getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Assign", 0));
            doTestShouldIgnoreNavigationCalledOnRedirect(webServer, redirectUrl,
                    redirectTargetUrl);
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCalledOnJavaScriptLocationImmediateReplaceRedirect()
            throws Throwable {
        TestWebServer webServer = null;
        try {
            // Set up the HTML page.
            webServer = new TestWebServer(false);
            final String redirectTargetUrl = createRedirectTargetPage(webServer);
            final String redirectUrl = addPageToTestServer(webServer, "/js_immediate_replace.html",
                    getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Replace", 0));
            doTestShouldIgnoreNavigationCalledOnRedirect(webServer, redirectUrl,
                    redirectTargetUrl);
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCalledOnJavaScriptLocationDelayedAssignRedirect()
            throws Throwable {
        TestWebServer webServer = null;
        try {
            // Set up the HTML page.
            webServer = new TestWebServer(false);
            final String redirectTargetUrl = createRedirectTargetPage(webServer);
            final String redirectUrl = addPageToTestServer(webServer, "/js_delayed_assign.html",
                    getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Assign", 100));
            doTestShouldIgnoreNavigationCalledOnRedirect(webServer, redirectUrl,
                    redirectTargetUrl);
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView", "Navigation"})
    public void testShouldIgnoreNavigationCalledOnJavaScriptLocationDelayedReplaceRedirect()
            throws Throwable {
        TestWebServer webServer = null;
        try {
            // Set up the HTML page.
            webServer = new TestWebServer(false);
            final String redirectTargetUrl = createRedirectTargetPage(webServer);
            final String redirectUrl = addPageToTestServer(webServer, "/js_delayed_replace.html",
                    getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Replace", 100));
            doTestShouldIgnoreNavigationCalledOnRedirect(webServer, redirectUrl,
                    redirectTargetUrl);
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }
}
