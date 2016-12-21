// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.UrlUtils;

import java.net.URI;
import java.net.URLDecoder;

public class EncodeHtmlDataUriTest extends InstrumentationTestCase {
    private static final String DATA_URI_PREFIX = "data:text/html;utf-8,";

    private String getData(String dataUri) {
        assertNotNull("Data URI is null", dataUri);
        assertTrue("Incorrect HTML Data URI prefix", dataUri.startsWith(DATA_URI_PREFIX));
        return dataUri.substring(DATA_URI_PREFIX.length());
    }

    private String decode(String dataUri) throws java.io.UnsupportedEncodingException {
        String data = getData(dataUri);
        return URLDecoder.decode(data, "UTF-8");
    }

    @SmallTest
    public void testDelimitersEncoding() throws java.io.UnsupportedEncodingException {
        String testString = "><#%\"'";
        String encodedUri = UrlUtils.encodeHtmlDataUri(testString);
        String decodedUri = decode(encodedUri);
        assertEquals("Delimiters are not properly encoded", decodedUri, testString);
    }

    @SmallTest
    public void testUnwiseCharactersEncoding() throws java.io.UnsupportedEncodingException {
        String testString = "{}|\\^[]`";
        String encodedUri = UrlUtils.encodeHtmlDataUri(testString);
        String decodedUri = decode(encodedUri);
        assertEquals("Unwise characters are not properly encoded", decodedUri, testString);
    }

    @SmallTest
    public void testWhitespaceEncoding() throws java.io.UnsupportedEncodingException {
        String testString = " \n\t";
        String encodedUri = UrlUtils.encodeHtmlDataUri(testString);
        String decodedUri = decode(encodedUri);
        assertEquals("Whitespace characters are not properly encoded", decodedUri, testString);
    }

    @SmallTest
    public void testReturnsValidUri()
            throws java.net.URISyntaxException, java.io.UnsupportedEncodingException {
        String testString = "<html><body onload=\"alert('Hello \\\"world\\\"');\"></body></html>";
        String encodedUri = UrlUtils.encodeHtmlDataUri(testString);
        String decodedUri = decode(encodedUri);
        // Verify that the encoded URI is valid.
        new URI(encodedUri);
        // Verify that something sensible was encoded.
        assertEquals("Simple HTML is not properly encoded", decodedUri, testString);
    }
}
