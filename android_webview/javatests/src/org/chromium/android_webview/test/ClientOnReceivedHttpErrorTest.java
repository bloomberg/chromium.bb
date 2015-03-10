// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the ContentViewClient.onReceivedHttpError() method.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
public class ClientOnReceivedHttpErrorTest extends AwTestBase {

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mWebServer = TestWebServer.start();
    }

    private void setTestAwContentsClient(TestAwContentsClient contentsClient) throws Exception {
        mContentsClient = contentsClient;
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @Override
    protected void tearDown() throws Exception {
        if (mWebServer != null) mWebServer.shutdown();
        super.tearDown();
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnReceivedHttpErrorForMainResource() throws Throwable {
        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();

        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html; charset=utf-8"));
        headers.add(Pair.create("Coalesce", ""));
        headers.add(Pair.create("Coalesce", "a"));
        headers.add(Pair.create("Coalesce", ""));
        headers.add(Pair.create("Coalesce", "a"));
        final String url = mWebServer.setResponseWithNotFoundStatus("/404.html", headers);
        int onReceivedHttpErrorCallCount = onReceivedHttpErrorHelper.getCallCount();
        loadUrlAsync(mAwContents, url);

        onReceivedHttpErrorHelper.waitForCallback(onReceivedHttpErrorCallCount,
                1 /* numberOfCallsToWaitFor */,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
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
    public void testOnReceivedHttpErrorForUserGesture() throws Throwable {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html; charset=utf-8"));
        final String badUrl = mWebServer.setResponseWithNotFoundStatus("/404.html", headers);
        final String pageWithLinkUrl = mWebServer.setResponse("/page_with_link.html",
                CommonResources.makeHtmlPageWithSimpleLinkTo(badUrl), null);
        enableJavaScriptOnUiThread(mAwContents);
        loadUrlAsync(mAwContents, pageWithLinkUrl);
        waitForVisualStateCallback(mAwContents);

        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        AwTestTouchUtils.simulateTouchCenterOfView(mTestContainerView);
        int onReceivedHttpErrorCallCount = onReceivedHttpErrorHelper.getCallCount();
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
    public void testOnReceivedHttpErrorForSubresource() throws Throwable {
        class LocalTestClient extends TestAwContentsClient {
            private boolean mIsOnReceivedHttpErrorCalled = false;
            private boolean mIsOnPageFinishedCalled = false;

            @Override
            public void onReceivedHttpError(AwWebResourceRequest request,
                    AwWebResourceResponse response) {
                assertEquals("onReceivedHttpError called twice for " + request.url,
                        false, mIsOnReceivedHttpErrorCalled);
                mIsOnReceivedHttpErrorCalled = true;
                super.onReceivedHttpError(request, response);
            }

            @Override
            public void onPageFinished(String url) {
                mIsOnPageFinishedCalled = true;
                super.onPageFinished(url);
            }
        }
        LocalTestClient testContentsClient = new LocalTestClient();
        setTestAwContentsClient(testContentsClient);

        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html; charset=utf-8"));
        final String imageUrl = mWebServer.setResponseWithNotFoundStatus("/404.png", headers);
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<img src='" + imageUrl + "' class='img.big' />");
        final String pageUrl = mWebServer.setResponse("/page.html", pageHtml, null);

        loadUrlSync(mAwContents, onPageFinishedHelper, pageUrl);

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
    public void testOnReceivedHttpErrorNotCalledIfNoHttpError() throws Throwable {
        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        final String goodUrl = mWebServer.setResponse("/1.html", CommonResources.ABOUT_HTML, null);
        final String badUrl = mWebServer.setResponseWithNotFoundStatus("/404.html");

        final int onReceivedHttpErrorCallCount = onReceivedHttpErrorHelper.getCallCount();
        loadUrlSync(mAwContents, onPageFinishedHelper, goodUrl);

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
}
