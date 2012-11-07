// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;
import android.util.Pair;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.InterceptedRequestData;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.net.test.util.TestWebServer;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/**
 * Tests for the WebViewClient.shouldInterceptRequest() method.
 */
public class AwContentsClientShouldInterceptRequestTest extends AndroidWebViewTestBase {

    private static class TestAwContentsClient
            extends org.chromium.android_webview.test.TestAwContentsClient {

        public static class ShouldInterceptRequestHelper extends CallbackHelper {
            private List<String> mShouldInterceptRequestUrls = new ArrayList<String>();
            // This is read from the IO thread, so needs to be marked volatile.
            private volatile InterceptedRequestData mShouldInterceptRequestReturnValue = null;
            void setReturnValue(InterceptedRequestData value) {
                mShouldInterceptRequestReturnValue = value;
            }
            public List<String> getUrls() {
                assert getCallCount() > 0;
                return mShouldInterceptRequestUrls;
            }
            public InterceptedRequestData getReturnValue() {
                return mShouldInterceptRequestReturnValue;
            }
            public void notifyCalled(String url) {
                mShouldInterceptRequestUrls.add(url);
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
        public InterceptedRequestData shouldInterceptRequest(String url) {
            InterceptedRequestData returnValue = mShouldInterceptRequestHelper.getReturnValue();
            mShouldInterceptRequestHelper.notifyCalled(url);
            return returnValue;
        }

        @Override
        public void onLoadResource(String url) {
            super.onLoadResource(url);
            mOnLoadResourceHelper.notifyCalled(url);
        }

        private ShouldInterceptRequestHelper mShouldInterceptRequestHelper;
        private OnLoadResourceHelper mOnLoadResourceHelper;

        public TestAwContentsClient() {
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

    private InterceptedRequestData stringToInterceptedRequestData(String input) throws Throwable {
        final String mimeType = "text/html";
        final String encoding = "UTF-8";

        return new InterceptedRequestData(
                mimeType, encoding, new ByteArrayInputStream(input.getBytes(encoding)));
    }

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

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectUrl() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
            contentsClient.getShouldInterceptRequestHelper();

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        int callCount = shouldInterceptRequestHelper.getCallCount();
        int onPageFinishedCallCount = contentsClient.getOnPageFinishedHelper().getCallCount();

        loadUrlAsync(awContents, aboutPageUrl);

        shouldInterceptRequestHelper.waitForCallback(callCount);
        assertEquals(1, shouldInterceptRequestHelper.getUrls().size());
        assertEquals(aboutPageUrl,
                shouldInterceptRequestHelper.getUrls().get(0));

        contentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
        assertEquals(CommonResources.ABOUT_TITLE, getTitleOnUiThread(awContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnLoadResourceCalledWithCorrectUrl() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.OnLoadResourceHelper onLoadResourceHelper =
            contentsClient.getOnLoadResourceHelper();

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        int callCount = onLoadResourceHelper.getCallCount();

        loadUrlAsync(awContents, aboutPageUrl);

        onLoadResourceHelper.waitForCallback(callCount);
        assertEquals(aboutPageUrl, onLoadResourceHelper.getUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnInvalidData() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
            contentsClient.getShouldInterceptRequestHelper();

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        shouldInterceptRequestHelper.setReturnValue(
                new InterceptedRequestData("text/html", "UTF-8", null));
        int callCount = shouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(awContents, aboutPageUrl);
        shouldInterceptRequestHelper.waitForCallback(callCount);

        shouldInterceptRequestHelper.setReturnValue(
                new InterceptedRequestData(null, null, new ByteArrayInputStream(new byte[0])));
        callCount = shouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(awContents, aboutPageUrl);
        shouldInterceptRequestHelper.waitForCallback(callCount);

        shouldInterceptRequestHelper.setReturnValue(
                new InterceptedRequestData(null, null, null));
        callCount = shouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(awContents, aboutPageUrl);
        shouldInterceptRequestHelper.waitForCallback(callCount);
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
            if (n < 0)
                throw new IOException("skipping negative number of bytes");
            return 0;
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnEmptyStream() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
            contentsClient.getShouldInterceptRequestHelper();

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        shouldInterceptRequestHelper.setReturnValue(
                new InterceptedRequestData("text/html", "UTF-8", new EmptyInputStream()));
        int shouldInterceptRequestCallCount = shouldInterceptRequestHelper.getCallCount();
        int onPageFinishedCallCount = contentsClient.getOnPageFinishedHelper().getCallCount();

        loadUrlAsync(awContents, aboutPageUrl);

        shouldInterceptRequestHelper.waitForCallback(shouldInterceptRequestCallCount);
        contentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
    }


    private String makePageWithTitle(String title) {
        return CommonResources.makeHtmlPageFrom("<title>" + title + "</title>",
                "<div> The title is: " + title + " </div>");
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCanInterceptMainFrame() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
            contentsClient.getShouldInterceptRequestHelper();

        final String expectedTitle = "testShouldInterceptRequestCanInterceptMainFrame";
        final String expectedPage = makePageWithTitle(expectedTitle);

        shouldInterceptRequestHelper.setReturnValue(
                stringToInterceptedRequestData(expectedPage));

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        assertEquals(expectedTitle, getTitleOnUiThread(awContents));
        assertEquals(0, mWebServer.getRequestCount("/" + CommonResources.ABOUT_FILENAME));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotChangeReportedUrl() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
            contentsClient.getShouldInterceptRequestHelper();

        shouldInterceptRequestHelper.setReturnValue(
                stringToInterceptedRequestData(makePageWithTitle("some title")));

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        assertEquals(aboutPageUrl, contentsClient.getOnPageFinishedHelper().getUrl());
        assertEquals(aboutPageUrl, contentsClient.getOnPageStartedHelper().getUrl());
    }


    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForImage() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
            contentsClient.getShouldInterceptRequestHelper();

        final String imagePath = "/" + CommonResources.FAVICON_FILENAME;
        mWebServer.setResponseBase64(imagePath,
                CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(true));
        final String pageWithImage =
            addPageToTestServer(mWebServer, "/page_with_image.html",
                    CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME));

        int callCount = shouldInterceptRequestHelper.getCallCount();
        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), pageWithImage);
        shouldInterceptRequestHelper.waitForCallback(callCount, 2);

        assertEquals(2, shouldInterceptRequestHelper.getUrls().size());
        assertTrue(shouldInterceptRequestHelper.getUrls().get(1).endsWith(
                CommonResources.FAVICON_FILENAME));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForIframe() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
            contentsClient.getShouldInterceptRequestHelper();

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithIframe = addPageToTestServer(mWebServer, "/page_with_iframe.html",
                CommonResources.makeHtmlPageFrom("",
                    "<iframe src=\"" + aboutPageUrl + "\"/>"));

        int callCount = shouldInterceptRequestHelper.getCallCount();
        loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), pageWithIframe);
        shouldInterceptRequestHelper.waitForCallback(callCount, 2);
        assertEquals(2, shouldInterceptRequestHelper.getUrls().size());
        assertEquals(aboutPageUrl, shouldInterceptRequestHelper.getUrls().get(1));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForUnsupportedSchemes() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
            createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
            contentsClient.getShouldInterceptRequestHelper();

        final String unhandledSchemeUrl = "foobar://resource/1";
        int callCount = shouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(awContents, unhandledSchemeUrl);
        shouldInterceptRequestHelper.waitForCallback(callCount);
        assertEquals(unhandledSchemeUrl,
                shouldInterceptRequestHelper.getUrls().get(0));
    }
}
