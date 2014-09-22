// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.View;

import org.apache.http.Header;
import org.apache.http.HttpRequest;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.TestAwContentsClient.OnDownloadStartHelper;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.util.TestWebServer;

import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * AwContents tests.
 */
public class AwContentsTest extends AwTestBase {

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    @SmallTest
    @Feature({"AndroidWebView"})
    @UiThreadTest
    public void testCreateDestroy() throws Throwable {
        // NOTE this test runs on UI thread, so we cannot call any async methods.
        createAwTestContainerView(mContentsClient).getAwContents().destroy();
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateLoadPageDestroy() throws Throwable {
        AwTestContainerView awTestContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        loadUrlSync(awTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), CommonResources.ABOUT_HTML);
        destroyAwContentsOnMainSync(awTestContainerView.getAwContents());
        // It should be safe to call destroy multiple times.
        destroyAwContentsOnMainSync(awTestContainerView.getAwContents());
    }

    @LargeTest
    @Feature({"AndroidWebView"})
    public void testCreateLoadDestroyManyTimes() throws Throwable {
        for (int i = 0; i < 10; ++i) {
            AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
            AwContents awContents = testView.getAwContents();

            loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
            destroyAwContentsOnMainSync(awContents);
        }
    }

    @LargeTest
    @Feature({"AndroidWebView"})
    public void testCreateLoadDestroyManyAtOnce() throws Throwable {
        AwTestContainerView views[] = new AwTestContainerView[10];

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

    @SmallTest
    @Feature({"AndroidWebView"})
    @UiThreadTest
    public void testWebViewApisFailGracefullyAfterDestruction() throws Throwable {
        AwContents awContents =
                createAwTestContainerView(mContentsClient).getAwContents();
        awContents.destroy();

        assertNull(awContents.getWebContents());
        assertNull(awContents.getContentViewCore());
        assertNull(awContents.getNavigationController());

        // The documentation for WebView#destroy() reads "This method should be called
        // after this WebView has been removed from the view system. No other methods
        // may be called on this WebView after destroy".
        // However, some apps do not respect that restriction so we need to ensure that
        // we fail gracefully and do not crash when APIs are invoked after destruction.
        // Due to the large number of APIs we only test a representative selection here.
        awContents.clearHistory();
        awContents.loadUrl(new LoadUrlParams("http://www.google.com"));
        awContents.findAllAsync("search");
        assertNull(awContents.getUrl());
        assertNull(awContents.getContentSettings());
        assertFalse(awContents.canGoBack());
        awContents.disableJavascriptInterfacesInspection();
        awContents.invokeZoomPicker();
        awContents.onResume();
        awContents.stopLoading();
        awContents.onWindowVisibilityChanged(View.VISIBLE);
        awContents.requestFocus();
        awContents.isMultiTouchZoomSupported();
        awContents.setOverScrollMode(View.OVER_SCROLL_NEVER);
        awContents.pauseTimers();
        awContents.onContainerViewScrollChanged(200, 200, 100, 100);
        awContents.computeScroll();
        awContents.onMeasure(100, 100);
        awContents.onDraw(new Canvas());
        awContents.getMostRecentProgress();
        assertEquals(0, awContents.computeHorizontalScrollOffset());
        assertEquals(0, awContents.getContentWidthCss());
        awContents.onKeyUp(KeyEvent.KEYCODE_BACK,
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MENU));
    }

