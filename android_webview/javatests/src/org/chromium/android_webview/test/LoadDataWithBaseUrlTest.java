// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Bitmap;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AndroidProtocolHandler;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentSettings;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.HistoryUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.io.File;
import java.io.FileOutputStream;
import java.util.concurrent.TimeUnit;

public class LoadDataWithBaseUrlTest extends AwTestBase {

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private ContentViewCore mContentViewCore;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mContentViewCore = testContainerView.getContentViewCore();
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
                mAwContents.loadUrl(LoadUrlParams.createLoadDataParamsWithBaseUrl(
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
    @Feature({"AndroidWebView"})
    public void testImageLoad() throws Throwable {
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            webServer.setResponseBase64("/" + CommonResources.FAVICON_FILENAME,
                    CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(true));

            ContentSettings contentSettings = getContentSettingsOnUiThread(mAwContents);
            contentSettings.setImagesEnabled(true);
            contentSettings.setJavaScriptEnabled(true);

            loadDataWithBaseUrlSync(
                    CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME),
                    "text/html", false, webServer.getBaseUrl(), null);

            assertEquals("5", getTitleOnUiThread(mAwContents));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testScriptLoad() throws Throwable {
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);

            final String scriptUrl = webServer.setResponse(SCRIPT_FILE, SCRIPT_JS,
                    CommonResources.getTextJavascriptHeaders(true));
            final String pageHtml = getScriptFileTestPageHtml(scriptUrl);

            getContentSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
            loadDataWithBaseUrlSync(pageHtml, "text/html", false, webServer.getBaseUrl(), null);
            assertEquals(SCRIPT_LOADED, getTitleOnUiThread(mAwContents));

        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSameOrigin() throws Throwable {
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String frameUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));
            final String html = getCrossOriginAccessTestPageHtml(frameUrl);

            getContentSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
            loadDataWithBaseUrlSync(html, "text/html", false, webServer.getBaseUrl(), null);
            assertEquals(frameUrl, getTitleOnUiThread(mAwContents));

        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCrossOrigin() throws Throwable {
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String frameUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));
            final String html = getCrossOriginAccessTestPageHtml(frameUrl);
            final String baseUrl = webServer.getBaseUrl().replaceFirst("localhost", "127.0.0.1");

            getContentSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
            loadDataWithBaseUrlSync(html, "text/html", false, baseUrl, null);

