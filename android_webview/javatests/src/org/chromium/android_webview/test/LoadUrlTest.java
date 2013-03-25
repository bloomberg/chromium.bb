// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.apache.http.Header;
import org.apache.http.HttpRequest;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.TimeUnit;
import java.util.HashMap;
import java.util.Map;

/**
 * Test suite for loadUrl().
 */
public class LoadUrlTest extends AwTestBase {
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDataUrl() throws Throwable {
        final String expectedTitle = "dataUrlTest";
        final String data =
            "<html><head><title>" + expectedTitle + "</title></head><body>foo</body></html>";

        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(), data,
                     "text/html", false);
        assertEquals(expectedTitle, getTitleOnUiThread(awContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDataUrlBase64() throws Throwable {
        final String expectedTitle = "dataUrlTestBase64";
        final String data = "PGh0bWw+PGhlYWQ+PHRpdGxlPmRhdGFVcmxUZXN0QmFzZTY0PC90aXRsZT48" +
                            "L2hlYWQ+PC9odG1sPg==";

        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(), data,
                     "text/html", true);
        assertEquals(expectedTitle, getTitleOnUiThread(awContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDataUrlCharset() throws Throwable {
        // Note that the '£' is the important character in the following
        // string as it's not in the US_ASCII character set.
        final String expectedTitle = "You win £100!";
        final String data =
            "<html><head><title>" + expectedTitle + "</title></head><body>foo</body></html>";
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        loadDataSyncWithCharset(awContents, contentsClient.getOnPageFinishedHelper(), data,
                     "text/html", false, "UTF-8");
        assertEquals(expectedTitle, getTitleOnUiThread(awContents));
    }

    /**
     * Loads url on the UI thread and blocks until onPageFinished is called.
     */
    protected void loadUrlWithExtraHeadersSync(
            final AwContents awContents,
            CallbackHelper onPageFinishedHelper,
            final String url,
            final Map<String, String> extraHeaders) throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                LoadUrlParams params = new LoadUrlParams(url);
                params.setExtraHeaders(extraHeaders);
                awContents.loadUrl(params);
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadUrlWithExtraHeaders() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();

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

            loadUrlWithExtraHeadersSync(awContents,
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
