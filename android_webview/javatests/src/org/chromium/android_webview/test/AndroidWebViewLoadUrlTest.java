// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.apache.http.Header;
import org.apache.http.HttpRequest;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.android_webview.test.util.TestWebServer;

import java.util.concurrent.TimeUnit;
import java.util.HashMap;
import java.util.Map;

/**
 * Test suite for loadUrl().
 */
public class AndroidWebViewLoadUrlTest extends AndroidWebViewTestBase {
    @SmallTest
    @Feature({"Android-WebView"})
    public void testDataUrl() throws Throwable {
        final String expectedTitle = "dataUrlTest";
        final String data =
            "<html><head><title>" + expectedTitle + "</title></head><body>foo</body></html>";

        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ContentViewCore contentViewCore =
            createAwTestContainerViewOnMainSync(contentsClient).getContentViewCore();
        loadDataSync(contentViewCore, contentsClient.getOnPageFinishedHelper(), data,
                     "text/html", false);
        assertEquals(expectedTitle, getTitleOnUiThread(contentViewCore));
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testDataUrlBase64() throws Throwable {
        final String expectedTitle = "dataUrlTestBase64";
        final String data = "PGh0bWw+PGhlYWQ+PHRpdGxlPmRhdGFVcmxUZXN0QmFzZTY0PC90aXRsZT48" +
                            "L2hlYWQ+PC9odG1sPg==";

        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ContentViewCore contentViewCore =
            createAwTestContainerViewOnMainSync(contentsClient).getContentViewCore();
        loadDataSync(contentViewCore, contentsClient.getOnPageFinishedHelper(), data,
                     "text/html", true);
        assertEquals(expectedTitle, getTitleOnUiThread(contentViewCore));
    }

    /**
     * Loads url on the UI thread and blocks until onPageFinished is called.
     */
    protected void loadUrlWithExtraHeadersSync(
            final ContentViewCore contentViewCore,
            CallbackHelper onPageFinishedHelper,
            final String url,
            final Map<String, String> extraHeaders) throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                LoadUrlParams params = new LoadUrlParams(url);
                params.setExtraHeaders(extraHeaders);
                contentViewCore.loadUrl(params);
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testLoadUrlWithExtraHeaders() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ContentViewCore contentViewCore =
            createAwTestContainerViewOnMainSync(contentsClient).getContentViewCore();

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String path = "/load_url_with_extra_headers_test.html";
            final String url = webServer.setResponse(path, "<html><body>foo</body></html>", null);

            String[] headerNames = {"X-ExtraHeaders1", "x-extraHeaders2"};
            String[] headerValues = {"extra-header-data1", "EXTRA-HEADER-DATA2"};
            Map<String, String> extraHeaders = new HashMap<String, String>();
            for (int i = 0; i < headerNames.length; ++i)
              extraHeaders.put(headerNames[i], headerValues[i]);

            loadUrlWithExtraHeadersSync(contentViewCore,
                                        contentsClient.getOnPageFinishedHelper(),
                                        url,
                                        extraHeaders);

            HttpRequest request = webServer.getLastRequest(path);
            for (int i = 0; i < headerNames.length; ++i) {
              Header[] matchingHeaders = request.getHeaders(headerNames[i]);
              assertEquals(1, matchingHeaders.length);

              Header header = matchingHeaders[0];
              assertEquals(headerNames[i].toLowerCase(), header.getName());
              assertEquals(headerValues[i], header.getValue());
            }
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }
}
