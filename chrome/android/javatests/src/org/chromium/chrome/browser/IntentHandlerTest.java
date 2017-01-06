// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Browser;
import android.speech.RecognizerResultsIntent;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.test.UiThreadTest;

import org.chromium.base.CollectionUtil;
import org.chromium.base.CommandLine;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.NativeLibraryTestBase;

import java.util.Vector;

/**
 * Tests for IntentHandler.
 * TODO(nileshagrawal): Add tests for onNewIntent.
 */
public class IntentHandlerTest extends NativeLibraryTestBase {
    private static final String VOICE_SEARCH_QUERY = "VOICE_QUERY";
    private static final String VOICE_SEARCH_QUERY_URL = "http://www.google.com/?q=VOICE_QUERY";

    private static final String VOICE_URL_QUERY = "www.google.com";
    private static final String VOICE_URL_QUERY_URL = "INVALID_URLZ";

    private static final String[] ACCEPTED_INTENT_URLS = {"http://www.google.com",
            "http://movies.nytimes.com/movie/review?"
                    + "res=9405EFDB1E3BE23BBC4153DFB7678382659EDE&partner=Rotten Tomatoes",
            "chrome://newtab", "file://foo.txt", "ftp://www.foo.com", "https://www.gmail.com", "",
            "http://www.example.com/\u00FCmlat.html&q=name", "http://www.example.com/quotation_\"",
            "http://www.example.com/lessthansymbol_<", "http://www.example.com/greaterthansymbol_>",
            "http://www.example.com/poundcharacter_#", "http://www.example.com/percentcharacter_%",
            "http://www.example.com/leftcurlybrace_{", "http://www.example.com/rightcurlybrace_}",
            "http://www.example.com/verticalpipe_|", "http://www.example.com/backslash_\\",
            "http://www.example.com/caret_^", "http://www.example.com/tilde_~",
            "http://www.example.com/leftsquarebracket_[",
            "http://www.example.com/rightsquarebracket_]", "http://www.example.com/graveaccent_`",
            "www.example.com", "www.google.com", "www.bing.com", "notreallyaurl",
            "://javascript:80/hello", "https:awesome@google.com/haha.gif",
            "ftp@https://confusing:@something.example:5/goat?sayit", "://www.google.com/",
            "//www.google.com", "chrome-search://food",
            "java-scr\nipt://alert", // - is significant
            "java.scr\nipt://alert", // . is significant
            "java+scr\nipt://alert", // + is significant
            "http ://time", "iris.beep:app"};

    private static final String[] REJECTED_INTENT_URLS = {"javascript://", " javascript:alert(1) ",
            "jar:http://www.example.com/jarfile.jar!/",
            "jar:http://www.example.com/jarfile.jar!/mypackage/myclass.class",
            "  \tjava\nscript\n:alert(1)  ", "javascript://window.opener",
            "   javascript:fun@somethings.com/yeah", " j\na\nr\t:f:oobarz ",
            "jar://http://@foo.com/test.html", "  jar://https://@foo.com/test.html",
            "javascript:http//bar.net:javascript/yes.no", " javascript:://window.open(1)",
            " java script:alert(1)", "~~~javascript://alert"};

    private static final String[] REJECTED_GOOGLECHROME_URLS = {
            IntentHandler.GOOGLECHROME_SCHEME + "://reddit.com",
            IntentHandler.GOOGLECHROME_SCHEME + "://navigate?reddit.com",
            IntentHandler.GOOGLECHROME_SCHEME + "://navigate?urlreddit.com",
    };

    private static final String GOOGLE_URL = "https://www.google.com";

    private IntentHandler mIntentHandler;
    private Intent mIntent;

