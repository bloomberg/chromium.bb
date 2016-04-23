// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
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
import android.webkit.JavascriptInterface;

import org.apache.http.Header;
import org.apache.http.HttpRequest;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.AwSwitches;
import org.chromium.android_webview.test.TestAwContentsClient.OnDownloadStartHelper;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.parameter.ParameterizedTest;
import org.chromium.content.browser.BindingManager;
import org.chromium.content.browser.ChildProcessConnection;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.content.browser.test.util.CallbackHelper;
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

        // The documentation for WebView#destroy() reads "This method should be called
        // after this WebView has been removed from the view system. No other methods
        // may be called on this WebView after destroy".
        // However, some apps do not respect that restriction so we need to ensure that
        // we fail gracefully and do not crash when APIs are invoked after destruction.
        // Due to the large number of APIs we only test a representative selection here.
        awContents.clearHistory();
        awContents.loadUrl("http://www.google.com");
        awContents.findAllAsync("search");
        assertNull(awContents.getUrl());
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

        TestWebServer webServer = TestWebServer.start();
        try {
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
            webServer.shutdown();
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

        TestWebServer webServer = TestWebServer.start();
        try {
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

            pollUiThread(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return awContents.getFavicon() != null
                            && !awContents.getFavicon().sameAs(defaultFavicon);
                }
            });

            final Object originalFaviconSource = (new URL(faviconUrl)).getContent();
            final Bitmap originalFavicon =
                    BitmapFactory.decodeStream((InputStream) originalFaviconSource);
            assertNotNull(originalFavicon);

            assertTrue(awContents.getFavicon().sameAs(originalFavicon));

        } finally {
            webServer.shutdown();
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

        TestWebServer webServer = TestWebServer.start();
        try {
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
            webServer.shutdown();
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

        @JavascriptInterface
        public void run() {
            mCallbackHelper.notifyCalled();
        }
    }

    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
                awContents.addJavascriptInterface(new JavaScriptObject(callback), "bridge");
                awContents.evaluateJavaScriptForTests("window.bridge.run();", null);
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

    @SuppressFBWarnings("DLS_DEAD_LOCAL_STORE")
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testCanInjectHeaders() throws Throwable {
        final AwTestContainerView testContainer =
                createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testContainer.getAwContents();

        TestWebServer webServer = TestWebServer.start();
        try {
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
            webServer.shutdown();
        }
    }

    // This is a meta test that we don't accidentally turn off hardware
    // acceleration in instrumentation tests without notice. Do not add the
    // @DisableHardwareAccelerationForTest annotation for this test.
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testHardwareModeWorks() throws Throwable {
        AwTestContainerView testContainer =
                createAwTestContainerViewOnMainSync(mContentsClient);
        assertTrue(testContainer.isHardwareAccelerated());
        assertTrue(testContainer.isBackedByHardwareView());
    }

    // TODO(hush): more ssl tests. And put the ssl tests into a separate test
    // class.
    @Feature({"AndroidWebView"})
    @SmallTest
    // If the user allows the ssl error, the same ssl error will not trigger
    // the onReceivedSslError callback; If the user denies it, the same ssl
    // error will still trigger the onReceivedSslError callback.
    public void testSslPreferences() throws Throwable {
        final AwTestContainerView testContainer =
                createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testContainer.getAwContents();
        TestWebServer webServer = TestWebServer.startSsl();
        final String pagePath = "/hello.html";
        final String pageUrl =
                webServer.setResponse(pagePath, "<html><body>hello world</body></html>", null);
        final CallbackHelper onReceivedSslErrorHelper =
                mContentsClient.getOnReceivedSslErrorHelper();
        int onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();

        loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

        assertEquals(onSslErrorCallCount + 1, onReceivedSslErrorHelper.getCallCount());
        assertEquals(1, webServer.getRequestCount(pagePath));

        // Now load the page again. This time, we expect no ssl error, because
        // user's decision should be remembered.
        onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        assertEquals(onSslErrorCallCount, onReceivedSslErrorHelper.getCallCount());

        // Now clear the ssl preferences then load the same url again. Expect to see
        // onReceivedSslError getting called again.
        awContents.clearSslPreferences();
        onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        assertEquals(onSslErrorCallCount + 1, onReceivedSslErrorHelper.getCallCount());

        // Now clear the stored decisions and tell the client to deny ssl errors.
        awContents.clearSslPreferences();
        mContentsClient.setAllowSslError(false);
        onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        assertEquals(onSslErrorCallCount + 1, onReceivedSslErrorHelper.getCallCount());

        // Now load the same page again. This time, we still expect onReceivedSslError,
        // because we only remember user's decision if it is "allow".
        onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        assertEquals(onSslErrorCallCount + 1, onReceivedSslErrorHelper.getCallCount());
    }

    /**
     * Verifies that Web Notifications and the Push API are not exposed in WebView.
     */
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testPushAndNotificationsDisabled() throws Throwable {
        AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();

        String script = "window.Notification || window.PushManager";

        enableJavaScriptOnUiThread(awContents);
        loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
        assertEquals("null", executeJavaScriptAndWaitForResult(awContents, mContentsClient,
                script));
    }

    private static class MockBindingManager implements BindingManager {
        private boolean mIsChildProcessCreated;

        boolean isChildProcessCreated() {
            return mIsChildProcessCreated;
        }

        @Override
        public void addNewConnection(int pid, ChildProcessConnection connection) {
            mIsChildProcessCreated = true;
        }

        @Override
        public void setInForeground(int pid, boolean inForeground) {}

        @Override
        public void determinedVisibility(int pid) {}

        @Override
        public void onSentToBackground() {}

        @Override
        public void onBroughtToForeground() {}

        @Override
        public boolean isOomProtected(int pid) {
            return false;
        }

        @Override
        public void clearConnection(int pid) {}

        @Override
        public void startModerateBindingManagement(
                Context context, int maxSize, boolean moderateBindingTillBackgrounded) {}

        @Override
        public void releaseAllModerateBindings() {}
    }

    /**
     * Verifies that a child process is actually gets created with WEBVIEW_SANDBOXED_RENDERER flag.
     */
    @Feature({"AndroidWebView"})
    @SmallTest
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_SANDBOXED_RENDERER)
    @ParameterizedTest.Set
    public void testSandboxedRendererWorks() throws Throwable {
        MockBindingManager bindingManager = new MockBindingManager();
        ChildProcessLauncher.setBindingManagerForTesting(bindingManager);
        assertFalse(bindingManager.isChildProcessCreated());

        AwTestContainerView testView = createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();
        final String pageTitle = "I am sandboxed";
        loadDataSync(awContents, mContentsClient.getOnPageFinishedHelper(),
                "<html><head><title>" + pageTitle + "</title></head></html>", "text/html", false);
        assertEquals(pageTitle, getTitleOnUiThread(awContents));

        assertTrue(bindingManager.isChildProcessCreated());

        // Test end-to-end interaction with the renderer.
        AwSettings awSettings = getAwSettingsOnUiThread(awContents);
        awSettings.setJavaScriptEnabled(true);
        assertEquals("42", JSUtils.executeJavaScriptAndWaitForResult(this, awContents,
                        mContentsClient.getOnEvaluateJavaScriptResultHelper(), "21 + 21"));
    }

}
