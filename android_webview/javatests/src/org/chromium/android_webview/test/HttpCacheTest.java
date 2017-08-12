// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.io.File;

/**
 * Test suite for the HTTP cache.
 */
public class HttpCacheTest extends AwTestBase {

    @Override
    public boolean needsBrowserProcessStarted() {
        return false;
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpCacheIsInsideCacheDir() throws Exception {
        File webViewCacheDir = new File(
                getInstrumentation().getTargetContext().getCacheDir().getPath(),
                "org.chromium.android_webview");
        deleteDirectory(webViewCacheDir);

        startBrowserProcess();
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();

        TestWebServer httpServer = null;
        try {
            httpServer = TestWebServer.start();
            final String pageUrl = "/page.html";
            final String pageHtml = "<body>Hello, World!</body>";
            final String fullPageUrl = httpServer.setResponse(pageUrl, pageHtml, null);
            loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), fullPageUrl);
            assertEquals(1, httpServer.getRequestCount(pageUrl));
        } finally {
            if (httpServer != null) {
                httpServer.shutdown();
            }
        }

        assertTrue(webViewCacheDir.isDirectory());
        assertTrue(webViewCacheDir.list().length > 0);
    }

    private void deleteDirectory(File dir) throws Exception {
        if (!dir.exists()) return;
        assertTrue(dir.isDirectory());
        Process rmrf = Runtime.getRuntime().exec("rm -rf " + dir.getAbsolutePath());
        rmrf.waitFor();
        assertFalse(dir.exists());
    }
}