    private void processUrls(String[] urls, boolean isValid) {
        Vector<String> failedTests = new Vector<String>();

        for (String url : urls) {
            mIntent.setData(Uri.parse(url));
            if (mIntentHandler.intentHasValidUrl(mIntent) != isValid) {
                failedTests.add(url);
            }
        }
        assertTrue(failedTests.toString(), failedTests.isEmpty());
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        CommandLine.init(null);
        loadNativeLibraryAndInitBrowserProcess();
        IntentHandler.setTestIntentsEnabled(false);
        mIntentHandler = new IntentHandler(null, null);
        mIntent = new Intent();
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testAcceptedUrls() {
        processUrls(ACCEPTED_INTENT_URLS, true);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRejectedUrls() {
        processUrls(REJECTED_INTENT_URLS, false);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testAcceptedGoogleChromeSchemeNavigateUrls() {
        // Test all of the accepted URLs after prepending googlechrome://navigate?url.
        String[] expectedAccepts = new String[ACCEPTED_INTENT_URLS.length];
        for (int i = 0; i < ACCEPTED_INTENT_URLS.length; ++i) {
            expectedAccepts[i] =
                    IntentHandler.GOOGLECHROME_NAVIGATE_PREFIX + ACCEPTED_INTENT_URLS[i];
        }
        processUrls(expectedAccepts, true);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRejectedGoogleChromeSchemeNavigateUrls() {
        // Test all of the rejected URLs after prepending googlechrome://navigate?url.
        String[] expectedRejections = new String[REJECTED_INTENT_URLS.length];
        for (int i = 0; i < REJECTED_INTENT_URLS.length; ++i) {
            expectedRejections[i] =
                    IntentHandler.GOOGLECHROME_NAVIGATE_PREFIX + REJECTED_INTENT_URLS[i];
        }
        processUrls(expectedRejections, false);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRejectedGoogleChromeSchemeUrls() {
        Vector<String> failedTests = new Vector<String>();

        for (String url : REJECTED_GOOGLECHROME_URLS) {
            mIntent.setData(Uri.parse(url));
            if (IntentHandler.getUrlFromIntent(mIntent) != null) {
                failedTests.add(url);
            }
        }
        assertTrue(failedTests.toString(), failedTests.isEmpty());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testNullUrlIntent() {
        mIntent.setData(null);
        assertTrue(
                "Intent with null data should be valid", mIntentHandler.intentHasValidUrl(mIntent));
    }

    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testGetQueryFromVoiceSearchResultIntent_validVoiceQuery() {
        Intent intent = new Intent(RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS);
        intent.putStringArrayListExtra(RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_STRINGS,
                CollectionUtil.newArrayList(VOICE_SEARCH_QUERY));
        intent.putStringArrayListExtra(RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_URLS,
                CollectionUtil.newArrayList(VOICE_SEARCH_QUERY_URL));
        String query = IntentHandler.getUrlFromVoiceSearchResult(intent);
        assertEquals(VOICE_SEARCH_QUERY_URL, query);
    }

    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testGetQueryFromVoiceSearchResultIntent_validUrlQuery() {
        Intent intent = new Intent(RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS);
        intent.putStringArrayListExtra(RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_STRINGS,
                CollectionUtil.newArrayList(VOICE_URL_QUERY));
        intent.putStringArrayListExtra(RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_URLS,
                CollectionUtil.newArrayList(VOICE_URL_QUERY_URL));
        String query = IntentHandler.getUrlFromVoiceSearchResult(intent);
        assertTrue(String.format("Expected qualified URL: %s, to start "
                                   + "with http://www.google.com",
                           query),
                query.indexOf("http://www.google.com") == 0);
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_extraReferrer() {
        // Check that EXTRA_REFERRER is not accepted with a random URL.
        Intent foreignIntent = new Intent(Intent.ACTION_VIEW);
        foreignIntent.putExtra(Intent.EXTRA_REFERRER, GOOGLE_URL);
        assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(foreignIntent));

        // Check that EXTRA_REFERRER with android-app URL works.
        final String appUrl = "android-app://com.application/http/www.application.com";
        Intent appIntent = new Intent(Intent.ACTION_VIEW);
        appIntent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(appUrl));
        assertEquals(appUrl, IntentHandler.getReferrerUrlIncludingExtraHeaders(appIntent));

        // Check that EXTRA_REFERRER_NAME with android-app works.
        Intent nameIntent = new Intent(Intent.ACTION_VIEW);
        nameIntent.putExtra(Intent.EXTRA_REFERRER_NAME, appUrl);
        assertEquals(appUrl, IntentHandler.getReferrerUrlIncludingExtraHeaders(nameIntent));
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_extraHeadersInclReferer() {
        // Check that invalid header specified in EXTRA_HEADERS isn't used.
        Bundle bundle = new Bundle();
        bundle.putString("X-custom-header", "X-custom-value");
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        assertEquals("X-custom-header: X-custom-value",
                IntentHandler.getExtraHeadersFromIntent(headersIntent));
        assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_extraHeadersInclRefererMultiple() {
        // Check that invalid header specified in EXTRA_HEADERS isn't used.
        Bundle bundle = new Bundle();
        bundle.putString("X-custom-header", "X-custom-value");
        bundle.putString("X-custom-header-2", "X-custom-value-2");
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        assertEquals("X-custom-header-2: X-custom-value-2\nX-custom-header: X-custom-value",
                IntentHandler.getExtraHeadersFromIntent(headersIntent));
        assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_extraHeadersOnlyReferer() {
        // Check that invalid header specified in EXTRA_HEADERS isn't used.
        Bundle bundle = new Bundle();
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_extraHeadersAndExtraReferrer() {
        String validReferer = "android-app://package/http/url";
        Bundle bundle = new Bundle();
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        headersIntent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(validReferer));
        assertEquals(validReferer,
                IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
        assertNull(IntentHandler.getExtraHeadersFromIntent(headersIntent));
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_extraHeadersValidReferrer() {
        String validReferer = "android-app://package/http/url";
        Bundle bundle = new Bundle();
        bundle.putString("Referer", validReferer);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        assertEquals(validReferer,
                IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
        assertNull(IntentHandler.getExtraHeadersFromIntent(headersIntent));
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testAddTimestampToIntent() {
        Intent intent = new Intent();
        assertEquals(-1, IntentHandler.getTimestampFromIntent(intent));
        // Check both before and after to make sure that the returned value is
        // really from {@link SystemClock#elapsedRealtime()}.
        long before = SystemClock.elapsedRealtime();
        IntentHandler.addTimestampToIntent(intent);
        long after = SystemClock.elapsedRealtime();
        assertTrue("Time should be increasing",
                before <= IntentHandler.getTimestampFromIntent(intent));
        assertTrue("Time should be increasing",
                IntentHandler.getTimestampFromIntent(intent) <= after);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGeneratedReferrer() {
        Context context = getInstrumentation().getTargetContext();
        String packageName = context.getPackageName();
        String referrer = IntentHandler.constructValidReferrerForAuthority(packageName).getUrl();
        assertEquals("android-app://" + packageName, referrer);
    }
}
