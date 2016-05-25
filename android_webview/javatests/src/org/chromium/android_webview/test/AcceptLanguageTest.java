// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Tests for Accept Language implementation.
 */
public class AcceptLanguageTest extends AwTestBase {
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private EmbeddedTestServer mTestServer;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mAwContents = createAwTestContainerViewOnMainSync(mContentsClient).getAwContents();

        mTestServer = EmbeddedTestServer.createAndStartDefaultServer(
                getInstrumentation().getContext());
    }

    @Override
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    private static final Pattern COMMA_AND_OPTIONAL_Q_VALUE =
            Pattern.compile("(?:;q=[^,]+)?(?:,|$)");
    private static final Pattern MAYBE_QUOTED_STRING = Pattern.compile("^(\"?)(.*)\\1$");

    /**
     * Extract the languages from the Accept-Language header.
     *
     * The Accept-Language header can have more than one language along with optional quality
     * factors for each, e.g.
     *
     *  "de-DE,en-US;q=0.8,en-UK;q=0.5"
     *
     * This function extracts only the language strings from the Accept-Language header, so
     * the example above would yield the following:
     *
     *  ["de-DE", "en-US", "en-UK"]
     *
     * @param raw String containing the raw Accept-Language header
     * @return A list of languages as Strings.
     */
    private String[] getAcceptLanguages(String raw) {
        assertNotNull(raw);
        Matcher m = MAYBE_QUOTED_STRING.matcher(raw);
        assertTrue(m.matches());
        return COMMA_AND_OPTIONAL_Q_VALUE.split(m.group(2));
    }

    /**
     * Verify that the Accept Language string is correct.
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAcceptLanguage() throws Throwable {
        getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);

        // This should yield a lightly formatted page with the contents of the Accept-Language
        // header, e.g. "en-US" or "de-DE,en-US;q=0.8", as the only text content.
        String url = mTestServer.getURL("/echoheader?Accept-Language");
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        String[] acceptLanguages = getAcceptLanguages(
                JSUtils.executeJavaScriptAndWaitForResult(
                        this, mAwContents, mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                        "document.body.textContent"));
        assertEquals(LocaleUtils.getDefaultLocale(), acceptLanguages[0]);

        String[] acceptLanguagesJs = getAcceptLanguages(
                JSUtils.executeJavaScriptAndWaitForResult(
                        this, mAwContents, mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                        "navigator.languages.join(',')"));
        assertEquals(acceptLanguagesJs.length, acceptLanguages.length);
        for (int i = 0; i < acceptLanguagesJs.length; ++i) {
            assertEquals(acceptLanguagesJs[i], acceptLanguages[i]);
        }

        // Now test locale change at run time
        Locale.setDefault(new Locale("de", "DE"));
        mAwContents.setLocale("de-DE");
        mAwContents.getSettings().updateAcceptLanguages();

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        acceptLanguages = getAcceptLanguages(
                JSUtils.executeJavaScriptAndWaitForResult(
                        this, mAwContents, mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                        "document.body.textContent"));
        assertEquals(LocaleUtils.getDefaultLocale(), acceptLanguages[0]);
    }
}
