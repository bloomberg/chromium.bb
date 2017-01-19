// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.support.test.filters.SmallTest;

import org.chromium.content.browser.test.NativeLibraryTestBase;

/** Tests for {@link UrlUtilities}. */
public class UrlUtilitiesTest extends NativeLibraryTestBase {

    @Override
    public void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryNoBrowserProcess();
    }

    @SmallTest
    public void testIsAcceptedScheme() {
        assertTrue(UrlUtilities.isAcceptedScheme("about:awesome"));
        assertTrue(UrlUtilities.isAcceptedScheme("data:data"));
        assertTrue(UrlUtilities.isAcceptedScheme(
                "https://user:pass@awesome.com:9000/bad-scheme/#fake"));
        assertTrue(UrlUtilities.isAcceptedScheme("http://awesome.example.com/"));
        assertTrue(UrlUtilities.isAcceptedScheme("file://hostname/path/to/file"));
        assertTrue(UrlUtilities.isAcceptedScheme("inline:skates.co.uk"));
        assertTrue(UrlUtilities.isAcceptedScheme("javascript:alert(1)"));
        assertTrue(UrlUtilities.isAcceptedScheme("http://foo.bar/has[square].html"));

        assertFalse(UrlUtilities.isAcceptedScheme("super:awesome"));
        assertFalse(UrlUtilities.isAcceptedScheme(
                "ftp://https:password@example.com/"));
        assertFalse(UrlUtilities.isAcceptedScheme(
                "ftp://https:password@example.com/?http:#http:"));
        assertFalse(UrlUtilities.isAcceptedScheme(
                 "google-search://https:password@example.com/?http:#http:"));
        assertFalse(UrlUtilities.isAcceptedScheme("chrome://http://version"));
        assertFalse(UrlUtilities.isAcceptedScheme(""));
    }

    @SmallTest
    public void testIsDownloadableScheme() {
        assertTrue(UrlUtilities.isDownloadableScheme("data:data"));
        assertTrue(UrlUtilities.isDownloadableScheme(
                "https://user:pass@awesome.com:9000/bad-scheme:#fake:"));
        assertTrue(UrlUtilities.isDownloadableScheme("http://awesome.example.com/"));
        assertTrue(UrlUtilities.isDownloadableScheme(
                "filesystem:https://user:pass@google.com:99/t/foo;bar?q=a#ref"));
        assertTrue(UrlUtilities.isDownloadableScheme("blob:https://awesome.example.com/"));
        assertTrue(UrlUtilities.isDownloadableScheme("file://hostname/path/to/file"));

        assertFalse(UrlUtilities.isDownloadableScheme("inline:skates.co.uk"));
        assertFalse(UrlUtilities.isDownloadableScheme("javascript:alert(1)"));
        assertFalse(UrlUtilities.isDownloadableScheme("about:awesome"));
        assertFalse(UrlUtilities.isDownloadableScheme("super:awesome"));
        assertFalse(UrlUtilities.isDownloadableScheme("ftp://https:password@example.com/"));
        assertFalse(UrlUtilities.isDownloadableScheme(
                "ftp://https:password@example.com/?http:#http:"));
        assertFalse(UrlUtilities.isDownloadableScheme(
                "google-search://https:password@example.com/?http:#http:"));
        assertFalse(UrlUtilities.isDownloadableScheme("chrome://http://version"));
        assertFalse(UrlUtilities.isDownloadableScheme(""));
    }

    @SmallTest
    public void testIsValidForIntentFallbackUrl() {
        assertTrue(UrlUtilities.isValidForIntentFallbackNavigation(
                "https://user:pass@awesome.com:9000/bad-scheme:#fake:"));
        assertTrue(UrlUtilities.isValidForIntentFallbackNavigation("http://awesome.example.com/"));
        assertFalse(UrlUtilities.isValidForIntentFallbackNavigation("inline:skates.co.uk"));
        assertFalse(UrlUtilities.isValidForIntentFallbackNavigation("javascript:alert(1)"));
        assertFalse(UrlUtilities.isValidForIntentFallbackNavigation(""));
    }

    @SmallTest
    public void testValidateIntentUrl() {
        // Valid action, hostname, and (empty) path.
        assertTrue(UrlUtilities.validateIntentUrl(
                "intent://10010#Intent;scheme=tel;action=com.google.android.apps."
                + "authenticator.AUTHENTICATE;end"));
        // Valid package, scheme, hostname, and path.
        assertTrue(UrlUtilities.validateIntentUrl(
                "intent://scan/#Intent;package=com.google.zxing.client.android;"
                + "scheme=zxing;end;"));
        // Valid package, scheme, component, hostname, and path.
        assertTrue(UrlUtilities.validateIntentUrl(
                "intent://wump-hey.example.com/#Intent;package=com.example.wump;"
                + "scheme=yow;component=com.example.PUMPKIN;end;"));
        // Valid package, scheme, action, hostname, and path.
        assertTrue(UrlUtilities.validateIntentUrl(
                "intent://wump-hey.example.com/#Intent;package=com.example.wump;"
                + "scheme=eeek;action=frighten_children;end;"));
        // Valid package, component, String extra, hostname, and path.
        assertTrue(UrlUtilities.validateIntentUrl(
                "intent://testing/#Intent;package=cybergoat.noodle.crumpet;"
                + "component=wump.noodle/Crumpet;S.goat=leg;end"));

        // Valid package, component, int extra (with URL-encoded key), String
        // extra, hostname, and path.
        assertTrue(UrlUtilities.validateIntentUrl(
                "intent://testing/#Intent;package=cybergoat.noodle.crumpet;"
                + "component=wump.noodle/Crumpet;i.pumpkinCount%3D=42;"
                + "S.goat=leg;end"));

        // Android's Intent.toUri does not generate URLs like this, but
        // Google Authenticator does, and we must handle them.
        assertTrue(UrlUtilities.validateIntentUrl(
                "intent:#Intent;action=com.google.android.apps.chrome."
                + "TEST_AUTHENTICATOR;category=android.intent.category."
                + "BROWSABLE;S.inputData=cancelled;end"));

        // null does not have a valid intent scheme.
        assertFalse(UrlUtilities.validateIntentUrl(null));
        // The empty string does not have a valid intent scheme.
        assertFalse(UrlUtilities.validateIntentUrl(""));
        // A whitespace string does not have a valid intent scheme.
        assertFalse(UrlUtilities.validateIntentUrl(" "));
        // Junk after end.
        assertFalse(UrlUtilities.validateIntentUrl(
                "intent://10010#Intent;scheme=tel;action=com.google.android.apps."
                + "authenticator.AUTHENTICATE;end','*');"
                + "alert(document.cookie);//"));
        // component appears twice.
        assertFalse(UrlUtilities.validateIntentUrl(
                "intent://wump-hey.example.com/#Intent;package=com.example.wump;"
                + "scheme=yow;component=com.example.PUMPKIN;"
                + "component=com.example.AVOCADO;end;"));
        // scheme contains illegal character.
        assertFalse(UrlUtilities.validateIntentUrl(
                "intent://wump-hey.example.com/#Intent;package=com.example.wump;"
                + "scheme=hello+goodbye;component=com.example.PUMPKIN;end;"));
        // category contains illegal character.
        assertFalse(UrlUtilities.validateIntentUrl(
                "intent://wump-hey.example.com/#Intent;package=com.example.wump;"
                + "category=42%_by_volume;end"));
        // Incorrectly URL-encoded.
        assertFalse(UrlUtilities.validateIntentUrl(
                "intent://testing/#Intent;package=cybergoat.noodle.crumpet;"
                + "component=wump.noodle/Crumpet;i.pumpkinCount%%3D=42;"
                + "S.goat=&leg;end"));
    }

    @SmallTest
    public void testIsUrlWithinScope() {
        String scope = "http://www.example.com/sub";
        assertTrue(UrlUtilities.isUrlWithinScope(scope, scope));
        assertTrue(UrlUtilities.isUrlWithinScope(scope + "/path", scope));
        assertTrue(UrlUtilities.isUrlWithinScope(scope + "/a b/path", scope + "/a%20b"));

        assertFalse(UrlUtilities.isUrlWithinScope("https://www.example.com/sub", scope));
        assertFalse(UrlUtilities.isUrlWithinScope(scope, scope + "/inner"));
        assertFalse(UrlUtilities.isUrlWithinScope(scope + "/this", scope + "/different"));
        assertFalse(
                UrlUtilities.isUrlWithinScope("http://awesome.example.com", "http://example.com"));
        assertFalse(UrlUtilities.isUrlWithinScope(
                "https://www.google.com.evil.com", "https://www.google.com"));
    }

    @SmallTest
    public void testUrlsMatchIgnoringFragments() {
        String url = "http://www.example.com/path";
        assertTrue(UrlUtilities.urlsMatchIgnoringFragments(url, url));
        assertTrue(UrlUtilities.urlsMatchIgnoringFragments(url + "#fragment", url));
        assertTrue(UrlUtilities.urlsMatchIgnoringFragments(url + "#fragment", url + "#fragment2"));
        assertTrue(UrlUtilities.urlsMatchIgnoringFragments("HTTP://www.example.com/path"
                        + "#fragment",
                url + "#fragment2"));
        assertFalse(UrlUtilities.urlsMatchIgnoringFragments(
                url + "#fragment", "http://example.com:443/path#fragment"));
    }

    @SmallTest
    public void testUrlsFragmentsDiffer() {
        String url = "http://www.example.com/path";
        assertFalse(UrlUtilities.urlsFragmentsDiffer(url, url));
        assertTrue(UrlUtilities.urlsFragmentsDiffer(url + "#fragment", url));
    }
}
