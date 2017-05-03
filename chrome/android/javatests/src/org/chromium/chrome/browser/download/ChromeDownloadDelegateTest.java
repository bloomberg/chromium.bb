// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

import java.util.concurrent.Callable;

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
            super(context, tab);
        }

        @Override
        protected void onDownloadStartNoStream(DownloadInfo downloadInfo) {
        }
    }

    /**
     * Test to make sure {@link ChromeDownloadDelegate#shouldInterceptContextMenuDownload}
     * returns true only for ".dd" or ".dm" extensions with http/https scheme.
     */
    @SmallTest
    @Feature({"Download"})
    @RetryOnFailure
    public void testShouldInterceptContextMenuDownload() throws InterruptedException {
        final Tab tab = getActivity().getActivityTab();
        loadUrl("about:blank");
        ChromeDownloadDelegate delegate = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<ChromeDownloadDelegate>() {
                    @Override
                    public ChromeDownloadDelegate call() {
                        return new MockChromeDownloadDelegate(
                                getInstrumentation().getTargetContext(), tab);
                    }
                });
        assertFalse(delegate.shouldInterceptContextMenuDownload("file://test/test.html"));
        assertFalse(delegate.shouldInterceptContextMenuDownload("http://test/test.html"));
        assertFalse(delegate.shouldInterceptContextMenuDownload("ftp://test/test.dm"));
        assertFalse(delegate.shouldInterceptContextMenuDownload("data://test.dd"));
        assertFalse(delegate.shouldInterceptContextMenuDownload("http://test.dd"));
        assertFalse(delegate.shouldInterceptContextMenuDownload("http://test/test.dd"));
        assertTrue(delegate.shouldInterceptContextMenuDownload("https://test/test.dm"));
    }

    @SmallTest
    @Feature({"Download"})
    public void testGetFileExtension() {
        assertEquals("ext", ChromeDownloadDelegate.getFileExtension("", "file.ext"));
        assertEquals("ext", ChromeDownloadDelegate.getFileExtension("http://file.ext", ""));
        assertEquals("txt", ChromeDownloadDelegate.getFileExtension("http://file.ext", "file.txt"));
        assertEquals("txt", ChromeDownloadDelegate.getFileExtension(
                "http://file.ext", "file name.txt"));
    }
}
