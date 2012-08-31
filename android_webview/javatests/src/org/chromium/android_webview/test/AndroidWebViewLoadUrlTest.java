// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;
import android.test.suitebuilder.annotation.Smoke;

import org.chromium.base.test.Feature;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwWebContentsDelegate;
import org.chromium.content.browser.ContentViewCore;

import java.util.concurrent.Callable;

/**
 * Test suite for loadUrl().
 */
public class AndroidWebViewLoadUrlTest extends AndroidWebViewTestBase {
    @SmallTest
    @Feature({"Android-WebView"})
    public void testDataUrl() throws Throwable {
        final String expectedTitle = "dataUrlTest";
        final String data =
            "<html><head><title>" + expectedTitle + "</title></head><body>foo</body></html>";

        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ContentViewCore contentViewCore =
            createAwTestContainerViewOnMainSync(contentsClient).getContentViewCore();
        loadDataSync(contentViewCore, contentsClient.getOnPageFinishedHelper(), data,
                     "text/html", false);
        assertEquals(expectedTitle, getTitleOnUiThread(contentViewCore));
    }

    @SmallTest
    @Feature({"Android-WebView"})
    public void testDataUrlBase64() throws Throwable {
        final String expectedTitle = "dataUrlTestBase64";
        final String data = "PGh0bWw+PGhlYWQ+PHRpdGxlPmRhdGFVcmxUZXN0QmFzZTY0PC90aXRsZT48" +
                            "L2hlYWQ+PC9odG1sPg==";

        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final ContentViewCore contentViewCore =
            createAwTestContainerViewOnMainSync(contentsClient).getContentViewCore();
        loadDataSync(contentViewCore, contentsClient.getOnPageFinishedHelper(), data,
                     "text/html", true);
        assertEquals(expectedTitle, getTitleOnUiThread(contentViewCore));
    }
}
