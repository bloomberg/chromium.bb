// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.MediumTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.CountDownLatch;

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
    public void testPassesCorrectUrl() throws Throwable {
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
    @FlakyTest(message = "crbug.com/652577")
    public void testCalledAfterError() throws Throwable {
        class LocalTestClient extends TestAwContentsClient {
            private boolean mIsOnReceivedErrorCalled;
            private boolean mIsOnPageFinishedCalled;
            private boolean mAllowAboutBlank;

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
    public void testCalledAfterRedirectedUrlIsOverridden() throws Throwable {
        /*
         * If url1 is redirected url2, and url2 load is overridden, onPageFinished should still be
         * called for url2.
         * Steps:
         * 1. load url1. url1 onPageStarted
         * 2. server redirects url1 to url2. url2 onPageStarted
         * 3. shouldOverridedUrlLoading called for url2 and returns true
         * 4. url2 onPageFinishedCalled
         */

        TestWebServer webServer = TestWebServer.start();
        try {
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
            webServer.shutdown();
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledForValidSubresources() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        TestWebServer webServer = TestWebServer.start();
        try {
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
            webServer.shutdown();
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledForHistoryApi() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        enableJavaScriptOnUiThread(mAwContents);

        TestWebServer webServer = TestWebServer.start();
        try {
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
            webServer.shutdown();
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledForHrefNavigations() throws Throwable {
        doTestOnPageFinishedCalledForHrefNavigations(false);
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledForHrefNavigationsWithBaseUrl() throws Throwable {
        doTestOnPageFinishedCalledForHrefNavigations(true);
    }

    private void doTestOnPageFinishedCalledForHrefNavigations(boolean useBaseUrl) throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        TestCallbackHelperContainer.OnPageStartedHelper onPageStartedHelper =
                mContentsClient.getOnPageStartedHelper();
        enableJavaScriptOnUiThread(mAwContents);

        TestWebServer webServer = TestWebServer.start();
        try {
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
            webServer.shutdown();
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledOnDomModificationForBlankWebView() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            doTestOnPageFinishedNotCalledOnDomMutation(webServer, null);
        } finally {
            webServer.shutdown();
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledOnDomModificationAfterNonCommittedLoadFromApi()
            throws Throwable {
        enableJavaScriptOnUiThread(mAwContents);
        TestWebServer webServer = TestWebServer.start();
        try {
            final String noContentUrl = webServer.setResponseWithNoContentStatus("/nocontent.html");
            loadUrlAsync(mAwContents, noContentUrl);
            doTestOnPageFinishedNotCalledOnDomMutation(webServer, noContentUrl);
        } finally {
            webServer.shutdown();
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledOnDomModificationWithJavascriptUrlAfterNonCommittedLoadFromApi()
            throws Throwable {
        enableJavaScriptOnUiThread(mAwContents);
        TestWebServer webServer = TestWebServer.start();
        try {
            final CountDownLatch latch = new CountDownLatch(1);
            final String url = webServer.setResponseWithRunnableAction(
                    "/about.html", CommonResources.ABOUT_HTML, null,
                    new Runnable() {
                        @Override
                        public void run() {
                            try {
                                assertTrue(latch.await(WAIT_TIMEOUT_MS,
                                        java.util.concurrent.TimeUnit.MILLISECONDS));
                            } catch (InterruptedException e) {
                                fail("Caught InterruptedException " + e);
                            }
                        }
                    });
            TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();
            final int onPageFinishedCallCount = onPageFinishedHelper.getCallCount();
            loadUrlAsync(mAwContents, url);
            loadUrlAsync(mAwContents,
                    "javascript:(function(){document.body.innerHTML='Hello,%20World!';})()");
            stopLoading(mAwContents);
            // We now have 3 possible outcomes:
            //  - the good one -- onPageFinished only fires for the first load;
            //  - two bad ones:
            //      - onPageFinished fires for the dom mutation, then for the first load. (1)
            //      - onPageFinished fires for the first load, then for the dom mutation; (2)
            // We verify that (1) doesn't happen with the code below. Then we load a sync page,
            // and make sure that we are getting onPageFinished for the sync page, not due
            // to the dom mutation, thus verifying that (2) doesn't happen as well.
            onPageFinishedHelper.waitForCallback(onPageFinishedCallCount);
            assertEquals(url, onPageFinishedHelper.getUrl());
            assertEquals(onPageFinishedCallCount + 1, onPageFinishedHelper.getCallCount());
            latch.countDown();  // Release the server.
            final String syncUrl = webServer.setResponse("/sync.html", "", null);
            loadUrlAsync(mAwContents, syncUrl);
            onPageFinishedHelper.waitForCallback(onPageFinishedCallCount + 1);
            assertEquals(syncUrl, onPageFinishedHelper.getUrl());
            assertEquals(onPageFinishedCallCount + 2, onPageFinishedHelper.getCallCount());
        } finally {
            webServer.shutdown();
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledOnDomModificationAfterLoadUrl() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String testUrl =
                    webServer.setResponse("/test.html", CommonResources.ABOUT_HTML, null);
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), testUrl);
            doTestOnPageFinishedNotCalledOnDomMutation(webServer, null);
        } finally {
            webServer.shutdown();
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledOnDomModificationAfterLoadData()
            throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                    CommonResources.ABOUT_HTML, "text/html", false);
            doTestOnPageFinishedNotCalledOnDomMutation(webServer, null);
        } finally {
            webServer.shutdown();
        }
    }

    private void doTestOnPageFinishedNotCalledOnDomMutation(TestWebServer webServer, String syncUrl)
            throws Throwable {
        enableJavaScriptOnUiThread(mAwContents);
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        final int onPageFinishedCallCount = onPageFinishedHelper.getCallCount();
        // Mutate DOM.
        executeJavaScriptAndWaitForResult(mAwContents, mContentsClient,
                "document.body.innerHTML='Hello, World!'");
        // Rather than wait a fixed time to see that an onPageFinished callback isn't issued
        // we load another valid page. Since callbacks arrive sequentially if the next callback
        // we get is for the synchronizationUrl we know that DOM mutation did not schedule
        // a callback for the iframe.
        if (syncUrl == null) {
            syncUrl = webServer.setResponse("/sync.html", "", null);
            loadUrlAsync(mAwContents, syncUrl);
        }
        onPageFinishedHelper.waitForCallback(onPageFinishedCallCount);
        assertEquals(syncUrl, onPageFinishedHelper.getUrl());
        assertEquals(onPageFinishedCallCount + 1, onPageFinishedHelper.getCallCount());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledAfter204Reply() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String url = webServer.setResponseWithNoContentStatus("/page.html");
            TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();
            int currentCallCount = onPageFinishedHelper.getCallCount();
            loadUrlAsync(mAwContents, url);
            onPageFinishedHelper.waitForCallback(currentCallCount);
            assertEquals(url, onPageFinishedHelper.getUrl());
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Ensure onPageFinished is called for an empty response (if the response status isn't
     * HttpStatus.SC_NO_CONTENT).
     */
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledOnEmptyResponse() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String url = webServer.setEmptyResponse("/page.html");
            TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();
            int currentCallCount = onPageFinishedHelper.getCallCount();
            loadUrlAsync(mAwContents, url);
            onPageFinishedHelper.waitForCallback(currentCallCount);
            assertEquals(url, onPageFinishedHelper.getUrl());
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Ensure onPageFinished is called when a provisional load is cancelled.
     */
    @MediumTest
    @Feature({"AndroidWebView"})
    @RetryOnFailure
    public void testCalledOnCancelingProvisionalLoad() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        final CountDownLatch testDoneLatch = new CountDownLatch(1);
        try {
            final String url = webServer.setResponseWithRunnableAction(
                    "/slow_page.html", "", null /* headers */, new Runnable() {
                        @Override
                        public void run() {
                            try {
                                assertTrue(testDoneLatch.await(WAIT_TIMEOUT_MS,
                                        java.util.concurrent.TimeUnit.MILLISECONDS));
                            } catch (InterruptedException e) {
                                fail("Caught InterruptedException " + e);
                            }
                        }
                    });
            TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();
            int initialCallCount = onPageFinishedHelper.getCallCount();
            loadUrlAsync(mAwContents, url);

            // Cancel before getting a response from the server.
            stopLoading(mAwContents);

            onPageFinishedHelper.waitForCallback(initialCallCount);
            assertEquals(url, onPageFinishedHelper.getUrl());

            // Load another page to ensure onPageFinished isn't called several times.
            final String syncUrl = webServer.setResponse("/sync.html", "", null);
            final int synchronizationPageCallCount = onPageFinishedHelper.getCallCount();
            assertEquals(initialCallCount + 1, synchronizationPageCallCount);
            loadUrlAsync(mAwContents, syncUrl);

            onPageFinishedHelper.waitForCallback(synchronizationPageCallCount);
            assertEquals(syncUrl, onPageFinishedHelper.getUrl());
        } finally {
            testDoneLatch.countDown();
            webServer.shutdown();
        }
    }

    /**
     * Ensure onPageFinished is called when a committed load is cancelled.
     */
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledOnCancelingCommittedLoad() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        TestWebServer webServer = TestWebServer.start();
        final CountDownLatch serverImageUrlLatch = new CountDownLatch(1);
        final CountDownLatch testDoneLatch = new CountDownLatch(1);
        try {
            final String stallingImageUrl = webServer.setResponseWithRunnableAction(
                    "/stallingImage.html", "", null /* headers */, new Runnable() {
                        @Override
                        public void run() {
                            serverImageUrlLatch.countDown();
                            try {
                                assertTrue(testDoneLatch.await(WAIT_TIMEOUT_MS,
                                        java.util.concurrent.TimeUnit.MILLISECONDS));
                            } catch (InterruptedException e) {
                                fail("Caught InterruptedException " + e);
                            }
                        }
                    });

            final String mainPageHtml =
                    CommonResources.makeHtmlPageFrom("", "<img src=" + stallingImageUrl + ">");
            final String mainPageUrl = webServer.setResponse("/mainPage.html", mainPageHtml, null);

            assertEquals(0, onPageFinishedHelper.getCallCount());
            loadUrlAsync(mAwContents, mainPageUrl);

            assertTrue(serverImageUrlLatch.await(
                    WAIT_TIMEOUT_MS, java.util.concurrent.TimeUnit.MILLISECONDS));
            assertEquals(0, onPageFinishedHelper.getCallCount());
            // Our load isn't done since we haven't loaded the image - now cancel the load.
            stopLoading(mAwContents);
            onPageFinishedHelper.waitForCallback(0);
            assertEquals(1, onPageFinishedHelper.getCallCount());
        } finally {
            testDoneLatch.countDown();
            webServer.shutdown();
        }
    }
}
