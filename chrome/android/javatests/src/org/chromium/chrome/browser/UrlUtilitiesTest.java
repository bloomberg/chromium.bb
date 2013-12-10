// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

import java.net.URI;
import java.net.URISyntaxException;

public class UrlUtilitiesTest extends InstrumentationTestCase {
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

        assertFalse(UrlUtilities.isDownloadableScheme("inline:skates.co.uk"));
        assertFalse(UrlUtilities.isDownloadableScheme("javascript:alert(1)"));
        assertFalse(UrlUtilities.isDownloadableScheme("file://awesome.example.com/"));
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
    public void testFixUrl() throws URISyntaxException {
        try {
            URI uri;
            uri = new URI(UrlUtilities.fixUrl("google.com"));
            assertTrue("http".equals(uri.getScheme()));
            uri = new URI(UrlUtilities.fixUrl("\n://user:pass@example.com:80/"));
            assertTrue("http".equals(uri.getScheme()));
            uri = new URI(UrlUtilities.fixUrl("inline:google.com"));
            assertTrue("inline".equals(uri.getScheme()));
            uri = new URI(UrlUtilities.fixUrl("chrome:user:pass@google:443/leg:foot"));
            assertTrue("chrome".equals(uri.getScheme()));
            uri = new URI(UrlUtilities.fixUrl("https://mail.google.com:/"));
            assertTrue("https".equals(uri.getScheme()));
            uri = new URI(UrlUtilities.fixUrl("://mail.google.com:/"));
            assertTrue("http".equals(uri.getScheme()));
            uri = new URI(UrlUtilities.fixUrl("//mail.google.com:/"));
            assertTrue("http".equals(uri.getScheme()));
        } catch (URISyntaxException e) {
            assertFalse(true);
        }
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testGetOriginForDisplay() {
        URI uri;

        uri = URI.create("http://chopped.com/is/awesome");
        assertEquals("http://chopped.com", UrlUtilities.getOriginForDisplay(uri, true));
        assertEquals("chopped.com", UrlUtilities.getOriginForDisplay(uri, false));

        uri = URI.create("http://lopped.com");
        assertEquals("http://lopped.com", UrlUtilities.getOriginForDisplay(uri, true));
        assertEquals("lopped.com", UrlUtilities.getOriginForDisplay(uri, false));

        uri = URI.create("http://dropped.com?things");
        assertEquals("http://dropped.com", UrlUtilities.getOriginForDisplay(uri, true));
        assertEquals("dropped.com", UrlUtilities.getOriginForDisplay(uri, false));

        uri = URI.create("http://dfalcant@stopped.com:1234");
        assertEquals("http://stopped.com:1234", UrlUtilities.getOriginForDisplay(uri, true));
        assertEquals("stopped.com:1234", UrlUtilities.getOriginForDisplay(uri, false));

        uri = URI.create("http://dfalcant:secret@stopped.com:9999");
        assertEquals("http://stopped.com:9999", UrlUtilities.getOriginForDisplay(uri, true));
        assertEquals("stopped.com:9999", UrlUtilities.getOriginForDisplay(uri, false));

        uri = URI.create("chrome://settings:443");
        assertEquals("chrome://settings:443", UrlUtilities.getOriginForDisplay(uri, true));
        assertEquals("settings:443", UrlUtilities.getOriginForDisplay(uri, false));

        uri = URI.create("about:blank");
        assertEquals("about:blank", UrlUtilities.getOriginForDisplay(uri, true));
        assertEquals("about:blank", UrlUtilities.getOriginForDisplay(uri, false));
    }

}
