// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

/**
 * Tests for ChromeDownloadDelegate class.
 */
public class ChromeDownloadDelegateTest extends InstrumentationTestCase {

    /**
     * Test to make sure {@link ChromeDownloadDelegate#fileName} returns the
     * right file for different URLs and MIME types.
     */
    @SmallTest
    @Feature({"Download"})
    public void testFileName() {
        String testUrl = "http://server.com/file.pdf";
        assertEquals("file.pdf", ChromeDownloadDelegate.fileName(testUrl, "application/pdf", ""));
        assertEquals("file.pdf", ChromeDownloadDelegate.fileName(testUrl, "", ""));

        // .php is an unknown MIME format extension.
        // This used to generate file.php even when the MIME type was set.
        // http://code.google.com/p/chromium/issues/detail?id=134396
        testUrl = "http://server.com/file.php";

        assertEquals("file.pdf", ChromeDownloadDelegate.fileName(testUrl, "application/pdf", ""));
        assertEquals("file.php", ChromeDownloadDelegate.fileName(testUrl, "", ""));

        // .xml is a known MIME format extension.
        testUrl = "http://server.com/file.xml";
        assertEquals("file.xml", ChromeDownloadDelegate.fileName(testUrl, "", ""));

        assertEquals("file.pdf", ChromeDownloadDelegate.fileName(testUrl, "application/pdf", ""));

        // If the file's extension and HTTP header's MIME type are the same, use
        // the former to derive the final extension.
        // https://code.google.com/p/chromium/issues/detail?id=170852
        testUrl = "http://server.com/file.mp3";
        assertEquals("file.mp3", ChromeDownloadDelegate.fileName(testUrl, "audio/mpeg", ""));

        testUrl = "http://server.com/";
        assertEquals("downloadfile.bin", ChromeDownloadDelegate.fileName(testUrl, "", ""));
        assertEquals("downloadfile.pdf",
                ChromeDownloadDelegate.fileName(testUrl, "application/pdf", ""));

        // Fails to match the filename pattern from header; uses one from url.
        // Note that header itself is a valid one.
        testUrl = "http://server.com/file.pdf";
        assertEquals("file.pdf", ChromeDownloadDelegate.fileName(testUrl, "application/pdf",
                "attachment; name=\"foo\"; filename=\"bar\""));
        assertEquals("file.pdf", ChromeDownloadDelegate.fileName(testUrl, "application/pdf",
                "attachment; filename=\"bar\"; name=\"foo\""));
        assertEquals("file.pdf", ChromeDownloadDelegate.fileName(testUrl, "application/pdf",
                "attachment; filename=\"bar\"; filename*=utf-8''baz"));
    }
}
