// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

/**
 * Tests the mixed use of synchronous and asynchronous find-in-page APIs in WebView.
 * Helps to spot race conditions or potential deadlocks caused by the renderer receiving
 * asynchronous messages while synchronous requests are being processed.
 */
public class WebViewMixedFindApisTest extends WebViewFindApisTestBase {

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testAsyncFindOperationsMixedWithSyncFind() throws Throwable {
        clearMatchesOnUiThread();
        assertEquals(4, findAllSyncOnUiThread("wood"));
        assertEquals(4, findAllSyncOnUiThread("wood"));
        clearMatchesOnUiThread();
        assertEquals(4, findAllAsyncOnUiThread("wood"));
        assertEquals(3, findNextOnUiThread(true));
        clearMatchesOnUiThread();
        assertEquals(4, findAllSyncOnUiThread("wood"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testInterleavedFinds() throws Throwable {
        assertEquals(4, findAllSyncOnUiThread("wood"));
        assertEquals(4, findAllAsyncOnUiThread("wood"));
        assertEquals(4, findAllSyncOnUiThread("wood"));
        assertEquals(3, findNextOnUiThread(true));
        assertEquals(4, findAllAsyncOnUiThread("wood"));
        assertEquals(1, findNextOnUiThread(true));
        assertEquals(4, findAllSyncOnUiThread("wood"));
    }
}