    @LargeTest
    @Feature({"AndroidWebView"})
    public void testCreateAndGcManyTimes() throws Throwable {
        final int concurrentInstances = 4;
        final int repetitions = 16;
        // The system retains a strong ref to the last focused view (in InputMethodManager)
        // so allow for 1 'leaked' instance.
        final int maxIdleInstances = 1;

        System.gc();

        pollOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return AwContents.getNativeInstanceCount() <= maxIdleInstances;
            }
        });
        for (int i = 0; i < repetitions; ++i) {
            for (int j = 0; j < concurrentInstances; ++j) {
                AwTestContainerView view = createAwTestContainerViewOnMainSync(mContentsClient);
                loadUrlAsync(view.getAwContents(), "about:blank");
            }
            assertTrue(AwContents.getNativeInstanceCount() >= concurrentInstances);
            assertTrue(AwContents.getNativeInstanceCount() <= (i + 1) * concurrentInstances);
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    getActivity().removeAllViews();
                }
            });
        }

        System.gc();

        pollOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return AwContents.getNativeInstanceCount() <= maxIdleInstances;
            }
        });
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUseAwSettingsAfterDestroy() throws Throwable {
        AwTestContainerView awTestContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        AwSettings awSettings = getAwSettingsOnUiThread(awTestContainerView.getAwContents());
        loadUrlSync(awTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), CommonResources.ABOUT_HTML);
        destroyAwContentsOnMainSync(awTestContainerView.getAwContents());

        // AwSettings should still be usable even after native side is destroyed.
        String newFontFamily = "serif";
        awSettings.setStandardFontFamily(newFontFamily);
        assertEquals(newFontFamily, awSettings.getStandardFontFamily());
        boolean newBlockNetworkLoads = !awSettings.getBlockNetworkLoads();
        awSettings.setBlockNetworkLoads(newBlockNetworkLoads);
        assertEquals(newBlockNetworkLoads, awSettings.getBlockNetworkLoads());
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
        assertTrue(s.tryAcquire(WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
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

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetFavicon() throws Throwable {
        AwContents.setShouldDownloadFavicons();
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

            getAwSettingsOnUiThread(awContents).setImagesEnabled(true);
            loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

            pollOnUiThread(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return awContents.getFavicon() != null &&
                        !awContents.getFavicon().sameAs(defaultFavicon);
                }
            });

            final Object originalFaviconSource = (new URL(faviconUrl)).getContent();
            final Bitmap originalFavicon =
                BitmapFactory.decodeStream((InputStream) originalFaviconSource);
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

    @Feature({"AndroidWebView", "setNetworkAvailable"})
    @SmallTest
    public void testSetNetworkAvailable() throws Throwable {
        AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();
        String script = "navigator.onLine";

        enableJavaScriptOnUiThread(awContents);
        loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");

        // Default to "online".
        assertEquals("true", executeJavaScriptAndWaitForResult(awContents, mContentsClient,
              script));

        // Forcing "offline".
        setNetworkAvailableOnUiThread(awContents, false);
        assertEquals("false", executeJavaScriptAndWaitForResult(awContents, mContentsClient,
              script));

        // Forcing "online".
        setNetworkAvailableOnUiThread(awContents, true);
        assertEquals("true", executeJavaScriptAndWaitForResult(awContents, mContentsClient,
              script));
    }


    static class JavaScriptObject {
        private CallbackHelper mCallbackHelper;
        public JavaScriptObject(CallbackHelper callbackHelper) {
            mCallbackHelper = callbackHelper;
        }

        public void run() {
            mCallbackHelper.notifyCalled();
        }
    }

    @Feature({"AndroidWebView", "JavaBridge"})
    @SmallTest
    public void testJavaBridge() throws Throwable {
        final AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        final CallbackHelper callback = new CallbackHelper();

        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                AwContents awContents = testView.getAwContents();
                AwSettings awSettings = awContents.getSettings();
                awSettings.setJavaScriptEnabled(true);
                awContents.addPossiblyUnsafeJavascriptInterface(
                        new JavaScriptObject(callback), "bridge", null);
                awContents.evaluateJavaScript("javascript:window.bridge.run();", null);
            }
        });
        callback.waitForCallback(0, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testEscapingOfErrorPage() throws Throwable {
        AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();
        String script = "window.failed == true";

        enableJavaScriptOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(awContents,
                "file:///file-that-does-not-exist#<script>window.failed = true;</script>");
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS,
                                             TimeUnit.MILLISECONDS);

        assertEquals("false", executeJavaScriptAndWaitForResult(awContents, mContentsClient,
                script));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testCanInjectHeaders() throws Throwable {
        final AwTestContainerView testContainer =
                createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testContainer.getAwContents();

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String pagePath = "/test_can_inject_headers.html";
            final String pageUrl = webServer.setResponse(
                    pagePath, "<html><body>foo</body></html>", null);

            final Map<String, String> extraHeaders = new HashMap<String, String>();
            extraHeaders.put("Referer", "foo");
            extraHeaders.put("X-foo", "bar");
            loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(),
                    webServer.getResponseUrl(pagePath), extraHeaders);

            assertEquals(1, webServer.getRequestCount(pagePath));

            HttpRequest request = webServer.getLastRequest(pagePath);
            assertNotNull(request);

            for (Map.Entry<String, String> value : extraHeaders.entrySet()) {
                String header = value.getKey();
                Header[] matchingHeaders = request.getHeaders(header);
                assertEquals("header " + header + " not found", 1, matchingHeaders.length);
                assertEquals(value.getValue(), matchingHeaders[0].getValue());
            }
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

}
