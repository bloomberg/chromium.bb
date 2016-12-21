// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;
import android.util.Pair;
import android.webkit.ConsoleMessage;
import android.webkit.WebSettings;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;

/**
 * Verify that content loading blocks initiated by renderer can be detected
 * by the embedder via WebChromeClient.onConsoleMessage.
 */
public class ConsoleMessagesForBlockedLoadsTest extends AwTestBase {

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient.AddMessageToConsoleHelper mOnConsoleMessageHelper;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mOnConsoleMessageHelper = mContentsClient.getAddMessageToConsoleHelper();
    }

    @Override
    protected void tearDown() throws Exception {
        if (mWebServer != null) mWebServer.shutdown();
        super.tearDown();
    }

    private void startWebServer() throws Exception {
        mWebServer = TestWebServer.start();
    }

    private ConsoleMessage getSingleErrorMessage() {
        ConsoleMessage result = null;
        for (ConsoleMessage m : mOnConsoleMessageHelper.getMessages()) {
            if (m.messageLevel() == ConsoleMessage.MessageLevel.ERROR) {
                assertNull(result);
                result = m;
            }
        }
        assertNotNull(result);
        return result;
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testXFrameOptionsDenial() throws Throwable {
        startWebServer();
        final String iframeHtml = CommonResources.makeHtmlPageFrom("", "FAIL");
        List<Pair<String, String>> iframeHeaders = new ArrayList<Pair<String, String>>();
        iframeHeaders.add(Pair.create("x-frame-options", "DENY"));
        final String iframeUrl = mWebServer.setResponse("/iframe.html", iframeHtml, iframeHeaders);
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<iframe src='" + iframeUrl + "' />");
        final String pageUrl = mWebServer.setResponse("/page.html", pageHtml, null);
        mOnConsoleMessageHelper.clearMessages();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        ConsoleMessage errorMessage = getSingleErrorMessage();
        assertTrue(errorMessage.message().indexOf(iframeUrl) != -1);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMixedContentDenial() throws Throwable {
        startWebServer();
        TestWebServer httpsServer = null;
        AwSettings settings = getAwSettingsOnUiThread(mAwContents);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        try {
            httpsServer = TestWebServer.startSsl();
            final String imageUrl = mWebServer.setResponseBase64(
                    "/insecure.png", CommonResources.FAVICON_DATA_BASE64, null);
            final String secureHtml = CommonResources.makeHtmlPageFrom(
                    "", "<img src='" + imageUrl + "' />");
            String secureUrl = httpsServer.setResponse("/secure.html", secureHtml, null);
            mOnConsoleMessageHelper.clearMessages();
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), secureUrl);
            ConsoleMessage errorMessage = getSingleErrorMessage();
            assertTrue(errorMessage.message().indexOf(imageUrl) != -1);
            assertTrue(errorMessage.message().indexOf(secureUrl) != -1);
        } finally {
            if (httpsServer != null) {
                httpsServer.shutdown();
            }
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCrossOriginDenial() throws Throwable {
        startWebServer();
        final String iframeXsl =
                "<?xml version='1.0' encoding='UTF-8'?>"
                + "<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>"
                + "<xsl:template match='*'>"
                + "<html><body>FAIL</body></html>"
                + "</xsl:template>"
                + "</xsl:stylesheet>";
        final String iframeXslUrl = mWebServer.setResponse(
                "/iframe.xsl", iframeXsl, null).replace("localhost", "127.0.0.1");
        final String iframeXml =
                "<?xml version='1.0' encoding='UTF-8'?>"
                + "<?xml-stylesheet type='text/xsl' href='" + iframeXslUrl + "'?>"
                + "<html xmlns='http://www.w3.org/1999/xhtml'>"
                + "<body>PASS</body></html>";
        final String iframeXmlUrl = mWebServer.setResponse("/iframe.xml", iframeXml, null);
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<iframe src='" + iframeXmlUrl + "' />");
        final String pageUrl = mWebServer.setResponse("/page.html", pageHtml, null);
        mOnConsoleMessageHelper.clearMessages();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        ConsoleMessage errorMessage = getSingleErrorMessage();
        assertTrue(errorMessage.message().indexOf(iframeXslUrl) != -1);
        assertTrue(errorMessage.message().indexOf(iframeXmlUrl) != -1);
    }
}
