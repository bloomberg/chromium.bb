// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.base.PathUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.io.File;

/**
 * Test suite for the HTTP cache.
 */
public class HttpCacheTest extends AwTestBase {

    @Override
    protected boolean needsBrowserProcessStarted() {
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

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLegacyHttpCacheDirIsRemovedOnStartup() throws Exception {
        Context targetContext = getInstrumentation().getTargetContext();
        PathUtils.setPrivateDataDirectorySuffix(
                AwBrowserProcess.PRIVATE_DATA_DIRECTORY_SUFFIX, targetContext);
        File webViewLegacyCacheDir = new File(
                PathUtils.getDataDirectory(targetContext), "Cache");
        if (!webViewLegacyCacheDir.isDirectory()) {
            assertTrue(webViewLegacyCacheDir.mkdir());
            assertTrue(webViewLegacyCacheDir.isDirectory());
        }
        File dummyCacheFile = File.createTempFile("test", null, webViewLegacyCacheDir);
        assertTrue(dummyCacheFile.exists());

        // Set up JNI bindings.
        AwBrowserProcess.loadLibrary(targetContext);
        // No delay before removing the legacy cache files.
        AwContentsStatics.setLegacyCacheRemovalDelayForTest(0);

        startBrowserProcess();
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();

        // Do some page loading to make sure that FILE thread has processed
        // our directory removal task.
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

        assertFalse(webViewLegacyCacheDir.exists());
        assertFalse(dummyCacheFile.exists());
    }
}
