// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;
import android.util.Pair;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the ContentViewClient.onReceivedHttpError() method.
 */
public class ClientOnReceivedHttpErrorTest extends AwTestBase {

    private VerifyOnReceivedHttpErrorCallClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new VerifyOnReceivedHttpErrorCallClient();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mWebServer = TestWebServer.start();
    }

    private void useDefaultTestAwContentsClient() throws Exception {
        mContentsClient.enableBypass();
    }

    private static class VerifyOnReceivedHttpErrorCallClient extends TestAwContentsClient {
        private boolean mBypass;
        private boolean mIsOnPageFinishedCalled;
        private boolean mIsOnReceivedHttpErrorCalled;

        void enableBypass() {
            mBypass = true;
        }

        @Override
        public void onPageFinished(String url) {
            if (!mBypass) {
                assertEquals(
                        "onPageFinished called twice for " + url, false, mIsOnPageFinishedCalled);
                mIsOnPageFinishedCalled = true;
                assertEquals("onReceivedHttpError not called before onPageFinished for " + url,
                        true, mIsOnReceivedHttpErrorCalled);
            }
            super.onPageFinished(url);
        }

        @Override
        public void onReceivedHttpError(
                AwWebResourceRequest request, AwWebResourceResponse response) {
            if (!mBypass) {
                assertEquals("onReceivedHttpError called twice for " + request.url, false,
                        mIsOnReceivedHttpErrorCalled);
                mIsOnReceivedHttpErrorCalled = true;
            }
            super.onReceivedHttpError(request, response);
        }
    }

    @Override
    protected void tearDown() throws Exception {
        if (mWebServer != null) mWebServer.shutdown();
        super.tearDown();
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testForMainResource() throws Throwable {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html; charset=utf-8"));
        headers.add(Pair.create("Coalesce", ""));
        headers.add(Pair.create("Coalesce", "a"));
        headers.add(Pair.create("Coalesce", ""));
        headers.add(Pair.create("Coalesce", "a"));
        final String url = mWebServer.setResponseWithNotFoundStatus("/404.html", headers);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        AwWebResourceRequest request = onReceivedHttpErrorHelper.getRequest();
        assertNotNull(request);
        assertEquals(url, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        assertFalse(request.requestHeaders.isEmpty());
        assertTrue(request.isMainFrame);
        assertFalse(request.hasUserGesture);
        AwWebResourceResponse response = onReceivedHttpErrorHelper.getResponse();
        assertEquals(404, response.getStatusCode());
        assertEquals("Not Found", response.getReasonPhrase());
        assertEquals("text/html", response.getMimeType());
        assertEquals("utf-8", response.getCharset());
        assertNotNull(response.getResponseHeaders());
        assertTrue(response.getResponseHeaders().containsKey("Content-Type"));
        assertEquals("text/html; charset=utf-8", response.getResponseHeaders().get("Content-Type"));
        assertTrue(response.getResponseHeaders().containsKey("Coalesce"));
        assertEquals("a, a", response.getResponseHeaders().get("Coalesce"));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testForUserGesture() throws Throwable {
        useDefaultTestAwContentsClient();
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html; charset=utf-8"));
        final String badUrl = mWebServer.setResponseWithNotFoundStatus("/404.html", headers);
        final String pageWithLinkUrl = mWebServer.setResponse("/page_with_link.html",
                CommonResources.makeHtmlPageWithSimpleLinkTo(badUrl), null);
        enableJavaScriptOnUiThread(mAwContents);
        loadUrlAsync(mAwContents, pageWithLinkUrl);
        waitForPixelColorAtCenterOfView(mAwContents,
                mTestContainerView, CommonResources.LINK_COLOR);

        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        int onReceivedHttpErrorCallCount = onReceivedHttpErrorHelper.getCallCount();
        AwTestTouchUtils.simulateTouchCenterOfView(mTestContainerView);
        onReceivedHttpErrorHelper.waitForCallback(onReceivedHttpErrorCallCount,
                1 /* numberOfCallsToWaitFor */,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        AwWebResourceRequest request = onReceivedHttpErrorHelper.getRequest();
        assertNotNull(request);
        assertEquals(badUrl, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        assertFalse(request.requestHeaders.isEmpty());
        assertTrue(request.isMainFrame);
        assertTrue(request.hasUserGesture);
        AwWebResourceResponse response = onReceivedHttpErrorHelper.getResponse();
        assertEquals(404, response.getStatusCode());
        assertEquals("Not Found", response.getReasonPhrase());
        assertEquals("text/html", response.getMimeType());
        assertEquals("utf-8", response.getCharset());
        assertNotNull(response.getResponseHeaders());
        assertTrue(response.getResponseHeaders().containsKey("Content-Type"));
        assertEquals("text/html; charset=utf-8", response.getResponseHeaders().get("Content-Type"));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testForSubresource() throws Throwable {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html; charset=utf-8"));
        final String imageUrl = mWebServer.setResponseWithNotFoundStatus("/404.png", headers);
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<img src='" + imageUrl + "' class='img.big' />");
        final String pageUrl = mWebServer.setResponse("/page.html", pageHtml, null);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        AwWebResourceRequest request = onReceivedHttpErrorHelper.getRequest();
        assertNotNull(request);
        assertEquals(imageUrl, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        assertFalse(request.requestHeaders.isEmpty());
        assertFalse(request.isMainFrame);
        assertFalse(request.hasUserGesture);
        AwWebResourceResponse response = onReceivedHttpErrorHelper.getResponse();
        assertEquals(404, response.getStatusCode());
        assertEquals("Not Found", response.getReasonPhrase());
        assertEquals("text/html", response.getMimeType());
        assertEquals("utf-8", response.getCharset());
        assertNotNull(response.getResponseHeaders());
        assertTrue(response.getResponseHeaders().containsKey("Content-Type"));
        assertEquals("text/html; charset=utf-8", response.getResponseHeaders().get("Content-Type"));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotCalledIfNoHttpError() throws Throwable {
        useDefaultTestAwContentsClient();
        final String goodUrl = mWebServer.setResponse("/1.html", CommonResources.ABOUT_HTML, null);
        final String badUrl = mWebServer.setResponseWithNotFoundStatus("/404.html");
        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        final int onReceivedHttpErrorCallCount = onReceivedHttpErrorHelper.getCallCount();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), goodUrl);

        // Instead of waiting for OnReceivedHttpError not to be called, we schedule
        // a load that will result in a error, and check that we have only got one callback,
        // originating from the last attempt.
        loadUrlAsync(mAwContents, badUrl);
        onReceivedHttpErrorHelper.waitForCallback(onReceivedHttpErrorCallCount,
                1 /* numberOfCallsToWaitFor */,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        assertEquals(onReceivedHttpErrorCallCount + 1, onReceivedHttpErrorHelper.getCallCount());
        assertEquals(badUrl, onReceivedHttpErrorHelper.getRequest().url);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAfterRedirect() throws Throwable {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html; charset=utf-8"));
        final String secondUrl = mWebServer.setResponseWithNotFoundStatus("/404.html", headers);
        final String firstUrl = mWebServer.setRedirect("/302.html", secondUrl);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), firstUrl);

        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        AwWebResourceRequest request = onReceivedHttpErrorHelper.getRequest();
        assertNotNull(request);
        assertEquals(secondUrl, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        assertFalse(request.requestHeaders.isEmpty());
        assertTrue(request.isMainFrame);
        assertFalse(request.hasUserGesture);
        AwWebResourceResponse response = onReceivedHttpErrorHelper.getResponse();
        assertEquals(404, response.getStatusCode());
        assertEquals("Not Found", response.getReasonPhrase());
        assertEquals("text/html", response.getMimeType());
        assertEquals("utf-8", response.getCharset());
        assertNotNull(response.getResponseHeaders());
        assertTrue(response.getResponseHeaders().containsKey("Content-Type"));
        assertEquals("text/html; charset=utf-8", response.getResponseHeaders().get("Content-Type"));
    }
}
