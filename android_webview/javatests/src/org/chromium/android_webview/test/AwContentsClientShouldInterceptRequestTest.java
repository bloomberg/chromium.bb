// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;
import android.util.Pair;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnReceivedErrorHelper;
import org.chromium.net.test.util.TestWebServer;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CountDownLatch;

/**
 * Tests for the WebViewClient.shouldInterceptRequest() method.
 */
public class AwContentsClientShouldInterceptRequestTest extends AwTestBase {

    private static class ShouldInterceptRequestClient extends TestAwContentsClient {

        public static class ShouldInterceptRequestHelper extends CallbackHelper {
            private List<String> mShouldInterceptRequestUrls = new ArrayList<String>();
            private ConcurrentHashMap<String, AwWebResourceResponse> mReturnValuesByUrls =
                    new ConcurrentHashMap<String, AwWebResourceResponse>();
            private ConcurrentHashMap<String, AwWebResourceRequest> mRequestsByUrls =
                    new ConcurrentHashMap<String, AwWebResourceRequest>();
            // This is read on another thread, so needs to be marked volatile.
            private volatile AwWebResourceResponse mShouldInterceptRequestReturnValue;
            void setReturnValue(AwWebResourceResponse value) {
                mShouldInterceptRequestReturnValue = value;
            }
            void setReturnValueForUrl(String url, AwWebResourceResponse value) {
                mReturnValuesByUrls.put(url, value);
            }
            public List<String> getUrls() {
                assert getCallCount() > 0;
                return mShouldInterceptRequestUrls;
            }
            public AwWebResourceResponse getReturnValue(String url) {
                AwWebResourceResponse value = mReturnValuesByUrls.get(url);
                if (value != null) return value;
                return mShouldInterceptRequestReturnValue;
            }
            public AwWebResourceRequest getRequestsForUrl(String url) {
                assert getCallCount() > 0;
                assert mRequestsByUrls.containsKey(url);
                return mRequestsByUrls.get(url);
            }
            public void notifyCalled(AwWebResourceRequest request) {
                mShouldInterceptRequestUrls.add(request.url);
                mRequestsByUrls.put(request.url, request);
                notifyCalled();
            }
        }

        public static class OnLoadResourceHelper extends CallbackHelper {
            private String mUrl;

            public String getUrl() {
                assert getCallCount() > 0;
                return mUrl;
            }

            public void notifyCalled(String url) {
                mUrl = url;
                notifyCalled();
            }
        }

        @Override
        public AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request) {
            AwWebResourceResponse returnValue =
                    mShouldInterceptRequestHelper.getReturnValue(request.url);
            mShouldInterceptRequestHelper.notifyCalled(request);
            return returnValue;
        }

        @Override
        public void onLoadResource(String url) {
            super.onLoadResource(url);
            mOnLoadResourceHelper.notifyCalled(url);
        }

        private ShouldInterceptRequestHelper mShouldInterceptRequestHelper;
        private OnLoadResourceHelper mOnLoadResourceHelper;

        public ShouldInterceptRequestClient() {
            mShouldInterceptRequestHelper = new ShouldInterceptRequestHelper();
            mOnLoadResourceHelper = new OnLoadResourceHelper();
        }

        public ShouldInterceptRequestHelper getShouldInterceptRequestHelper() {
            return mShouldInterceptRequestHelper;
        }

