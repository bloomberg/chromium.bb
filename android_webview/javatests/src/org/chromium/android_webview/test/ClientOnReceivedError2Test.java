// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.webkit.WebSettings;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.AwWebResourceError;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.ErrorCodeConversionHelper;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the ContentViewClient.onReceivedError2() method.
 */
public class ClientOnReceivedError2Test extends AwTestBase {

    private VerifyOnReceivedError2CallClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    private static final String BAD_HTML_URL =
            "http://id.be.really.surprised.if.this.address.existed/a.html";

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new VerifyOnReceivedError2CallClient();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
    }

    @Override
    protected void tearDown() throws Exception {
        if (mWebServer != null) mWebServer.shutdown();
        super.tearDown();
    }

    private void startWebServer() throws Exception {
        mWebServer = TestWebServer.start();
    }

    private void useDefaultTestAwContentsClient() throws Exception {
        mContentsClient.enableBypass();
    }

    private static class VerifyOnReceivedError2CallClient extends TestAwContentsClient {
        private boolean mBypass = false;
        private boolean mIsOnPageFinishedCalled = false;
        private boolean mIsOnReceivedError2Called = false;

        void enableBypass() {
            mBypass = true;
        }

        @Override
        public void onPageFinished(String url) {
            if (!mBypass) {
                assertEquals(
                        "onPageFinished called twice for " + url, false, mIsOnPageFinishedCalled);
                mIsOnPageFinishedCalled = true;
                assertEquals("onReceivedError2 not called before onPageFinished for " + url, true,
                        mIsOnReceivedError2Called);
            }
            super.onPageFinished(url);
        }

        @Override
        public void onReceivedError2(AwWebResourceRequest request,
                AwWebResourceError error) {
            if (!mBypass) {
                assertEquals("onReceivedError2 called twice for " + request.url, false,
                        mIsOnReceivedError2Called);
                mIsOnReceivedError2Called = true;
            }
            super.onReceivedError2(request, error);
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMainFrame() throws Throwable {
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), BAD_HTML_URL);

        TestAwContentsClient.OnReceivedError2Helper onReceivedError2Helper =
                mContentsClient.getOnReceivedError2Helper();
        AwWebResourceRequest request = onReceivedError2Helper.getRequest();
        assertNotNull(request);
        assertEquals(BAD_HTML_URL, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        // No actual request has been made, as the host name can't be resolved.
        assertTrue(request.requestHeaders.isEmpty());
        assertTrue(request.isMainFrame);
        assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedError2Helper.getError();
        // The particular error code that is returned depends on the configuration of the device
        // (such as existence of a proxy) so we don't test for it.
        assertFalse(ErrorCodeConversionHelper.ERROR_UNKNOWN == error.errorCode);
        assertNotNull(error.description);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUserGesture() throws Throwable {
        useDefaultTestAwContentsClient();
        final String pageHtml = CommonResources.makeHtmlPageWithSimpleLinkTo(BAD_HTML_URL);
        loadDataAsync(mAwContents, pageHtml, "text/html", false);
        waitForPixelColorAtCenterOfView(mAwContents,
                mTestContainerView, CommonResources.LINK_COLOR);

        TestAwContentsClient.OnReceivedError2Helper onReceivedError2Helper =
                mContentsClient.getOnReceivedError2Helper();
        int onReceivedError2CallCount = onReceivedError2Helper.getCallCount();
        AwTestTouchUtils.simulateTouchCenterOfView(mTestContainerView);
        onReceivedError2Helper.waitForCallback(onReceivedError2CallCount,
                1 /* numberOfCallsToWaitFor */,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        AwWebResourceRequest request = onReceivedError2Helper.getRequest();
        assertNotNull(request);
        assertEquals(BAD_HTML_URL, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        // No actual request has been made, as the host name can't be resolved.
        assertTrue(request.requestHeaders.isEmpty());
        assertTrue(request.isMainFrame);
        assertTrue(request.hasUserGesture);
        AwWebResourceError error = onReceivedError2Helper.getError();
        // The particular error code that is returned depends on the configuration of the device
        // (such as existence of a proxy) so we don't test for it.
        assertFalse(ErrorCodeConversionHelper.ERROR_UNKNOWN == error.errorCode);
        assertNotNull(error.description);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testIframeSubresource() throws Throwable {
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<iframe src='" + BAD_HTML_URL + "' />");
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                pageHtml, "text/html", false);

        TestAwContentsClient.OnReceivedError2Helper onReceivedError2Helper =
                mContentsClient.getOnReceivedError2Helper();
        AwWebResourceRequest request = onReceivedError2Helper.getRequest();
        assertNotNull(request);
        assertEquals(BAD_HTML_URL, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        // No actual request has been made, as the host name can't be resolved.
        assertTrue(request.requestHeaders.isEmpty());
        assertFalse(request.isMainFrame);
        assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedError2Helper.getError();
        // The particular error code that is returned depends on the configuration of the device
        // (such as existence of a proxy) so we don't test for it.
        assertFalse(ErrorCodeConversionHelper.ERROR_UNKNOWN == error.errorCode);
        assertNotNull(error.description);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @RetryOnFailure(message = "crbug.com/653130")
    public void testUserGestureForIframeSubresource() throws Throwable {
        useDefaultTestAwContentsClient();
        startWebServer();
        final String iframeHtml = CommonResources.makeHtmlPageWithSimpleLinkTo(BAD_HTML_URL);
        final String iframeUrl = mWebServer.setResponse("/iframe.html", iframeHtml, null);
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<iframe style='width:100%;height:100%;' src='" + iframeUrl + "' />");
        loadDataAsync(mAwContents, pageHtml, "text/html", false);
        waitForPixelColorAtCenterOfView(mAwContents,
                mTestContainerView, CommonResources.LINK_COLOR);

        TestAwContentsClient.OnReceivedError2Helper onReceivedError2Helper =
                mContentsClient.getOnReceivedError2Helper();
        int onReceivedError2CallCount = onReceivedError2Helper.getCallCount();
        AwTestTouchUtils.simulateTouchCenterOfView(mTestContainerView);
        onReceivedError2Helper.waitForCallback(onReceivedError2CallCount,
                1 /* numberOfCallsToWaitFor */,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        AwWebResourceRequest request = onReceivedError2Helper.getRequest();
        assertNotNull(request);
        assertEquals(BAD_HTML_URL, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        // No actual request has been made, as the host name can't be resolved.
        assertTrue(request.requestHeaders.isEmpty());
        assertFalse(request.isMainFrame);
        assertTrue(request.hasUserGesture);
        AwWebResourceError error = onReceivedError2Helper.getError();
        // The particular error code that is returned depends on the configuration of the device
        // (such as existence of a proxy) so we don't test for it.
        assertFalse(ErrorCodeConversionHelper.ERROR_UNKNOWN == error.errorCode);
        assertNotNull(error.description);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testImageSubresource() throws Throwable {
        final String imageUrl = "http://man.id.be.really.surprised.if.this.address.existed/a.png";
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<img src='" + imageUrl + "' />");
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                pageHtml, "text/html", false);

        TestAwContentsClient.OnReceivedError2Helper onReceivedError2Helper =
                mContentsClient.getOnReceivedError2Helper();
        AwWebResourceRequest request = onReceivedError2Helper.getRequest();
        assertNotNull(request);
        assertEquals(imageUrl, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        // No actual request has been made, as the host name can't be resolved.
        assertTrue(request.requestHeaders.isEmpty());
        assertFalse(request.isMainFrame);
        assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedError2Helper.getError();
        // The particular error code that is returned depends on the configuration of the device
        // (such as existence of a proxy) so we don't test for it.
        assertFalse(ErrorCodeConversionHelper.ERROR_UNKNOWN == error.errorCode);
        assertNotNull(error.description);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnInvalidScheme() throws Throwable {
        final String iframeUrl = "foo://some/resource";
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<iframe src='" + iframeUrl + "' />");
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                pageHtml, "text/html", false);

        TestAwContentsClient.OnReceivedError2Helper onReceivedError2Helper =
                mContentsClient.getOnReceivedError2Helper();
        AwWebResourceRequest request = onReceivedError2Helper.getRequest();
        assertNotNull(request);
        assertEquals(iframeUrl, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        assertFalse(request.requestHeaders.isEmpty());
        assertFalse(request.isMainFrame);
        assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedError2Helper.getError();
        assertEquals(ErrorCodeConversionHelper.ERROR_UNSUPPORTED_SCHEME, error.errorCode);
        assertNotNull(error.description);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnNonExistentAssetUrl() throws Throwable {
        final String baseUrl = "file:///android_asset/";
        final String iframeUrl = baseUrl + "does_not_exist.html";
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<iframe src='" + iframeUrl + "' />");
        loadDataWithBaseUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                pageHtml, "text/html", false, baseUrl, "about:blank");

        TestAwContentsClient.OnReceivedError2Helper onReceivedError2Helper =
                mContentsClient.getOnReceivedError2Helper();
        AwWebResourceRequest request = onReceivedError2Helper.getRequest();
        assertNotNull(request);
        assertEquals(iframeUrl, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        assertFalse(request.requestHeaders.isEmpty());
        assertFalse(request.isMainFrame);
        assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedError2Helper.getError();
        assertEquals(ErrorCodeConversionHelper.ERROR_UNKNOWN, error.errorCode);
        assertNotNull(error.description);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnNonExistentResourceUrl() throws Throwable {
        final String baseUrl = "file:///android_res/raw/";
        final String iframeUrl = baseUrl + "does_not_exist.html";
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<iframe src='" + iframeUrl + "' />");
        loadDataWithBaseUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageHtml,
                "text/html", false, baseUrl, "about:blank");

        TestAwContentsClient.OnReceivedError2Helper onReceivedError2Helper =
                mContentsClient.getOnReceivedError2Helper();
        AwWebResourceRequest request = onReceivedError2Helper.getRequest();
        assertNotNull(request);
        assertEquals(iframeUrl, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        assertFalse(request.requestHeaders.isEmpty());
        assertFalse(request.isMainFrame);
        assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedError2Helper.getError();
        assertEquals(ErrorCodeConversionHelper.ERROR_UNKNOWN, error.errorCode);
        assertNotNull(error.description);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnCacheMiss() throws Throwable {
        final String iframeUrl = "http://example.com/index.html";
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<iframe src='" + iframeUrl + "' />");
        getAwSettingsOnUiThread(mAwContents).setCacheMode(WebSettings.LOAD_CACHE_ONLY);
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                pageHtml, "text/html", false);

        TestAwContentsClient.OnReceivedError2Helper onReceivedError2Helper =
                mContentsClient.getOnReceivedError2Helper();
        AwWebResourceRequest request = onReceivedError2Helper.getRequest();
        assertNotNull(request);
        assertEquals(iframeUrl, request.url);
        assertEquals("GET", request.method);
        assertNotNull(request.requestHeaders);
        assertFalse(request.requestHeaders.isEmpty());
        assertFalse(request.isMainFrame);
        assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedError2Helper.getError();
        assertEquals(ErrorCodeConversionHelper.ERROR_UNKNOWN, error.errorCode);
        assertNotNull(error.description);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotCalledOnStopLoading() throws Throwable {
        useDefaultTestAwContentsClient();
        final CountDownLatch latch = new CountDownLatch(1);
        startWebServer();
        final String url = mWebServer.setResponseWithRunnableAction(
                "/about.html", CommonResources.ABOUT_HTML, null,
                new Runnable() {
                    @Override
                    public void run() {
                        try {
                            latch.await(
                                    WAIT_TIMEOUT_MS, java.util.concurrent.TimeUnit.MILLISECONDS);
                        } catch (InterruptedException e) {
                            fail("Caught InterruptedException " + e);
                        }
                    }
                });
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        final int onPageFinishedCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(mAwContents, url);
        stopLoading(mAwContents);
        onPageFinishedHelper.waitForCallback(onPageFinishedCallCount,
                1 /* numberOfCallsToWaitFor */,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        latch.countDown(); // Release the server.

        // Instead of waiting for OnReceivedError2 not to be called, we schedule
        // a load that will result in a error, and check that we have only got one callback,
        // originating from the last attempt.
        TestAwContentsClient.OnReceivedError2Helper onReceivedError2Helper =
                mContentsClient.getOnReceivedError2Helper();
        final int onReceivedError2CallCount = onReceivedError2Helper.getCallCount();
        loadUrlAsync(mAwContents, BAD_HTML_URL);
        onReceivedError2Helper.waitForCallback(onReceivedError2CallCount,
                1 /* numberOfCallsToWaitFor */,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        assertEquals(onReceivedError2CallCount + 1, onReceivedError2Helper.getCallCount());
        assertEquals(BAD_HTML_URL, onReceivedError2Helper.getRequest().url);
    }
}
