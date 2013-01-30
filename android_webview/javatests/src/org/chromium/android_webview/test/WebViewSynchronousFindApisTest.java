// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.FlakyTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

/**
 * Tests the synchronous find-in-page APIs in WebView.
 */
public class WebViewSynchronousFindApisTest extends WebViewFindApisTestBase {

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllFinds() throws Throwable {
        assertEquals(4, findAllSyncOnUiThread("wood"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllDouble() throws Throwable {
        findAllSyncOnUiThread("wood");
        assertEquals(4, findAllSyncOnUiThread("chuck"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllDoubleNext() throws Throwable {
        assertEquals(4, findAllSyncOnUiThread("wood"));
        assertEquals(4, findAllSyncOnUiThread("wood"));
        assertEquals(2, findNextOnUiThread(true));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllDoesNotFind() throws Throwable {
        assertEquals(0, findAllSyncOnUiThread("foo"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllEmptyPage() throws Throwable {
        assertEquals(0, findAllSyncOnUiThread("foo"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllEmptyString() throws Throwable {
        assertEquals(0, findAllSyncOnUiThread(""));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindNextForward() throws Throwable {
        assertEquals(4, findAllSyncOnUiThread("wood"));

        for (int i = 2; i <= 4; i++) {
            assertEquals(i - 1, findNextOnUiThread(true));
        }
        assertEquals(0, findNextOnUiThread(true));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindNextBackward() throws Throwable {
        assertEquals(4, findAllSyncOnUiThread("wood"));

        for (int i = 4; i >= 1; i--) {
            assertEquals(i - 1, findNextOnUiThread(false));
        }
        assertEquals(3, findNextOnUiThread(false));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindNextBig() throws Throwable {
        assertEquals(4, findAllSyncOnUiThread("wood"));

        assertEquals(1, findNextOnUiThread(true));
        assertEquals(0, findNextOnUiThread(false));
        assertEquals(3, findNextOnUiThread(false));
        for (int i = 1; i <= 4; i++) {
            assertEquals(i - 1, findNextOnUiThread(true));
        }
        assertEquals(0, findNextOnUiThread(true));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllEmptyNext() throws Throwable {
        assertEquals(4, findAllSyncOnUiThread("wood"));
        assertEquals(1, findNextOnUiThread(true));
        assertEquals(0, findAllSyncOnUiThread(""));
        assertEquals(0, findNextOnUiThread(true));
        assertEquals(0, findAllSyncOnUiThread(""));
        assertEquals(4, findAllSyncOnUiThread("wood"));
        assertEquals(1, findNextOnUiThread(true));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testClearMatches() throws Throwable {
        assertEquals(4, findAllSyncOnUiThread("wood"));
        clearMatchesOnUiThread();
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testClearFindNext() throws Throwable {
        assertEquals(4, findAllSyncOnUiThread("wood"));
        clearMatchesOnUiThread();
        assertEquals(4, findAllSyncOnUiThread("wood"));
        assertEquals(2, findNextOnUiThread(true));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindEmptyNext() throws Throwable {
        assertEquals(0, findAllSyncOnUiThread(""));
        assertEquals(0, findNextOnUiThread(true));
        assertEquals(4, findAllSyncOnUiThread("wood"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindNextFirst() throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                contents().findNext(true);
            }
        });
        assertEquals(4, findAllSyncOnUiThread("wood"));
        assertEquals(1, findNextOnUiThread(true));
        assertEquals(0, findNextOnUiThread(false));
        assertEquals(3, findNextOnUiThread(false));
    }
}