        public OnLoadResourceHelper getOnLoadResourceHelper() {
            return mOnLoadResourceHelper;
        }
    }

    private static final int TEAPOT_STATUS_CODE = 418;
    private static final String TEAPOT_RESPONSE_PHRASE = "I'm a teapot";

    private String addPageToTestServer(TestWebServer webServer, String httpPath, String html) {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html"));
        headers.add(Pair.create("Cache-Control", "no-store"));
        return webServer.setResponse(httpPath, html, headers);
    }

    private String addAboutPageToTestServer(TestWebServer webServer) {
        return addPageToTestServer(webServer, "/" + CommonResources.ABOUT_FILENAME,
                CommonResources.ABOUT_HTML);
    }

    private AwWebResourceResponse stringToAwWebResourceResponse(String input) throws Throwable {
        final String mimeType = "text/html";
        final String encoding = "UTF-8";

        return new AwWebResourceResponse(
                mimeType, encoding, new ByteArrayInputStream(input.getBytes(encoding)));
    }

    private TestWebServer mWebServer;
    private ShouldInterceptRequestClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private ShouldInterceptRequestClient.ShouldInterceptRequestHelper mShouldInterceptRequestHelper;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mContentsClient = new ShouldInterceptRequestClient();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mShouldInterceptRequestHelper = mContentsClient.getShouldInterceptRequestHelper();

        mWebServer = TestWebServer.start();
    }

    @Override
    protected void tearDown() throws Exception {
        mWebServer.shutdown();
        super.tearDown();
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectUrlParam() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        assertEquals(1, mShouldInterceptRequestHelper.getUrls().size());
        assertEquals(aboutPageUrl,
                mShouldInterceptRequestHelper.getUrls().get(0));

        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
        assertEquals(CommonResources.ABOUT_TITLE, getTitleOnUiThread(mAwContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectIsMainFrameParam() throws Throwable {
        final String subframeUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithIframeUrl = addPageToTestServer(mWebServer, "/page_with_iframe.html",
                CommonResources.makeHtmlPageFrom("",
                    "<iframe src=\"" + subframeUrl + "\"/>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, pageWithIframeUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);
        assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        assertEquals(false,
                mShouldInterceptRequestHelper.getRequestsForUrl(subframeUrl).isMainFrame);
        assertEquals(true,
                mShouldInterceptRequestHelper.getRequestsForUrl(pageWithIframeUrl).isMainFrame);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectMethodParam() throws Throwable {
        final String pageToPostToUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithFormUrl = addPageToTestServer(mWebServer, "/page_with_form.html",
                CommonResources.makeHtmlPageWithSimplePostFormTo(pageToPostToUrl));
        enableJavaScriptOnUiThread(mAwContents);

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, pageWithFormUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        assertEquals("GET",
                mShouldInterceptRequestHelper.getRequestsForUrl(pageWithFormUrl).method);

        callCount = mShouldInterceptRequestHelper.getCallCount();
        JSUtils.clickOnLinkUsingJs(this, mAwContents,
                mContentsClient.getOnEvaluateJavaScriptResultHelper(), "link");
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        assertEquals("POST",
                mShouldInterceptRequestHelper.getRequestsForUrl(pageToPostToUrl).method);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectHasUserGestureParam() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithLinkUrl = addPageToTestServer(mWebServer, "/page_with_link.html",
                CommonResources.makeHtmlPageWithSimpleLinkTo(aboutPageUrl));
        enableJavaScriptOnUiThread(mAwContents);

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, pageWithLinkUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        assertEquals(false,
                mShouldInterceptRequestHelper.getRequestsForUrl(pageWithLinkUrl).hasUserGesture);

        waitForPixelColorAtCenterOfView(mAwContents,
                mTestContainerView, CommonResources.LINK_COLOR);
        callCount = mShouldInterceptRequestHelper.getCallCount();
        AwTestTouchUtils.simulateTouchCenterOfView(mTestContainerView);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        assertEquals(true,
                mShouldInterceptRequestHelper.getRequestsForUrl(aboutPageUrl).hasUserGesture);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectRefererHeader() throws Throwable {
        final String refererHeaderName = "Referer";
        final String imageUrl = mWebServer.setResponseBase64(
                "/" + CommonResources.TEST_IMAGE_FILENAME,
                CommonResources.FAVICON_DATA_BASE64,
                CommonResources.getImagePngHeaders(true));
        final String pageUrl = addPageToTestServer(mWebServer, "/main.html",
                CommonResources.makeHtmlPageFrom(
                        "", "<img src=\'" + CommonResources.TEST_IMAGE_FILENAME + "\'>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, pageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);
        assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        assertEquals(pageUrl,
                mShouldInterceptRequestHelper.getUrls().get(0));
        Map<String, String> headers =
                mShouldInterceptRequestHelper.getRequestsForUrl(pageUrl).requestHeaders;
        assertFalse(headers.containsKey(refererHeaderName));
        assertEquals(imageUrl,
                mShouldInterceptRequestHelper.getUrls().get(1));
        headers = mShouldInterceptRequestHelper.getRequestsForUrl(imageUrl).requestHeaders;
        assertTrue(headers.containsKey(refererHeaderName));
        assertEquals(pageUrl, headers.get(refererHeaderName));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectHeadersParam() throws Throwable {
        final String headerName = "X-Test-Header-Name";
        final String headerValue = "TestHeaderValue";
        final String syncGetUrl = addPageToTestServer(mWebServer, "/intercept_me",
                CommonResources.ABOUT_HTML);
        final String mainPageUrl = addPageToTestServer(mWebServer, "/main",
                CommonResources.makeHtmlPageFrom("",
                "<script>"
                + "  var xhr = new XMLHttpRequest();"
                + "  xhr.open('GET', '" + syncGetUrl + "', false);"
                + "  xhr.setRequestHeader('" + headerName + "', '" + headerValue + "'); "
                + "  xhr.send(null);"
                + "</script>"));
        enableJavaScriptOnUiThread(mAwContents);

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, mainPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);

        Map<String, String> headers =
                mShouldInterceptRequestHelper.getRequestsForUrl(syncGetUrl).requestHeaders;
        assertTrue(headers.containsKey(headerName));
        assertEquals(headerValue, headers.get(headerName));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnLoadResourceCalledWithCorrectUrl() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final ShouldInterceptRequestClient.OnLoadResourceHelper onLoadResourceHelper =
                mContentsClient.getOnLoadResourceHelper();

        int callCount = onLoadResourceHelper.getCallCount();

        loadUrlAsync(mAwContents, aboutPageUrl);

        onLoadResourceHelper.waitForCallback(callCount);
        assertEquals(aboutPageUrl, onLoadResourceHelper.getUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnInvalidData() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", null));
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse(null, null, new ByteArrayInputStream(new byte[0])));
        callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse(null, null, null));
        callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
    }

    private static class EmptyInputStream extends InputStream {
        @Override
        public int available() {
            return 0;
        }

        @Override
        public int read() throws IOException {
            return -1;
        }

        @Override
        public int read(byte b[]) throws IOException {
            return -1;
        }

        @Override
        public int read(byte b[], int off, int len) throws IOException {
            return -1;
        }

        @Override
        public long skip(long n) throws IOException {
            if (n < 0) {
                throw new IOException("skipping negative number of bytes");
            }
            return 0;
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnEmptyStream() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", new EmptyInputStream()));
        int shouldInterceptRequestCallCount = mShouldInterceptRequestHelper.getCallCount();
        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();

        loadUrlAsync(mAwContents, aboutPageUrl);

        mShouldInterceptRequestHelper.waitForCallback(shouldInterceptRequestCallCount);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
    }

    private static class ThrowingInputStream extends EmptyInputStream {
        @Override
        public int available() {
            return 100;
        }

        @Override
        public int read() throws IOException {
            throw new IOException("test exception");
        }

        @Override
        public int read(byte b[]) throws IOException {
            throw new IOException("test exception");
        }

        @Override
        public int read(byte b[], int off, int len) throws IOException {
            throw new IOException("test exception");
        }

        @Override
        public long skip(long n) throws IOException {
            return n;
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnThrowingStream() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", new ThrowingInputStream()));
        int shouldInterceptRequestCallCount = mShouldInterceptRequestHelper.getCallCount();
        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();

        loadUrlAsync(mAwContents, aboutPageUrl);

        mShouldInterceptRequestHelper.waitForCallback(shouldInterceptRequestCallCount);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
    }

    private static class SlowAwWebResourceResponse extends AwWebResourceResponse {
        private CallbackHelper mReadStartedCallbackHelper = new CallbackHelper();
        private CountDownLatch mLatch = new CountDownLatch(1);

        public SlowAwWebResourceResponse(String mimeType, String encoding, InputStream data) {
            super(mimeType, encoding, data);
        }

        @Override
        public InputStream getData() {
            mReadStartedCallbackHelper.notifyCalled();
            try {
                mLatch.await();
            } catch (InterruptedException e) {
                // ignore
            }
            return super.getData();
        }

        public void unblockReads() {
            mLatch.countDown();
        }

        public CallbackHelper getReadStartedCallbackHelper() {
            return mReadStartedCallbackHelper;
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnSlowStream() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final String aboutPageData = makePageWithTitle("some title");
        final String encoding = "UTF-8";
        final SlowAwWebResourceResponse slowAwWebResourceResponse =
                new SlowAwWebResourceResponse("text/html", encoding,
                        new ByteArrayInputStream(aboutPageData.getBytes(encoding)));

        mShouldInterceptRequestHelper.setReturnValue(slowAwWebResourceResponse);
        int callCount = slowAwWebResourceResponse.getReadStartedCallbackHelper().getCallCount();
        loadUrlAsync(mAwContents, aboutPageUrl);
        slowAwWebResourceResponse.getReadStartedCallbackHelper().waitForCallback(callCount);

        // Now the AwContents is "stuck" waiting for the SlowInputStream to finish reading so we
        // delete it to make sure that the dangling 'read' task doesn't cause a crash. Unfortunately
        // this will not always lead to a crash but it should happen often enough for us to notice.

        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().removeAllViews();
            }
        });
        destroyAwContentsOnMainSync(mAwContents);
        pollUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return AwContents.getNativeInstanceCount() == 0;
            }
        });

        slowAwWebResourceResponse.unblockReads();
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpStatusCodeAndText() throws Throwable {
        final String syncGetUrl = mWebServer.getResponseUrl("/intercept_me");
        final String syncGetJs =
                "(function() {"
                + "  var xhr = new XMLHttpRequest();"
                + "  xhr.open('GET', '" + syncGetUrl + "', false);"
                + "  xhr.send(null);"
                + "  console.info('xhr.status = ' + xhr.status);"
                + "  console.info('xhr.statusText = ' + xhr.statusText);"
                + "  return '[' + xhr.status + '][' + xhr.statusText + ']';"
                + "})();";
        enableJavaScriptOnUiThread(mAwContents);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", null));
        assertEquals("\"[404][Not Found]\"",
                executeJavaScriptAndWaitForResult(mAwContents, mContentsClient, syncGetJs));

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", new EmptyInputStream()));
        assertEquals("\"[200][OK]\"",
                executeJavaScriptAndWaitForResult(mAwContents, mContentsClient, syncGetJs));

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", new EmptyInputStream(),
                        TEAPOT_STATUS_CODE, TEAPOT_RESPONSE_PHRASE, new HashMap<String, String>()));
        assertEquals("\"[" + TEAPOT_STATUS_CODE + "][" + TEAPOT_RESPONSE_PHRASE + "]\"",
                executeJavaScriptAndWaitForResult(mAwContents, mContentsClient, syncGetJs));
    }

    private String getHeaderValue(AwContents awContents, TestAwContentsClient contentsClient,
            String url, String headerName) throws Exception {
        final String syncGetJs =
                "(function() {"
                + "  var xhr = new XMLHttpRequest();"
                + "  xhr.open('GET', '" + url + "', false);"
                + "  xhr.send(null);"
                + "  console.info(xhr.getAllResponseHeaders());"
                + "  return xhr.getResponseHeader('" + headerName + "');"
                + "})();";
        String header = executeJavaScriptAndWaitForResult(awContents, contentsClient, syncGetJs);

        if (header.equals("null")) return null;
        // JSON stringification applied by executeJavaScriptAndWaitForResult adds quotes
        // around returned strings.
        assertTrue(header.length() > 2);
        assertEquals('"', header.charAt(0));
        assertEquals('"', header.charAt(header.length() - 1));
        return header.substring(1, header.length() - 1);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpResponseClientViaHeader() throws Throwable {
        final String clientResponseHeaderName = "Client-Via";
        final String clientResponseHeaderValue = "shouldInterceptRequest";
        final String syncGetUrl = mWebServer.getResponseUrl("/intercept_me");
        enableJavaScriptOnUiThread(mAwContents);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        // The response header is set regardless of whether the embedder has provided a
        // valid resource stream.
        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", null));
        assertEquals(clientResponseHeaderValue,
                getHeaderValue(mAwContents, mContentsClient, syncGetUrl, clientResponseHeaderName));
        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", new EmptyInputStream()));
        assertEquals(clientResponseHeaderValue,
                getHeaderValue(mAwContents, mContentsClient, syncGetUrl, clientResponseHeaderName));

    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpResponseHeader() throws Throwable {
        final String clientResponseHeaderName = "X-Test-Header-Name";
        final String clientResponseHeaderValue = "TestHeaderValue";
        final String syncGetUrl = mWebServer.getResponseUrl("/intercept_me");
        final Map<String, String> headers = new HashMap<String, String>();
        headers.put(clientResponseHeaderName, clientResponseHeaderValue);
        enableJavaScriptOnUiThread(mAwContents);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", null, 0, null, headers));
        assertEquals(clientResponseHeaderValue,
                getHeaderValue(mAwContents, mContentsClient, syncGetUrl, clientResponseHeaderName));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullHttpResponseHeaders() throws Throwable {
        final String syncGetUrl = mWebServer.getResponseUrl("/intercept_me");
        enableJavaScriptOnUiThread(mAwContents);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", null, 0, null, null));
        assertEquals(null, getHeaderValue(mAwContents, mContentsClient, syncGetUrl, "Some-Header"));
    }

    private String makePageWithTitle(String title) {
        return CommonResources.makeHtmlPageFrom("<title>" + title + "</title>",
                "<div> The title is: " + title + " </div>");
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCanInterceptMainFrame() throws Throwable {
        final String expectedTitle = "testShouldInterceptRequestCanInterceptMainFrame";
        final String expectedPage = makePageWithTitle(expectedTitle);

        mShouldInterceptRequestHelper.setReturnValue(
                stringToAwWebResourceResponse(expectedPage));

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        assertEquals(expectedTitle, getTitleOnUiThread(mAwContents));
        assertEquals(0, mWebServer.getRequestCount("/" + CommonResources.ABOUT_FILENAME));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotChangeReportedUrl() throws Throwable {
        mShouldInterceptRequestHelper.setReturnValue(
                stringToAwWebResourceResponse(makePageWithTitle("some title")));

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        assertEquals(aboutPageUrl, mContentsClient.getOnPageFinishedHelper().getUrl());
        assertEquals(aboutPageUrl, mContentsClient.getOnPageStartedHelper().getUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullInputStreamCausesErrorForMainFrame() throws Throwable {
        final OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", null));

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final int callCount = onReceivedErrorHelper.getCallCount();
        loadUrlAsync(mAwContents, aboutPageUrl);
        onReceivedErrorHelper.waitForCallback(callCount);
        assertEquals(0, mWebServer.getRequestCount("/" + CommonResources.ABOUT_FILENAME));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForImage() throws Throwable {
        final String imagePath = "/" + CommonResources.FAVICON_FILENAME;
        mWebServer.setResponseBase64(imagePath,
                CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(true));
        final String pageWithImage =
                addPageToTestServer(mWebServer, "/page_with_image.html",
                        CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithImage);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);

        assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        assertTrue(mShouldInterceptRequestHelper.getUrls().get(1).endsWith(
                CommonResources.FAVICON_FILENAME));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnReceivedErrorCallback() throws Throwable {
        mShouldInterceptRequestHelper.setReturnValue(new AwWebResourceResponse(null, null, null));
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        int onReceivedErrorHelperCallCount = onReceivedErrorHelper.getCallCount();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), "foo://bar");
        onReceivedErrorHelper.waitForCallback(onReceivedErrorHelperCallCount, 1);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoOnReceivedErrorCallback() throws Throwable {
        final String imagePath = "/" + CommonResources.FAVICON_FILENAME;
        final String imageUrl = mWebServer.setResponseBase64(imagePath,
                CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(true));
        final String pageWithImage =
                addPageToTestServer(mWebServer, "/page_with_image.html",
                        CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME));
        mShouldInterceptRequestHelper.setReturnValueForUrl(
                imageUrl, new AwWebResourceResponse(null, null, null));
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        int onReceivedErrorHelperCallCount = onReceivedErrorHelper.getCallCount();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithImage);
        assertEquals(onReceivedErrorHelperCallCount, onReceivedErrorHelper.getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForIframe() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithIframeUrl = addPageToTestServer(mWebServer, "/page_with_iframe.html",
                CommonResources.makeHtmlPageFrom("",
                    "<iframe src=\"" + aboutPageUrl + "\"/>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithIframeUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);
        assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        assertEquals(aboutPageUrl, mShouldInterceptRequestHelper.getUrls().get(1));
    }

    private void calledForUrlTemplate(final String url) throws Exception {
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        int onPageStartedCallCount = mContentsClient.getOnPageStartedHelper().getCallCount();
        loadUrlAsync(mAwContents, url);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        assertEquals(url, mShouldInterceptRequestHelper.getUrls().get(0));

        mContentsClient.getOnPageStartedHelper().waitForCallback(onPageStartedCallCount);
        assertEquals(onPageStartedCallCount + 1,
                mContentsClient.getOnPageStartedHelper().getCallCount());
    }

    private void notCalledForUrlTemplate(final String url) throws Exception {
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        // The intercepting must happen before onPageFinished. Since the IPC messages from the
        // renderer should be delivered in order waiting for onPageFinished is sufficient to
        // 'flush' any pending interception messages.
        assertEquals(callCount, mShouldInterceptRequestHelper.getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForUnsupportedSchemes() throws Throwable {
        calledForUrlTemplate("foobar://resource/1");
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForNonexistentFiles() throws Throwable {
        calledForUrlTemplate("file:///somewhere/something");
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForExistingFiles() throws Throwable {
        final String tmpDir = getInstrumentation().getTargetContext().getCacheDir().getPath();
        final String fileName = tmpDir + "/testfile.html";
        final String title = "existing file title";
        TestFileUtil.deleteFile(fileName);  // Remove leftover file if any.
        TestFileUtil.createNewHtmlFile(fileName, title, "");
        final String existingFileUrl = "file://" + fileName;

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        loadUrlAsync(mAwContents, existingFileUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        assertEquals(existingFileUrl, mShouldInterceptRequestHelper.getUrls().get(0));

        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
        assertEquals(title, getTitleOnUiThread(mAwContents));
        assertEquals(onPageFinishedCallCount + 1,
                mContentsClient.getOnPageFinishedHelper().getCallCount());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotCalledForExistingResource() throws Throwable {
        notCalledForUrlTemplate("file:///android_res/raw/resource_file.html");
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForNonexistentResource() throws Throwable {
        calledForUrlTemplate("file:///android_res/raw/no_file.html");
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotCalledForExistingAsset() throws Throwable {
        notCalledForUrlTemplate("file:///android_asset/asset_file.html");
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForNonexistentAsset() throws Throwable {
        calledForUrlTemplate("file:///android_res/raw/no_file.html");
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotCalledForExistingContentUrl() throws Throwable {
        final String contentResourceName = "target";
        final String existingContentUrl = TestContentProvider.createContentUrl(contentResourceName);

        notCalledForUrlTemplate(existingContentUrl);

        int contentRequestCount = TestContentProvider.getResourceRequestCount(
                getInstrumentation().getTargetContext(), contentResourceName);
        assertEquals(1, contentRequestCount);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForNonexistentContentUrl() throws Throwable {
        calledForUrlTemplate("content://org.chromium.webview.NoSuchProvider/foo");
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testBaseUrlOfLoadDataSentInRefererHeader() throws Throwable {
        final String imageFile = "a.jpg";
        final String pageHtml = "<img src='" + imageFile + "'>";
        final String baseUrl = "http://localhost:666/";
        final String imageUrl = baseUrl + imageFile;
        final String refererHeaderName = "Referer";
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadDataWithBaseUrlAsync(mAwContents, pageHtml, "text/html", false, baseUrl, null);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 1);
        assertEquals(1, mShouldInterceptRequestHelper.getUrls().size());
        if (!mShouldInterceptRequestHelper.getUrls().get(0).equals(imageUrl)) {
            // With PlzNavigate, data URLs are intercepted so wait for the next request.
            // See https://codereview.chromium.org/2235303002/.
            callCount = mShouldInterceptRequestHelper.getCallCount();
            mShouldInterceptRequestHelper.waitForCallback(callCount, 1);
            assertEquals(imageUrl, mShouldInterceptRequestHelper.getUrls().get(1));
        }

        Map<String, String> headers =
                mShouldInterceptRequestHelper.getRequestsForUrl(imageUrl).requestHeaders;
        assertTrue(headers.containsKey(refererHeaderName));
        assertEquals(baseUrl, headers.get(refererHeaderName));
    }

    private static class DeadlockingAwContentsClient extends TestAwContentsClient {
        public DeadlockingAwContentsClient(CountDownLatch ready, CountDownLatch wait) {
            mReady = ready;
            mWait = wait;
        }

        @Override
        public AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request) {
            mReady.countDown();
            try {
                mWait.await();
            } catch (InterruptedException e) {
                // ignore
            }
            return null;
        }

        private CountDownLatch mReady;
        private CountDownLatch mWait;
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDeadlock() throws Throwable {
        // The client will lock shouldInterceptRequest to wait for the UI thread.
        // On the UI thread, we will try engaging the IO thread by executing
        // an action that causes IPC message sending. If the client callback
        // is executed on the IO thread, this will cause a deadlock.
        CountDownLatch waitForShouldInterceptRequest = new CountDownLatch(1);
        CountDownLatch signalAfterSendingIpc = new CountDownLatch(1);
        DeadlockingAwContentsClient client = new DeadlockingAwContentsClient(
                waitForShouldInterceptRequest, signalAfterSendingIpc);
        mTestContainerView = createAwTestContainerViewOnMainSync(client);
        mAwContents = mTestContainerView.getAwContents();
        loadUrlAsync(mAwContents, "http://www.example.com");
        waitForShouldInterceptRequest.await();
        // The following call will try to send an IPC and wait for a reply from renderer.
        // We do not check the actual result, because it can be bogus. The important
        // thing is that the call does not cause a deadlock.
        executeJavaScriptAndWaitForResult(mAwContents, client, "1+1");
        signalAfterSendingIpc.countDown();
    }

    // Webview must be able to intercept requests with the content-id scheme.
    // See https://crbug.com/739658.
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContentIdImage() throws Throwable {
        final String imageContentIdUrl = "cid://intercept-me";
        final String pageUrl = addPageToTestServer(mWebServer, "/main.html",
                CommonResources.makeHtmlPageFrom("", "<img src=\'" + imageContentIdUrl + "\'>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, pageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);
        assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        assertEquals(imageContentIdUrl, mShouldInterceptRequestHelper.getUrls().get(1));
    }

    // Webview must be able to intercept requests with the content-id scheme.
    // See https://crbug.com/739658.
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContentIdIframe() throws Throwable {
        final String iframeContentIdUrl = "cid://intercept-me";
        final String pageUrl = addPageToTestServer(mWebServer, "/main.html",
                CommonResources.makeHtmlPageFrom(
                        "", "<iframe src=\'" + iframeContentIdUrl + "\'></iframe>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, pageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);
        assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        assertEquals(iframeContentIdUrl, mShouldInterceptRequestHelper.getUrls().get(1));
    }
}
