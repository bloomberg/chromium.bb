// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.DownloadInfo;

/**
 * Tests for ChromeDownloadDelegate class.
 */
public class ChromeDownloadDelegateTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    public ChromeDownloadDelegateTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    /**
     * Mock class for test.
     */
    static class MockChromeDownloadDelegate extends ChromeDownloadDelegate {
        public MockChromeDownloadDelegate(Context context, Tab tab) {
            super(context, null, tab);
        }

        @Override
        protected void onDownloadStartNoStream(DownloadInfo downloadInfo) {
        }
    }

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

    /**
     * Test to make sure {@link ChromeDownloadDelegate#shouldInterceptContextMenuDownload}
     * returns true only for ".dd" or ".dm" extensions with http/https scheme.
     */
    @SmallTest
    @Feature({"Download"})
    public void testShouldInterceptContextMenuDownload() {
        Tab tab = new Tab(0, false, getActivity().getWindowAndroid());
        ChromeDownloadDelegate delegate =
                new MockChromeDownloadDelegate(getInstrumentation().getTargetContext(), tab);
        assertFalse(delegate.shouldInterceptContextMenuDownload("file://test/test.html"));
        assertFalse(delegate.shouldInterceptContextMenuDownload("http://test/test.html"));
        assertFalse(delegate.shouldInterceptContextMenuDownload("ftp://test/test.dm"));
        assertFalse(delegate.shouldInterceptContextMenuDownload("data://test.dd"));
        assertFalse(delegate.shouldInterceptContextMenuDownload("http://test.dd"));
        assertFalse(delegate.shouldInterceptContextMenuDownload("http://test/test.dd"));
        assertTrue(delegate.shouldInterceptContextMenuDownload("https://test/test.dm"));
    }
}
