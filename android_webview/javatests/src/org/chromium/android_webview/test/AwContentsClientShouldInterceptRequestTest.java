// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;
import android.util.Pair;

import org.chromium.android_webview.AndroidProtocolHandler;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.InterceptedRequestData;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnReceivedErrorHelper;
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
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestAwContentsClient.ShouldInterceptRequestHelper mShouldInterceptRequestHelper;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mContentsClient = new TestAwContentsClient();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mShouldInterceptRequestHelper = mContentsClient.getShouldInterceptRequestHelper();

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
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();

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
    public void testOnLoadResourceCalledWithCorrectUrl() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final TestAwContentsClient.OnLoadResourceHelper onLoadResourceHelper =
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
                new InterceptedRequestData("text/html", "UTF-8", null));
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);

        mShouldInterceptRequestHelper.setReturnValue(
                new InterceptedRequestData(null, null, new ByteArrayInputStream(new byte[0])));
        callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);

        mShouldInterceptRequestHelper.setReturnValue(
                new InterceptedRequestData(null, null, null));
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
            if (n < 0)
                throw new IOException("skipping negative number of bytes");
            return 0;
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnEmptyStream() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mShouldInterceptRequestHelper.setReturnValue(
                new InterceptedRequestData("text/html", "UTF-8", new EmptyInputStream()));
        int shouldInterceptRequestCallCount = mShouldInterceptRequestHelper.getCallCount();
        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();

        loadUrlAsync(mAwContents, aboutPageUrl);

        mShouldInterceptRequestHelper.waitForCallback(shouldInterceptRequestCallCount);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpStatusField() throws Throwable {
        final String syncGetUrl = mWebServer.getResponseUrl("/intercept_me");
        final String syncGetJs =
            "(function() {" +
            "  var xhr = new XMLHttpRequest();" +
            "  xhr.open('GET', '" + syncGetUrl + "', false);" +
            "  xhr.send(null);" +
            "  console.info('xhr.status = ' + xhr.status);" +
            "  return xhr.status;" +
            "})();";
        enableJavaScriptOnUiThread(mAwContents);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        mShouldInterceptRequestHelper.setReturnValue(
                new InterceptedRequestData("text/html", "UTF-8", null));
        assertEquals("404",
                executeJavaScriptAndWaitForResult(mAwContents, mContentsClient, syncGetJs));

        mShouldInterceptRequestHelper.setReturnValue(
                new InterceptedRequestData("text/html", "UTF-8", new EmptyInputStream()));
        assertEquals("200",
                executeJavaScriptAndWaitForResult(mAwContents, mContentsClient, syncGetJs));
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
                stringToInterceptedRequestData(expectedPage));

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        assertEquals(expectedTitle, getTitleOnUiThread(mAwContents));
        assertEquals(0, mWebServer.getRequestCount("/" + CommonResources.ABOUT_FILENAME));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotChangeReportedUrl() throws Throwable {
        mShouldInterceptRequestHelper.setReturnValue(
                stringToInterceptedRequestData(makePageWithTitle("some title")));

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
                new InterceptedRequestData("text/html", "UTF-8", null));

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
    public void testCalledForIframe() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithIframe = addPageToTestServer(mWebServer, "/page_with_iframe.html",
                CommonResources.makeHtmlPageFrom("",
                    "<iframe src=\"" + aboutPageUrl + "\"/>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithIframe);
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
}
