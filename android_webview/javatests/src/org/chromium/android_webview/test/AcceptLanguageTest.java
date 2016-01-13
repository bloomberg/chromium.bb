// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.apache.http.Header;
import org.apache.http.HttpRequest;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.Locale;

/**
 * Tests for Accept Language implementation.
 */
public class AcceptLanguageTest extends AwTestBase {
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mAwContents = createAwTestContainerViewOnMainSync(mContentsClient).getAwContents();
    }

    private String getPrimaryAcceptLanguage(final HttpRequest request)
            throws Throwable {
        assertNotNull(request);

        Header[] headers = request.getHeaders("Accept-Language");
        assertEquals(1, headers.length);

        String acceptLanguageString = headers[0].getValue();
        assertNotNull(acceptLanguageString);
        return acceptLanguageString.split(",")[0];
    }

    /**
     * Verify that the Accept Language string is correct.
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAcceptLanguage() throws Throwable {
        getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);

        TestWebServer webServer = TestWebServer.start();
        try {
            HttpRequest request;

            final String pagePath = "/test_accept_language.html";
            final String pageUrl = webServer.setResponse(
                    pagePath, "<html><body>hello world</body></html>", null);
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                    pageUrl);

            assertEquals(1, webServer.getRequestCount(pagePath));

            request = webServer.getLastRequest(pagePath);

            assertEquals(LocaleUtils.getDefaultLocale(), getPrimaryAcceptLanguage(request));

            // Get rid of q-value for languages if any. The header with a q-value looks like:
            // Accept-Language: da, en-gb;q=0.8, en;q=0.7
            String acceptLanguagesFromRequest =
                    request.getHeaders("Accept-Language")[0].getValue().replaceAll(";q=[^,]+", "");

            String acceptLanguagesFromJS = JSUtils.executeJavaScriptAndWaitForResult(this,
                    mAwContents, mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                    "navigator.languages.join(',')");
            assertEquals(acceptLanguagesFromJS, "\"" + acceptLanguagesFromRequest + "\"");

            // Now test locale change at run time
            Locale.setDefault(new Locale("de", "DE"));
            mAwContents.setLocale("de-DE");
            mAwContents.getSettings().updateAcceptLanguages();

            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

            request = webServer.getLastRequest(pagePath);
            assertEquals(2, webServer.getRequestCount(pagePath));

            assertEquals(LocaleUtils.getDefaultLocale(), getPrimaryAcceptLanguage(request));

            acceptLanguagesFromRequest =
                    request.getHeaders("Accept-Language")[0].getValue().replaceAll(";q=[^,]+", "");
            acceptLanguagesFromJS = JSUtils.executeJavaScriptAndWaitForResult(this, mAwContents,
                    mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                    "navigator.languages.join(',')");
            assertEquals(acceptLanguagesFromJS, "\"" + acceptLanguagesFromRequest + "\"");
        } finally {
            webServer.shutdown();
        }
    }
}
