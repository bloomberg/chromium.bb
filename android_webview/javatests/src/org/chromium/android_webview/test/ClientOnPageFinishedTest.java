// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests for the ContentViewClient.onPageFinished() method.
 */
public class ClientOnPageFinishedTest extends AwTestBase {

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        setTestAwContentsClient(new TestAwContentsClient());
    }

    private void setTestAwContentsClient(TestAwContentsClient contentsClient) throws Exception {
        mContentsClient = contentsClient;
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedPassesCorrectUrl() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        String html = "<html><body>Simple page.</body></html>";
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataAsync(mAwContents, html, "text/html", false);

        onPageFinishedHelper.waitForCallback(currentCallCount);
        assertEquals("data:text/html," + html, onPageFinishedHelper.getUrl());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedCalledAfterError() throws Throwable {
        class LocalTestClient extends TestAwContentsClient {
            private boolean mIsOnReceivedErrorCalled = false;
            private boolean mIsOnPageFinishedCalled = false;
            private boolean mAllowAboutBlank = false;

            @Override
            public void onReceivedError(int errorCode, String description, String failingUrl) {
                assertEquals("onReceivedError called twice for " + failingUrl,
                        false, mIsOnReceivedErrorCalled);
                mIsOnReceivedErrorCalled = true;
                assertEquals("onPageFinished called before onReceivedError for " + failingUrl,
                        false, mIsOnPageFinishedCalled);
                super.onReceivedError(errorCode, description, failingUrl);
            }

            @Override
            public void onPageFinished(String url) {
                if (mAllowAboutBlank && "about:blank".equals(url)) {
                    super.onPageFinished(url);
                    return;
                }
                assertEquals("onPageFinished called twice for " + url,
                        false, mIsOnPageFinishedCalled);
                mIsOnPageFinishedCalled = true;
                assertEquals("onReceivedError not called before onPageFinished for " + url,
                        true, mIsOnReceivedErrorCalled);
                super.onPageFinished(url);
            }

            void setAllowAboutBlank() {
                mAllowAboutBlank = true;
            }
        }
        LocalTestClient testContentsClient = new LocalTestClient();
        setTestAwContentsClient(testContentsClient);

        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        String invalidUrl = "http://localhost:7/non_existent";
        loadUrlSync(mAwContents, onPageFinishedHelper, invalidUrl);

        assertEquals(invalidUrl, onReceivedErrorHelper.getFailingUrl());
        assertEquals(invalidUrl, onPageFinishedHelper.getUrl());

        // Rather than wait a fixed time to see that another onPageFinished callback isn't issued
        // we load a valid page. Since callbacks arrive sequentially, this will ensure that
        // any extra calls of onPageFinished / onReceivedError will arrive to our client.
        testContentsClient.setAllowAboutBlank();
        loadUrlSync(mAwContents, onPageFinishedHelper, "about:blank");
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedCalledAfterRedirectedUrlIsOverridden() throws Throwable {
        /*
         * If url1 is redirected url2, and url2 load is overridden, onPageFinished should still be
         * called for url2.
         * Steps:
         * 1. load url1. url1 onPageStarted
         * 2. server redirects url1 to url2. url2 onPageStarted
         * 3. shouldOverridedUrlLoading called for url2 and returns true
         * 4. url2 onPageFinishedCalled
         */

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String redirectTargetPath = "/redirect_target.html";
            final String redirectTargetUrl = webServer.setResponse(redirectTargetPath,
                    "<html><body>hello world</body></html>", null);
            final String redirectUrl = webServer.setRedirect("/302.html", redirectTargetUrl);

            final TestAwContentsClient.ShouldOverrideUrlLoadingHelper urlOverrideHelper =
                    mContentsClient.getShouldOverrideUrlLoadingHelper();
            // Override the load of redirectTargetUrl
            urlOverrideHelper.setShouldOverrideUrlLoadingReturnValue(true);

            TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

            final int currentOnPageFinishedCallCount = onPageFinishedHelper.getCallCount();
            loadUrlAsync(mAwContents, redirectUrl);

            onPageFinishedHelper.waitForCallback(currentOnPageFinishedCallCount);
            // onPageFinished needs to be called for redirectTargetUrl, but not for redirectUrl
            assertEquals(redirectTargetUrl, onPageFinishedHelper.getUrl());
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedNotCalledForValidSubresources() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);

            final String testHtml = "<html><head>Header</head><body>Body</body></html>";
            final String testPath = "/test.html";
            final String syncPath = "/sync.html";

            final String testUrl = webServer.setResponse(testPath, testHtml, null);
            final String syncUrl = webServer.setResponse(syncPath, testHtml, null);

            assertEquals(0, onPageFinishedHelper.getCallCount());
            final int pageWithSubresourcesCallCount = onPageFinishedHelper.getCallCount();
            loadDataAsync(mAwContents,
                          "<html><iframe src=\"" + testUrl + "\" /></html>",
                          "text/html",
                          false);

            onPageFinishedHelper.waitForCallback(pageWithSubresourcesCallCount);

            // Rather than wait a fixed time to see that an onPageFinished callback isn't issued
            // we load another valid page. Since callbacks arrive sequentially if the next callback
            // we get is for the synchronizationUrl we know that the previous load did not schedule
            // a callback for the iframe.
            final int synchronizationPageCallCount = onPageFinishedHelper.getCallCount();
            loadUrlAsync(mAwContents, syncUrl);

            onPageFinishedHelper.waitForCallback(synchronizationPageCallCount);
            assertEquals(syncUrl, onPageFinishedHelper.getUrl());
            assertEquals(2, onPageFinishedHelper.getCallCount());

        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedNotCalledForHistoryApi() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        enableJavaScriptOnUiThread(mAwContents);

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);

            final String testHtml = "<html><head>Header</head><body>Body</body></html>";
            final String testPath = "/test.html";
            final String historyPath = "/history.html";
            final String syncPath = "/sync.html";

            final String testUrl = webServer.setResponse(testPath, testHtml, null);
            final String historyUrl = webServer.getResponseUrl(historyPath);
            final String syncUrl = webServer.setResponse(syncPath, testHtml, null);

            assertEquals(0, onPageFinishedHelper.getCallCount());
            loadUrlSync(mAwContents, onPageFinishedHelper, testUrl);

            executeJavaScriptAndWaitForResult(mAwContents, mContentsClient,
                    "history.pushState(null, null, '" + historyUrl + "');");

            // Rather than wait a fixed time to see that an onPageFinished callback isn't issued
            // we load another valid page. Since callbacks arrive sequentially if the next callback
            // we get is for the synchronizationUrl we know that the previous load did not schedule
            // a callback for the iframe.
            final int synchronizationPageCallCount = onPageFinishedHelper.getCallCount();
            loadUrlAsync(mAwContents, syncUrl);

            onPageFinishedHelper.waitForCallback(synchronizationPageCallCount);
            assertEquals(syncUrl, onPageFinishedHelper.getUrl());
            assertEquals(2, onPageFinishedHelper.getCallCount());

        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedCalledForHrefNavigations() throws Throwable {
        doTestOnPageFinishedCalledForHrefNavigations(false);
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedCalledForHrefNavigationsWithBaseUrl() throws Throwable {
        doTestOnPageFinishedCalledForHrefNavigations(true);
    }

    private void doTestOnPageFinishedCalledForHrefNavigations(boolean useBaseUrl) throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        TestCallbackHelperContainer.OnPageStartedHelper onPageStartedHelper =
                mContentsClient.getOnPageStartedHelper();
        enableJavaScriptOnUiThread(mAwContents);

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);

            final String testHtml = CommonResources.makeHtmlPageFrom("",
                    "<a href=\"#anchor\" id=\"link\">anchor</a>");
            final String testPath = "/test.html";
            final String testUrl = webServer.setResponse(testPath, testHtml, null);

            if (useBaseUrl) {
                loadDataWithBaseUrlSync(mAwContents, onPageFinishedHelper,
                        testHtml, "text/html", false, webServer.getBaseUrl(), null);
            } else {
                loadUrlSync(mAwContents, onPageFinishedHelper, testUrl);
            }

            int onPageFinishedCallCount = onPageFinishedHelper.getCallCount();
            int onPageStartedCallCount = onPageStartedHelper.getCallCount();

            JSUtils.clickOnLinkUsingJs(this, mAwContents,
                    mContentsClient.getOnEvaluateJavaScriptResultHelper(), "link");

            onPageFinishedHelper.waitForCallback(onPageFinishedCallCount);
            assertEquals(onPageStartedCallCount, onPageStartedHelper.getCallCount());

            onPageFinishedCallCount = onPageFinishedHelper.getCallCount();
            onPageStartedCallCount = onPageStartedHelper.getCallCount();

            executeJavaScriptAndWaitForResult(mAwContents, mContentsClient,
                    "window.history.go(-1)");

            onPageFinishedHelper.waitForCallback(onPageFinishedCallCount);
            assertEquals(onPageStartedCallCount, onPageStartedHelper.getCallCount());
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }
}