            // TODO(mnaganov): Catch a security exception and set the title accordingly,
            // once https://bugs.webkit.org/show_bug.cgi?id=43504 is fixed.
            assertEquals("undefined", getTitleOnUiThread(mAwContents));

        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullBaseUrl() throws Throwable {
        getContentSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        final String pageHtml = "<html><body onload='document.title=document.location.href'>" +
                "</body></html>";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, null, null);
        assertEquals("about:blank", getTitleOnUiThread(mAwContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
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
    @Feature({"AndroidWebView"})
    public void testHistoryUrlIgnoredWithDataSchemeBaseUrl() throws Throwable {
        final String pageHtml = "<html><body>bar</body></html>";
        final String historyUrl = "http://history.com/";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, "data:foo", historyUrl);
        assertEquals("data:text/html," + pageHtml, HistoryUtils.getUrlOnUiThread(
                getInstrumentation(), mContentViewCore));
    }

    /*
    @SmallTest
    @Feature({"AndroidWebView"})
    http://crbug.com/173274
    */
    @DisabledTest
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
            assertEquals(page1Title, getTitleOnUiThread(mAwContents));

            final String page2Title = "Page2";
            final String page2Html = "<html><head><title>" + page2Title + "</title>" +
                    "<body>" + page2Title + "</body></html>";

            final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();
            loadDataSync(mAwContents, onPageFinishedHelper, page2Html, "text/html", false);
            assertEquals(page2Title, getTitleOnUiThread(mAwContents));

            HistoryUtils.goBackSync(getInstrumentation(), mContentViewCore, onPageFinishedHelper);
            // The title of the 'about.html' specified via historyUrl.
            assertEquals(CommonResources.ABOUT_TITLE, getTitleOnUiThread(mAwContents));

        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    /**
     * @return true if |fileUrl| was accessible from a data url with |baseUrl| as it's
     * base URL.
     */
    private boolean canAccessFileFromData(String baseUrl, String fileUrl) throws Throwable {
        final String IMAGE_LOADED = "LOADED";
        final String IMAGE_NOT_LOADED = "NOT_LOADED";
        String data = "<html><body>" +
                "<img src=\"" + fileUrl + "\" " +
                "onload=\"document.title=\'" + IMAGE_LOADED + "\';\" " +
                "onerror=\"document.title=\'" + IMAGE_NOT_LOADED + "\';\" />" +
                "</body></html>";

        loadDataWithBaseUrlSync(data, "text/html", false, baseUrl, null);

        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    String title = getTitleOnUiThread(mAwContents);
                    return IMAGE_LOADED.equals(title) || IMAGE_NOT_LOADED.equals(title);
                } catch (Throwable t) {
                    return false;
                }
            }
        });

        return IMAGE_LOADED.equals(getTitleOnUiThread(mAwContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadDataWithBaseUrlAccessingFile() throws Throwable {
        // Create a temporary file on the filesystem we can try to read.
        File cacheDir = getActivity().getCacheDir();
        File tempImage = File.createTempFile("test_image", ".png", cacheDir);
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.RGB_565);
        FileOutputStream fos = new FileOutputStream(tempImage);
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, fos);
        fos.close();
        String imagePath = tempImage.getAbsolutePath();

        ContentSettings contentSettings = getContentSettingsOnUiThread(mAwContents);
        contentSettings.setImagesEnabled(true);
        contentSettings.setJavaScriptEnabled(true);

        try {
            final String DATA_BASE_URL = "data:";
            final String NON_DATA_BASE_URL = "http://example.com";

            mAwContents.getSettings().setAllowFileAccess(false);
            String token = "" + System.currentTimeMillis();
            // All access to file://, including android_asset and android_res is blocked
            // with a data: base URL, regardless of AwSettings.getAllowFileAccess().
            assertFalse(canAccessFileFromData(DATA_BASE_URL,
                  "file:///android_asset/asset_icon.png?" + token));
            assertFalse(canAccessFileFromData(DATA_BASE_URL,
                  "file:///android_res/raw/resource_icon.png?" + token));
            assertFalse(canAccessFileFromData(DATA_BASE_URL, "file://" + imagePath + "?" + token));

            // WebView always has access to android_asset and android_res for non-data
            // base URLs and can access other file:// URLs based on the value of
            // AwSettings.getAllowFileAccess().
            assertTrue(canAccessFileFromData(NON_DATA_BASE_URL,
                  "file:///android_asset/asset_icon.png?" + token));
            assertTrue(canAccessFileFromData(NON_DATA_BASE_URL,
                  "file:///android_res/raw/resource_icon.png?" + token));
            assertFalse(canAccessFileFromData(NON_DATA_BASE_URL,
                  "file://" + imagePath + "?" + token));

            token += "a";
            mAwContents.getSettings().setAllowFileAccess(true);
            // We should still be unable to access any file:// with when loading with a
            // data: base URL, but we should now be able to access the wider file system
            // (still restricted by OS-level permission checks) with a non-data base URL.
            assertFalse(canAccessFileFromData(DATA_BASE_URL,
                  "file:///android_asset/asset_icon.png?" + token));
            assertFalse(canAccessFileFromData(DATA_BASE_URL,
                  "file:///android_res/raw/resource_icon.png?" + token));
            assertFalse(canAccessFileFromData(DATA_BASE_URL, "file://" + imagePath + "?" + token));

            assertTrue(canAccessFileFromData(NON_DATA_BASE_URL,
                  "file:///android_asset/asset_icon.png?" + token));
            assertTrue(canAccessFileFromData(NON_DATA_BASE_URL,
                  "file:///android_res/raw/resource_icon.png?" + token));
            assertTrue(canAccessFileFromData(NON_DATA_BASE_URL,
                  "file://" + imagePath + "?" + token));
        } finally {
          if (!tempImage.delete()) throw new AssertionError();
        }
    }
}
