// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.TestWebServer;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentSettings;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.browser.test.util.HistoryUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;

import java.util.concurrent.TimeUnit;

public class LoadDataWithBaseUrlTest extends AndroidWebViewTestBase {

    protected static int WAIT_TIMEOUT_SECONDS = 15;

    private TestAwContentsClient mContentsClient;
    private ContentViewCore mContentViewCore;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mContentViewCore =
                createAwTestContainerViewOnMainSync(mContentsClient).getContentViewCore();
    }

    protected void loadDataWithBaseUrlSync(
        final String data, final String mimeType, final boolean isBase64Encoded,
        final String baseUrl, final String historyUrl) throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataWithBaseUrlAsync(data, mimeType, isBase64Encoded, baseUrl, historyUrl);
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
    }

    protected void loadDataWithBaseUrlAsync(
        final String data, final String mimeType, final boolean isBase64Encoded,
        final String baseUrl, final String historyUrl) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                mContentViewCore.loadUrl(LoadUrlParams.createLoadDataParamsWithBaseUrl(
                        data, mimeType, isBase64Encoded, baseUrl, historyUrl));
            }
        });
    }

    private static final String SCRIPT_FILE = "/script.js";
    private static final String SCRIPT_LOADED = "Loaded";
    private static final String SCRIPT_NOT_LOADED = "Not loaded";
    private static final String SCRIPT_JS = "script_was_loaded = true;";

    private String getScriptFileTestPageHtml(final String scriptUrl) {
        return "<html>" +
                "  <head>" +
                "    <title>" + SCRIPT_NOT_LOADED + "</title>" +
                "    <script src='" + scriptUrl + "'></script>" +
                "  </head>" +
                "  <body onload=\"if(script_was_loaded) document.title='" + SCRIPT_LOADED + "'\">" +
                "  </body>" +
                "</html>";
    }

    private String getCrossOriginAccessTestPageHtml(final String iframeUrl) {
        return "<html>" +
                "  <body onload=\"" +
                "    document.title=document.getElementById('frame').contentWindow.location.href;" +
                "    \">" +
                "    <iframe id='frame' src='" + iframeUrl + "'>" +
                "  </body>" +
                "</html>";
    }


    @SmallTest
    @Feature({"Android-WebView"})
    public void testImageLoad() throws Throwable {
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            webServer.setResponseBase64("/" + CommonResources.FAVICON_FILENAME,
                    CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(true));

            ContentSettings contentSettings = getContentSettingsOnUiThread(mContentViewCore);
            contentSettings.setImagesEnabled(true);
            contentSettings.setJavaScriptEnabled(true);

            loadDataWithBaseUrlSync(
                    CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME),
                    "text/html", false, webServer.getBaseUrl(), null);

            assertEquals("5", getTitleOnUiThread(mContentViewCore));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testScriptLoad() throws Throwable {
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);

            final String scriptUrl = webServer.setResponse(SCRIPT_FILE, SCRIPT_JS,
                    CommonResources.getTextJavascriptHeaders(true));
            final String pageHtml = getScriptFileTestPageHtml(scriptUrl);

            getContentSettingsOnUiThread(mContentViewCore).setJavaScriptEnabled(true);
            loadDataWithBaseUrlSync(pageHtml, "text/html", false, webServer.getBaseUrl(), null);
            assertEquals(SCRIPT_LOADED, getTitleOnUiThread(mContentViewCore));

        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testSameOrigin() throws Throwable {
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String frameUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));
            final String html = getCrossOriginAccessTestPageHtml(frameUrl);

            getContentSettingsOnUiThread(mContentViewCore).setJavaScriptEnabled(true);
            loadDataWithBaseUrlSync(html, "text/html", false, webServer.getBaseUrl(), null);
            assertEquals(frameUrl, getTitleOnUiThread(mContentViewCore));

        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testCrossOrigin() throws Throwable {
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String frameUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));
            final String html = getCrossOriginAccessTestPageHtml(frameUrl);
            final String baseUrl = webServer.getBaseUrl().replaceFirst("localhost", "127.0.0.1");

            getContentSettingsOnUiThread(mContentViewCore).setJavaScriptEnabled(true);
            loadDataWithBaseUrlSync(html, "text/html", false, baseUrl, null);

            // TODO(mnaganov): Catch a security exception and set the title accordingly,
            // once https://bugs.webkit.org/show_bug.cgi?id=43504 is fixed.
            assertEquals("undefined", getTitleOnUiThread(mContentViewCore));

        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testNullBaseUrl() throws Throwable {
        getContentSettingsOnUiThread(mContentViewCore).setJavaScriptEnabled(true);
        final String pageHtml = "<html><body onload='document.title=document.location.href'>" +
                "</body></html>";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, null, null);
        assertEquals("about:blank", getTitleOnUiThread(mContentViewCore));
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testHistoryUrl() throws Throwable {

        final String pageHtml = "<html><body>Hello, world!</body></html>";
        final String baseUrl = "http://example.com";
        // TODO(mnaganov): Use the same string as Android CTS suite uses
        // once GURL issue is resolved (http://code.google.com/p/google-url/issues/detail?id=29)
        final String historyUrl = "http://history.com/";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, baseUrl, historyUrl);
        assertEquals(historyUrl, HistoryUtils.getUrlOnUiThread(
                getInstrumentation(), mContentViewCore));

        loadDataWithBaseUrlSync(pageHtml, "text/html", false, baseUrl, null);
        assertEquals("about:blank", HistoryUtils.getUrlOnUiThread(
                getInstrumentation(), mContentViewCore));
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testHistoryUrlIgnoredWithDataSchemeBaseUrl() throws Throwable {
        final String pageHtml = "<html><body>bar</body></html>";
        final String historyUrl = "http://history.com/";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, "data:foo", historyUrl);
        assertEquals("data:text/html," + pageHtml, HistoryUtils.getUrlOnUiThread(
                getInstrumentation(), mContentViewCore));
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testAccessToLocalFile() throws Throwable {
        getContentSettingsOnUiThread(mContentViewCore).setJavaScriptEnabled(true);
        final String baseUrl = UrlUtils.getTestFileUrl("webview/");
        final String scriptFile = baseUrl + "script.js";
        final String pageHtml = getScriptFileTestPageHtml(scriptFile);
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, baseUrl, null);
        assertEquals(SCRIPT_LOADED, getTitleOnUiThread(mContentViewCore));
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testFailedAccessToLocalFile() throws Throwable {
        getContentSettingsOnUiThread(mContentViewCore).setJavaScriptEnabled(true);
        final String scriptFile = UrlUtils.getTestFileUrl("webview/script.js");
        final String pageHtml = getScriptFileTestPageHtml(scriptFile);
        final String baseUrl = "http://example.com";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, baseUrl, null);
        assertEquals(SCRIPT_NOT_LOADED, getTitleOnUiThread(mContentViewCore));
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testHistoryUrlNavigation() throws Throwable {
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String historyUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));

            final String page1Title = "Page1";
            final String page1Html = "<html><head><title>" + page1Title + "</title>" +
                    "<body>" + page1Title + "</body></html>";

            loadDataWithBaseUrlSync(page1Html, "text/html", false, null, historyUrl);
            assertEquals(page1Title, getTitleOnUiThread(mContentViewCore));

            final String page2Title = "Page2";
            final String page2Html = "<html><head><title>" + page2Title + "</title>" +
                    "<body>" + page2Title + "</body></html>";

            final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();
            loadDataSync(mContentViewCore, onPageFinishedHelper, page2Html, "text/html", false);
            assertEquals(page2Title, getTitleOnUiThread(mContentViewCore));

            HistoryUtils.goBackSync(getInstrumentation(), mContentViewCore, onPageFinishedHelper);
            // The title of the 'about.html' specified via historyUrl.
            assertEquals(CommonResources.ABOUT_TITLE, getTitleOnUiThread(mContentViewCore));

        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }
}
