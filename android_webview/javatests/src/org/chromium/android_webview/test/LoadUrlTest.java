// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;

import org.apache.http.Header;
import org.apache.http.HttpRequest;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.HistoryUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.TimeUnit;

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
        // Note that the \u00a3 (pound sterling) is the important character in the following
        // string as it's not in the US_ASCII character set.
        final String expectedTitle = "You win \u00a3100!";
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
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS,
                                             TimeUnit.MILLISECONDS);
    }

    private static List<Pair<String, String>> createHeadersList(String[] namesAndValues) {
        List<Pair<String, String>> result = new ArrayList<Pair<String, String>>();
        for (int i = 0; i < namesAndValues.length; i += 2)
            result.add(Pair.create(namesAndValues[i], namesAndValues[i + 1]));
        return result;
    }

    private static Map<String, String> createHeadersMap(String[] namesAndValues) {
        Map<String, String> result = new HashMap<String, String>();
        for (int i = 0; i < namesAndValues.length; i += 2)
            result.put(namesAndValues[i], namesAndValues[i + 1]);
        return result;
    }

    private void validateRequestHeaders(String[] refNamesAndValues,
                                        HttpRequest request) {
        for (int i = 0; i < refNamesAndValues.length; i += 2) {
            Header[] matchingHeaders = request.getHeaders(refNamesAndValues[i]);
            assertEquals(1, matchingHeaders.length);

            Header header = matchingHeaders[0];
            assertEquals(refNamesAndValues[i].toLowerCase(Locale.ENGLISH), header.getName());
            assertEquals(refNamesAndValues[i + 1], header.getValue());
        }
    }

    private void validateNoRequestHeaders(String[] refNamesAndValues,
                                          HttpRequest request) {
        for (int i = 0; i < refNamesAndValues.length; i += 2) {
            Header[] matchingHeaders = request.getHeaders(refNamesAndValues[i]);
            assertEquals(0, matchingHeaders.length);
        }
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
            final String imagePath = "/" + CommonResources.FAVICON_FILENAME;
            webServer.setResponseBase64(imagePath,
                    CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(true));
            final String path = "/load_url_with_extra_headers_test.html";
            final String url = webServer.setResponse(
                    path,
                    CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME),
                    null);
            String[] extraHeaders = {
                "X-ExtraHeaders1", "extra-header-data1",
                "x-extraHeaders2", "EXTRA-HEADER-DATA2"
            };

            loadUrlWithExtraHeadersSync(awContents,
                                        contentsClient.getOnPageFinishedHelper(),
                                        url,
                                        createHeadersMap(extraHeaders));
            validateRequestHeaders(extraHeaders, webServer.getLastRequest(path));
            // Verify that extra headers are only passed for the main resource.
            validateNoRequestHeaders(extraHeaders, webServer.getLastRequest(imagePath));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoOverridingOfExistingHeaders() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String path = "/no_overriding_of_existing_headers_test.html";
            final String url = webServer.setResponse(
                    path,
                    "<html><body>foo</body></html>",
                    null);
            String[] extraHeaders = {
                "user-agent", "007"
            };

            loadUrlWithExtraHeadersSync(awContents,
                                        contentsClient.getOnPageFinishedHelper(),
                                        url,
                                        createHeadersMap(extraHeaders));
            Header[] matchingHeaders = webServer.getLastRequest(path).getHeaders(extraHeaders[0]);
            assertEquals(1, matchingHeaders.length);
            Header header = matchingHeaders[0];
            assertEquals(extraHeaders[0].toLowerCase(Locale.ENGLISH),
                    header.getName().toLowerCase(Locale.ENGLISH));
            // Just check that the value is there, and it's not the one we provided.
            assertTrue(header.getValue().length() > 0);
            assertFalse(extraHeaders[1].equals(header.getValue()));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testReloadWithExtraHeaders() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String path = "/reload_with_extra_headers_test.html";
            final String url = webServer.setResponse(path,
                    "<html><body>foo</body></html>",
                    createHeadersList(new String[] { "cache-control", "no-cache, no-store" }));
            String[] extraHeaders = {
                "X-ExtraHeaders1", "extra-header-data1",
                "x-extraHeaders2", "EXTRA-HEADER-DATA2"
            };

            loadUrlWithExtraHeadersSync(awContents,
                                        contentsClient.getOnPageFinishedHelper(),
                                        url,
                                        createHeadersMap(extraHeaders));
            validateRequestHeaders(extraHeaders, webServer.getLastRequest(path));

            reloadSync(awContents, contentsClient.getOnPageFinishedHelper());
            assertEquals(2, webServer.getRequestCount(path));
            validateRequestHeaders(extraHeaders, webServer.getLastRequest(path));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRedirectAndReloadWithExtraHeaders() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String path = "/redirect_and_reload_with_extra_headers_test.html";
            final String url = webServer.setResponse(path,
                    "<html><body>foo</body></html>",
                    createHeadersList(new String[] { "cache-control", "no-cache, no-store" }));
            final String redirectedPath = "/redirected.html";
            final String redirectedUrl = webServer.setRedirect(redirectedPath, path);
            String[] extraHeaders = {
                "X-ExtraHeaders1", "extra-header-data1",
                "x-extraHeaders2", "EXTRA-HEADER-DATA2"
            };

            loadUrlWithExtraHeadersSync(awContents,
                                        contentsClient.getOnPageFinishedHelper(),
                                        redirectedUrl,
                                        createHeadersMap(extraHeaders));
            validateRequestHeaders(extraHeaders, webServer.getLastRequest(path));
            validateRequestHeaders(extraHeaders, webServer.getLastRequest(redirectedPath));

            // WebView will only reload the main page.
            reloadSync(awContents, contentsClient.getOnPageFinishedHelper());
            assertEquals(2, webServer.getRequestCount(path));
            // No extra headers. This is consistent with legacy behavior.
            validateNoRequestHeaders(extraHeaders, webServer.getLastRequest(path));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRendererNavigationAndGoBackWithExtraHeaders() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings settings = getAwSettingsOnUiThread(awContents);
        settings.setJavaScriptEnabled(true);

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String nextPath = "/next.html";
            final String nextUrl = webServer.setResponse(nextPath,
                    "<html><body>Next!</body></html>",
                    null);
            final String path = "/renderer_nav_and_go_back_with_extra_headers_test.html";
            final String url = webServer.setResponse(path,
                    "<html><body><a id=\"next\" href=\"next.html\">Next!</a></body></html>",
                    createHeadersList(new String[] { "cache-control", "no-cache, no-store" }));
            String[] extraHeaders = {
                "X-ExtraHeaders1", "extra-header-data1",
                "x-extraHeaders2", "EXTRA-HEADER-DATA2"
            };

            loadUrlWithExtraHeadersSync(awContents,
                                        contentsClient.getOnPageFinishedHelper(),
                                        url,
                                        createHeadersMap(extraHeaders));
            validateRequestHeaders(extraHeaders, webServer.getLastRequest(path));

            int currentCallCount = contentsClient.getOnPageFinishedHelper().getCallCount();
            JSUtils.clickOnLinkUsingJs(this,
                                       awContents,
                                       contentsClient.getOnEvaluateJavaScriptResultHelper(),
                                       "next");
            contentsClient.getOnPageFinishedHelper().waitForCallback(
                    currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            // No extra headers for the page navigated via clicking.
            validateNoRequestHeaders(extraHeaders, webServer.getLastRequest(nextPath));

            HistoryUtils.goBackSync(getInstrumentation(),
                                    awContents.getContentViewCore(),
                                    contentsClient.getOnPageFinishedHelper());
            assertEquals(2, webServer.getRequestCount(path));
            validateRequestHeaders(extraHeaders, webServer.getLastRequest(path));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }
}
