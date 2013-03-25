// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.util.TestWebServer;

import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.List;

/**
 * AwContents tests.
 */
public class AwContentsTest extends AwTestBase {
    public static class OnDownloadStartHelper extends CallbackHelper {
        String mUrl;
        String mUserAgent;
        String mContentDisposition;
        String mMimeType;
        long mContentLength;

        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }

        public String getUserAgent() {
            assert getCallCount() > 0;
            return mUserAgent;
        }

        public String getContentDisposition() {
            assert getCallCount() > 0;
            return mContentDisposition;
        }

        public String getMimeType() {
            assert getCallCount() > 0;
            return mMimeType;
        }

        public long getContentLength() {
            assert getCallCount() > 0;
            return mContentLength;
        }

        public void notifyCalled(String url, String userAgent, String contentDisposition,
                String mimeType, long contentLength) {
            mUrl = url;
            mUserAgent = userAgent;
            mContentDisposition = contentDisposition;
            mMimeType = mimeType;
            mContentLength = contentLength;
            notifyCalled();
        }
    }

    private static class TestAwContentsClient
            extends org.chromium.android_webview.test.TestAwContentsClient {

        private OnDownloadStartHelper mOnDownloadStartHelper;

        public TestAwContentsClient() {
            mOnDownloadStartHelper = new OnDownloadStartHelper();
        }

        public OnDownloadStartHelper getOnDownloadStartHelper() {
            return mOnDownloadStartHelper;
        }

        @Override
        public void onDownloadStart(String url,
                                    String userAgent,
                                    String contentDisposition,
                                    String mimeType,
                                    long contentLength) {
            getOnDownloadStartHelper().notifyCalled(url, userAgent, contentDisposition, mimeType,
                    contentLength);
        }
    }

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    @SmallTest
    @Feature({"AndroidWebView"})
    @UiThreadTest
    public void testCreateDestroy() throws Throwable {
        // NOTE this test runs on UI thread, so we cannot call any async methods.
        createAwTestContainerView(mContentsClient).getAwContents().destroy();
    }

    /*
     * @LargeTest
     * @Feature({"AndroidWebView"})
     * Disabled until we switch to final rendering pipeline.
     */
    @DisabledTest
    public void testCreateLoadDestroyManyTimes() throws Throwable {
        final int CREATE_AND_DESTROY_REPEAT_COUNT = 10;
        for (int i = 0; i < CREATE_AND_DESTROY_REPEAT_COUNT; ++i) {
            AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
            AwContents awContents = testView.getAwContents();

            loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
            destroyAwContentsOnMainSync(awContents);
        }
    }

    /*
     * @LargeTest
     * @Feature({"AndroidWebView"})
     * Disabled until we switch to final rendering pipeline.
     */
    @DisabledTest
    public void testCreateLoadDestroyManyAtOnce() throws Throwable {
        final int CREATE_AND_DESTROY_REPEAT_COUNT = 10;
        AwTestContainerView views[] = new AwTestContainerView[CREATE_AND_DESTROY_REPEAT_COUNT];

        for (int i = 0; i < views.length; ++i) {
            views[i] = createAwTestContainerViewOnMainSync(mContentsClient);
            loadUrlSync(views[i].getAwContents(), mContentsClient.getOnPageFinishedHelper(),
                    "about:blank");
        }

        for (int i = 0; i < views.length; ++i) {
            destroyAwContentsOnMainSync(views[i].getAwContents());
            views[i] = null;
        }
    }

    private int callDocumentHasImagesSync(final AwContents awContents)
            throws Throwable, InterruptedException {
        // Set up a container to hold the result object and a semaphore to
        // make the test wait for the result.
        final AtomicInteger val = new AtomicInteger();
        final Semaphore s = new Semaphore(0);
        final Message msg = Message.obtain(new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(Message msg) {
                val.set(msg.arg1);
                s.release();
            }
        });
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
              awContents.documentHasImages(msg);
            }
        });
        assertTrue(s.tryAcquire(WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS));
        int result = val.get();
        return result;
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDocumentHasImages() throws Throwable {
        AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();

        final CallbackHelper loadHelper = mContentsClient.getOnPageFinishedHelper();

        final String mime = "text/html";
        final String emptyDoc = "<head/><body/>";
        final String imageDoc = "<head/><body><img/><img/></body>";

        // Make sure a document that does not have images returns 0
        loadDataSync(awContents, loadHelper, emptyDoc, mime, false);
        int result = callDocumentHasImagesSync(awContents);
        assertEquals(0, result);

        // Make sure a document that does have images returns 1
        loadDataSync(awContents, loadHelper, imageDoc, mime, false);
        result = callDocumentHasImagesSync(awContents);
        assertEquals(1, result);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testClearCacheMemoryAndDisk() throws Throwable {
        final AwTestContainerView testContainer =
                createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testContainer.getAwContents();

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String pagePath = "/clear_cache_test.html";
            List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
            // Set Cache-Control headers to cache this request. One century should be long enough.
            headers.add(Pair.create("Cache-Control", "max-age=3153600000"));
            headers.add(Pair.create("Last-Modified", "Wed, 3 Oct 2012 00:00:00 GMT"));
            final String pageUrl = webServer.setResponse(
                    pagePath, "<html><body>foo</body></html>", headers);

            // First load to populate cache.
            clearCacheOnUiThread(awContents, true);
            loadUrlSync(awContents,
                        mContentsClient.getOnPageFinishedHelper(),
                        pageUrl);
            assertEquals(1, webServer.getRequestCount(pagePath));

            // Load about:blank so next load is not treated as reload by webkit and force
            // revalidate with the server.
            loadUrlSync(awContents,
                        mContentsClient.getOnPageFinishedHelper(),
                        "about:blank");

            // No clearCache call, so should be loaded from cache.
            loadUrlSync(awContents,
                        mContentsClient.getOnPageFinishedHelper(),
                        pageUrl);
            assertEquals(1, webServer.getRequestCount(pagePath));

            // Same as above.
            loadUrlSync(awContents,
                        mContentsClient.getOnPageFinishedHelper(),
                        "about:blank");

            // Clear cache, so should hit server again.
            clearCacheOnUiThread(awContents, true);
            loadUrlSync(awContents,
                        mContentsClient.getOnPageFinishedHelper(),
                        pageUrl);
            assertEquals(2, webServer.getRequestCount(pagePath));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testClearCacheInQuickSuccession() throws Throwable {
        final AwTestContainerView testContainer =
                createAwTestContainerViewOnMainSync(new TestAwContentsClient());
        final AwContents awContents = testContainer.getAwContents();

        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
              for (int i = 0; i < 10; ++i) {
                  awContents.clearCache(true);
              }
            }
        });
    }

    private static final long TEST_TIMEOUT = 20000L;
    private static final int CHECK_INTERVAL = 100;

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetFavicon() throws Throwable {
        final AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);

            final String faviconUrl = webServer.setResponseBase64(
                    "/" + CommonResources.FAVICON_FILENAME, CommonResources.FAVICON_DATA_BASE64,
                    CommonResources.getImagePngHeaders(false));
            final String pageUrl = webServer.setResponse("/favicon.html",
                    CommonResources.FAVICON_STATIC_HTML, null);

            // The getFavicon will return the right icon a certain time after
            // the page load completes which makes it slightly hard to test.
            final Bitmap defaultFavicon = awContents.getFavicon();

            getContentSettingsOnUiThread(awContents).setImagesEnabled(true);
            loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

            assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return awContents.getFavicon() != null &&
                        !awContents.getFavicon().sameAs(defaultFavicon);
                }
            }, TEST_TIMEOUT, CHECK_INTERVAL));

            final Object originalFaviconSource = (new URL(faviconUrl)).getContent();
            final Bitmap originalFavicon =
                BitmapFactory.decodeStream((InputStream)originalFaviconSource);
            assertNotNull(originalFavicon);

            assertTrue(awContents.getFavicon().sameAs(originalFavicon));

        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @Feature({"AndroidWebView", "Downloads"})
    @SmallTest
    public void testDownload() throws Throwable {
        AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();

        final String data = "download data";
        final String contentDisposition = "attachment;filename=\"download.txt\"";
        final String mimeType = "text/plain";

        List<Pair<String, String>> downloadHeaders = new ArrayList<Pair<String, String>>();
        downloadHeaders.add(Pair.create("Content-Disposition", contentDisposition));
        downloadHeaders.add(Pair.create("Content-Type", mimeType));
        downloadHeaders.add(Pair.create("Content-Length", Integer.toString(data.length())));

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String pageUrl = webServer.setResponse(
                    "/download.txt", data, downloadHeaders);
            final OnDownloadStartHelper downloadStartHelper =
                mContentsClient.getOnDownloadStartHelper();
            final int callCount = downloadStartHelper.getCallCount();
            loadUrlAsync(awContents, pageUrl);
            downloadStartHelper.waitForCallback(callCount);

            assertEquals(pageUrl, downloadStartHelper.getUrl());
            assertEquals(contentDisposition, downloadStartHelper.getContentDisposition());
            assertEquals(mimeType, downloadStartHelper.getMimeType());
            assertEquals(data.length(), downloadStartHelper.getContentLength());
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }
}
