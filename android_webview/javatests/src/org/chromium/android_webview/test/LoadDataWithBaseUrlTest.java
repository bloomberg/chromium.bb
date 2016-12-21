// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Bitmap;
import android.support.test.filters.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.content.browser.test.util.HistoryUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.util.TestWebServer;

import java.io.File;
import java.io.FileOutputStream;
import java.util.concurrent.Callable;

/**
 * Tests for the {@link android.webkit.WebView#loadDataWithBaseURL(String, String, String, String,
 * String)} method.
 */
public class LoadDataWithBaseUrlTest extends AwTestBase {

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private WebContents mWebContents;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mWebContents = mAwContents.getWebContents();
    }

    protected void loadDataWithBaseUrlSync(
            final String data, final String mimeType, final boolean isBase64Encoded,
            final String baseUrl, final String historyUrl) throws Throwable {
        loadDataWithBaseUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                data, mimeType, isBase64Encoded, baseUrl, historyUrl);
    }

    private static final String SCRIPT_FILE = "/script.js";
    private static final String SCRIPT_LOADED = "Loaded";
    private static final String SCRIPT_NOT_LOADED = "Not loaded";
    private static final String SCRIPT_JS = "script_was_loaded = true;";

    private String getScriptFileTestPageHtml(final String scriptUrl) {
        return "<html>"
                + "  <head>"
                + "    <title>" + SCRIPT_NOT_LOADED + "</title>"
                + "    <script src='" + scriptUrl + "'></script>"
                + "  </head>"
                + "  <body onload=\"if(script_was_loaded) document.title='" + SCRIPT_LOADED + "'\">"
                + "  </body>"
                + "</html>";
    }

    private String getCrossOriginAccessTestPageHtml(final String iframeUrl) {
        return "<html>"
                + "  <head>"
                + "    <script>"
                + "      function onload() {"
                + "        try {"
                + "          document.title = "
                + "            document.getElementById('frame').contentWindow.location.href;"
                + "        } catch (e) {"
                + "          document.title = 'Exception';"
                + "        }"
                + "      }"
                + "    </script>"
                + "  </head>"
                + "  <body onload='onload()'>"
                + "    <iframe id='frame' src='" + iframeUrl + "'></iframe>"
                + "  </body>"
                + "</html>";
    }


    @SmallTest
    @Feature({"AndroidWebView"})
    public void testImageLoad() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            webServer.setResponseBase64("/" + CommonResources.FAVICON_FILENAME,
                    CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(true));

            AwSettings contentSettings = getAwSettingsOnUiThread(mAwContents);
            contentSettings.setImagesEnabled(true);
            contentSettings.setJavaScriptEnabled(true);

            loadDataWithBaseUrlSync(
                    CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME),
                    "text/html", false, webServer.getBaseUrl(), null);

            assertEquals("5", getTitleOnUiThread(mAwContents));
        } finally {
            webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testScriptLoad() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String scriptUrl = webServer.setResponse(SCRIPT_FILE, SCRIPT_JS,
                    CommonResources.getTextJavascriptHeaders(true));
            final String pageHtml = getScriptFileTestPageHtml(scriptUrl);

            getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
            loadDataWithBaseUrlSync(pageHtml, "text/html", false, webServer.getBaseUrl(), null);
            assertEquals(SCRIPT_LOADED, getTitleOnUiThread(mAwContents));
        } finally {
            webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSameOrigin() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String frameUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));
            final String html = getCrossOriginAccessTestPageHtml(frameUrl);

            getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
            loadDataWithBaseUrlSync(html, "text/html", false, webServer.getBaseUrl(), null);
            assertEquals(frameUrl, getTitleOnUiThread(mAwContents));
        } finally {
            webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCrossOrigin() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String frameUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));
            final String html = getCrossOriginAccessTestPageHtml(frameUrl);
            final String baseUrl = webServer.getBaseUrl().replaceFirst("localhost", "127.0.0.1");

            getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
            loadDataWithBaseUrlSync(html, "text/html", false, baseUrl, null);

            assertEquals("Exception", getTitleOnUiThread(mAwContents));
        } finally {
            webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullBaseUrl() throws Throwable {
        getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        final String pageHtml = "<html><body onload='document.title=document.location.href'>"
                + "</body></html>";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, null, null);
        assertEquals("about:blank", getTitleOnUiThread(mAwContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testInvalidBaseUrl() throws Throwable {
        final String invalidBaseUrl = "http://";
        getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        loadDataWithBaseUrlSync(
                CommonResources.ABOUT_HTML, "text/html", false, invalidBaseUrl, null);
        // Verify that the load succeeds. The actual base url is undefined.
        assertEquals(CommonResources.ABOUT_TITLE, getTitleOnUiThread(mAwContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testloadDataWithBaseUrlCallsOnPageStarted() throws Throwable {
        final String baseUrl = "http://base.com/";
        TestCallbackHelperContainer.OnPageStartedHelper onPageStartedHelper =
                mContentsClient.getOnPageStartedHelper();
        final int callCount = onPageStartedHelper.getCallCount();
        loadDataWithBaseUrlAsync(mAwContents, CommonResources.ABOUT_HTML, "text/html", false,
                baseUrl, "about:blank");
        onPageStartedHelper.waitForCallback(callCount);
        assertEquals(baseUrl, onPageStartedHelper.getUrl());
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
                getInstrumentation(), mWebContents));

        loadDataWithBaseUrlSync(pageHtml, "text/html", false, baseUrl, null);
        assertEquals("about:blank", HistoryUtils.getUrlOnUiThread(
                getInstrumentation(), mWebContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedUrlIsBaseUrl() throws Throwable {
        final String pageHtml = "<html><body>Hello, world!</body></html>";
        final String baseUrl = "http://example.com/";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, baseUrl, baseUrl);
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, baseUrl, baseUrl);
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        assertEquals(baseUrl, onPageFinishedHelper.getUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHistoryUrlIgnoredWithDataSchemeBaseUrl() throws Throwable {
        final String pageHtml = "<html><body>bar</body></html>";
        final String historyUrl = "http://history.com/";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, "data:foo", historyUrl);
        assertEquals("data:text/html," + pageHtml, HistoryUtils.getUrlOnUiThread(
                getInstrumentation(), mWebContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHistoryUrlNavigation() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String historyUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));

            final String page1Title = "Page1";
            final String page1Html = "<html><head><title>" + page1Title + "</title>"
                    + "<body>" + page1Title + "</body></html>";

            loadDataWithBaseUrlSync(page1Html, "text/html", false, null, historyUrl);
            assertEquals(page1Title, getTitleOnUiThread(mAwContents));

            final String page2Title = "Page2";
            final String page2Html = "<html><head><title>" + page2Title + "</title>"
                    + "<body>" + page2Title + "</body></html>";

            final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();
            loadDataSync(mAwContents, onPageFinishedHelper, page2Html, "text/html", false);
            assertEquals(page2Title, getTitleOnUiThread(mAwContents));

            HistoryUtils.goBackSync(getInstrumentation(), mWebContents, onPageFinishedHelper);
            // The title of first page loaded with loadDataWithBaseUrl.
            assertEquals(page1Title, getTitleOnUiThread(mAwContents));
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * @return true if |fileUrl| was accessible from a data url with |baseUrl| as it's
     * base URL.
     */
    private boolean canAccessFileFromData(String baseUrl, String fileUrl) throws Throwable {
        final String imageLoaded = "LOADED";
        final String imageNotLoaded = "NOT_LOADED";
        String data = "<html><body>"
                + "<img src=\"" + fileUrl + "\" "
                + "onload=\"document.title=\'" + imageLoaded + "\';\" "
                + "onerror=\"document.title=\'" + imageNotLoaded + "\';\" />"
                + "</body></html>";

        loadDataWithBaseUrlSync(data, "text/html", false, baseUrl, null);

        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                String title = getTitleOnUiThread(mAwContents);
                return imageLoaded.equals(title) || imageNotLoaded.equals(title);
            }
        });

        return imageLoaded.equals(getTitleOnUiThread(mAwContents));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @SuppressWarnings("Finally")
    public void testLoadDataWithBaseUrlAccessingFile() throws Throwable {
        // Create a temporary file on the filesystem we can try to read.
        File cacheDir = getActivity().getCacheDir();
        File tempImage = File.createTempFile("test_image", ".png", cacheDir);
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.RGB_565);
        FileOutputStream fos = new FileOutputStream(tempImage);
        try {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, fos);
        } finally {
            fos.close();
        }
        String imagePath = tempImage.getAbsolutePath();

        AwSettings contentSettings = getAwSettingsOnUiThread(mAwContents);
        contentSettings.setImagesEnabled(true);
        contentSettings.setJavaScriptEnabled(true);

        try {
            final String dataBaseUrl = "data:";
            final String nonDataBaseUrl = "http://example.com";

            mAwContents.getSettings().setAllowFileAccess(false);
            String token = "" + System.currentTimeMillis();
            // All access to file://, including android_asset and android_res is blocked
            // with a data: base URL, regardless of AwSettings.getAllowFileAccess().
            assertFalse(canAccessFileFromData(dataBaseUrl,
                    "file:///android_asset/asset_icon.png?" + token));
            assertFalse(canAccessFileFromData(dataBaseUrl,
                    "file:///android_res/raw/resource_icon.png?" + token));
            assertFalse(canAccessFileFromData(dataBaseUrl, "file://" + imagePath + "?" + token));

            // WebView always has access to android_asset and android_res for non-data
            // base URLs and can access other file:// URLs based on the value of
            // AwSettings.getAllowFileAccess().
            assertTrue(canAccessFileFromData(nonDataBaseUrl,
                    "file:///android_asset/asset_icon.png?" + token));
            assertTrue(canAccessFileFromData(nonDataBaseUrl,
                    "file:///android_res/raw/resource_icon.png?" + token));
            assertFalse(canAccessFileFromData(nonDataBaseUrl,
                    "file://" + imagePath + "?" + token));

            token += "a";
            mAwContents.getSettings().setAllowFileAccess(true);
            // We should still be unable to access any file:// with when loading with a
            // data: base URL, but we should now be able to access the wider file system
            // (still restricted by OS-level permission checks) with a non-data base URL.
            assertFalse(canAccessFileFromData(dataBaseUrl,
                    "file:///android_asset/asset_icon.png?" + token));
            assertFalse(canAccessFileFromData(dataBaseUrl,
                    "file:///android_res/raw/resource_icon.png?" + token));
            assertFalse(canAccessFileFromData(dataBaseUrl, "file://" + imagePath + "?" + token));

            assertTrue(canAccessFileFromData(nonDataBaseUrl,
                    "file:///android_asset/asset_icon.png?" + token));
            assertTrue(canAccessFileFromData(nonDataBaseUrl,
                    "file:///android_res/raw/resource_icon.png?" + token));
            assertTrue(canAccessFileFromData(nonDataBaseUrl,
                    "file://" + imagePath + "?" + token));
        } finally {
            if (!tempImage.delete()) throw new AssertionError();
        }
    }

    /**
     * Disallowed from running on Svelte devices due to OOM errors: crbug.com/598013
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testLoadLargeData() throws Throwable {
        // Chrome only allows URLs up to 2MB in IPC. Test something larger than this.
        // Note that the real URI may be significantly large if it gets encoded into
        // base64.
        final int kDataLength = 5 * 1024 * 1024;
        StringBuilder doc = new StringBuilder();
        doc.append("<html><head></head><body><!-- ");
        int i = doc.length();
        doc.setLength(i + kDataLength);
        while (i < doc.length()) doc.setCharAt(i++, 'A');
        doc.append("--><script>window.gotToEndOfBody=true;</script></body></html>");

        enableJavaScriptOnUiThread(mAwContents);
        loadDataWithBaseUrlSync(doc.toString(), "text/html", false, null, null);
        assertEquals("true", executeJavaScriptAndWaitForResult(mAwContents, mContentsClient,
                        "window.gotToEndOfBody"));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedWhenInterrupted() throws Throwable {
        // See crbug.com/594001 -- when a javascript: URL is loaded, the pending entry
        // gets discarded and the previous load goes through a different path
        // inside NavigationController.
        final String pageHtml = "<html><body>Hello, world!</body></html>";
        final String baseUrl = "http://example.com/";
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        final int callCount = onPageFinishedHelper.getCallCount();
        loadDataWithBaseUrlAsync(mAwContents, pageHtml, "text/html", false, baseUrl, null);
        loadUrlAsync(mAwContents, "javascript:42");
        onPageFinishedHelper.waitForCallback(callCount);
        assertEquals(baseUrl, onPageFinishedHelper.getUrl());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedWithInvalidBaseUrlWhenInterrupted() throws Throwable {
        final String pageHtml = CommonResources.ABOUT_HTML;
        final String invalidBaseUrl = "http://";
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        final int callCount = onPageFinishedHelper.getCallCount();
        getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        loadDataWithBaseUrlAsync(mAwContents, pageHtml, "text/html", false, invalidBaseUrl, null);
        loadUrlAsync(mAwContents, "javascript:42");
        onPageFinishedHelper.waitForCallback(callCount);
        // Verify that the load succeeds. The actual base url is undefined.
        assertEquals(CommonResources.ABOUT_TITLE, getTitleOnUiThread(mAwContents));
    }
}
