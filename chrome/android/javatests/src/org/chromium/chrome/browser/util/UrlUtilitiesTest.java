// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.NativeLibraryTestBase;

import java.net.URI;

public class UrlUtilitiesTest extends NativeLibraryTestBase {
    @SmallTest
    public void testIsAcceptedScheme() {
        assertTrue(UrlUtilities.isAcceptedScheme("about:awesome"));
        assertTrue(UrlUtilities.isAcceptedScheme("data:data"));
        assertTrue(UrlUtilities.isAcceptedScheme(
                "https://user:pass@:awesome.com:9000/bad-scheme:#fake:"));
        assertTrue(UrlUtilities.isAcceptedScheme("http://awesome.example.com/"));
        assertTrue(UrlUtilities.isAcceptedScheme("file://awesome.example.com/"));
        assertTrue(UrlUtilities.isAcceptedScheme("inline:skates.co.uk"));
        assertTrue(UrlUtilities.isAcceptedScheme("javascript:alert(1)"));

        assertFalse(UrlUtilities.isAcceptedScheme("super:awesome"));
        assertFalse(UrlUtilities.isAcceptedScheme(
                "ftp://https:password@example.com/"));
        assertFalse(UrlUtilities.isAcceptedScheme(
                "ftp://https:password@example.com/?http:#http:"));
        assertFalse(UrlUtilities.isAcceptedScheme(
                 "google-search://https:password@example.com/?http:#http:"));
        assertFalse(UrlUtilities.isAcceptedScheme("chrome://http://version"));
        assertFalse(UrlUtilities.isAcceptedScheme(""));
        assertFalse(UrlUtilities.isAcceptedScheme("  http://awesome.example.com/"));
        assertFalse(UrlUtilities.isAcceptedScheme("ht\ntp://awesome.example.com/"));
    }

    @SmallTest
    public void testIsDownloadableScheme() {
        assertTrue(UrlUtilities.isDownloadableScheme("data:data"));
        assertTrue(UrlUtilities.isDownloadableScheme(
                "https://user:pass@:awesome.com:9000/bad-scheme:#fake:"));
        assertTrue(UrlUtilities.isDownloadableScheme("http://awesome.example.com/"));
        assertTrue(UrlUtilities.isDownloadableScheme("filesystem://awesome.example.com/"));
        assertTrue(UrlUtilities.isDownloadableScheme("blob:https%3A//awesome.example.com/"));
        assertTrue(UrlUtilities.isDownloadableScheme("file://awesome.example.com/"));

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
        assertFalse(UrlUtilities.isDownloadableScheme("  http://awesome.example.com/"));
        assertFalse(UrlUtilities.isDownloadableScheme("ht\ntp://awesome.example.com/"));
    }

    @SmallTest
    public void testIsValidForIntentFallbackUrl() {
        assertTrue(UrlUtilities.isValidForIntentFallbackNavigation(
                "https://user:pass@:awesome.com:9000/bad-scheme:#fake:"));
        assertTrue(UrlUtilities.isValidForIntentFallbackNavigation("http://awesome.example.com/"));
        assertFalse(UrlUtilities.isValidForIntentFallbackNavigation("inline:skates.co.uk"));
        assertFalse(UrlUtilities.isValidForIntentFallbackNavigation("javascript:alert(1)"));
        assertFalse(UrlUtilities.isValidForIntentFallbackNavigation(""));
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testFormatUrlForSecurityDisplay() {
        loadNativeLibraryNoBrowserProcess();

        URI uri;

        uri = URI.create("http://chopped.com/is/awesome");
        assertEquals("http://chopped.com", UrlUtilities.formatUrlForSecurityDisplay(uri, true));
        assertEquals("chopped.com", UrlUtilities.formatUrlForSecurityDisplay(uri, false));

        uri = URI.create("http://lopped.com");
        assertEquals("http://lopped.com", UrlUtilities.formatUrlForSecurityDisplay(uri, true));
        assertEquals("lopped.com", UrlUtilities.formatUrlForSecurityDisplay(uri, false));

        uri = URI.create("http://dropped.com?things");
        assertEquals("http://dropped.com", UrlUtilities.formatUrlForSecurityDisplay(uri, true));
        assertEquals("dropped.com", UrlUtilities.formatUrlForSecurityDisplay(uri, false));

        uri = URI.create("http://dfalcant@stopped.com:1234");
        assertEquals(
                "http://stopped.com:1234", UrlUtilities.formatUrlForSecurityDisplay(uri, true));
        assertEquals("stopped.com:1234", UrlUtilities.formatUrlForSecurityDisplay(uri, false));

        uri = URI.create("http://dfalcant:secret@stopped.com:9999");
        assertEquals(
                "http://stopped.com:9999", UrlUtilities.formatUrlForSecurityDisplay(uri, true));
        assertEquals("stopped.com:9999", UrlUtilities.formatUrlForSecurityDisplay(uri, false));

        uri = URI.create("chrome://settings:443");
        assertEquals("chrome://settings:443", UrlUtilities.formatUrlForSecurityDisplay(uri, true));
        assertEquals("chrome://settings:443", UrlUtilities.formatUrlForSecurityDisplay(uri, false));

        uri = URI.create("about:blank");
        assertEquals("about:blank", UrlUtilities.formatUrlForSecurityDisplay(uri, true));
        assertEquals("about:blank", UrlUtilities.formatUrlForSecurityDisplay(uri, false));
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

}
