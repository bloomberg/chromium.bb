// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell.test;

import android.support.test.filters.SmallTest;
import android.test.ActivityInstrumentationTestCase2;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.WebStorage;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.chromium.webview_shell.WebViewThreadTestActivity;

/**
 * Tests running WebView on different threads.
 */
public class WebViewThreadTest extends ActivityInstrumentationTestCase2<WebViewThreadTestActivity> {
    private static final long TIMEOUT = scaleTimeout(4000);
    private static final String DATA = "<html><body>Testing<script>"
            + "console.log(\"testing\")</script></body></html>";
    private static final String URL_DATA = "javascript:console.log(\"testing\")";
    private WebViewThreadTestActivity mActivity;

    public WebViewThreadTest() {
        super(WebViewThreadTestActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = getActivity();
    }

    @Override
    protected void tearDown() throws Exception {
        mActivity.finish();
        super.tearDown();
    }

    /**
     * Create webview then loadData, on non-ui thread
     */
    @SmallTest
    public void testLoadDataNonUiThread() throws InterruptedException {
        assertTrue(mActivity.createWebViewOnNonUiThread(TIMEOUT));
        assertTrue(loadDataWebViewNonUiThread(DATA));
    }

    /**
     * Create webview then loadUrl, on non-ui thread
     */
    @SmallTest
    public void testLoadUrlNonUiThread() throws InterruptedException {
        assertTrue(mActivity.createWebViewOnNonUiThread(TIMEOUT));
        assertTrue(loadUrlWebViewNonUiThread(URL_DATA));
    }

    /**
     * Run getWebViewDatabase on a non-ui thread before creating webview on ui thread
     */
    @SmallTest
    public void testWebViewDatabaseBeforeCreateWebView() throws InterruptedException {
        mActivity.getWebViewDatabase();
        mActivity.createWebViewOnUiThread(TIMEOUT);
        assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /**
     * Create webview on ui-thread, then getWebViewDatabase on non-ui thread
     */
    @SmallTest
    public void testWebViewDatabaseAfterCreateWebView() throws InterruptedException {
        assertTrue(mActivity.createWebViewOnUiThread(TIMEOUT));
        mActivity.getWebViewDatabase();
        assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /**
     * Run CookieManager.getInstance on a non-ui thread before creating webview on ui thread
     */
    @SmallTest
    public void testCookieManagerBeforeCreateWebView() throws InterruptedException {
        CookieManager.getInstance();
        mActivity.createWebViewOnUiThread(TIMEOUT);
        assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /**
     * Create webview on ui-thread, then run CookieManager.getInstance on non-ui thread
     */
    @SmallTest
    public void testCookieManagerAfterCreateWebView() throws InterruptedException {
        assertTrue(mActivity.createWebViewOnUiThread(TIMEOUT));
        CookieManager.getInstance();
        assertTrue(loadDataWebViewInUiThread(DATA));
    }


    /**
     * Run GeolocationPermissions.getInstance on a non-ui thread before creating
     * webview on ui thread
     */
    @SmallTest
    public void testGeolocationPermissionsBeforeCreateWebView() throws InterruptedException {
        GeolocationPermissions.getInstance();
        mActivity.createWebViewOnUiThread(TIMEOUT);
        assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /**
     * Create webview on ui-thread, then run GeolocationPermissions.getInstance on non-ui thread
     */
    @SmallTest
    public void testGelolocationPermissionsAfterCreateWebView() throws InterruptedException {
        assertTrue(mActivity.createWebViewOnUiThread(TIMEOUT));
        GeolocationPermissions.getInstance();
        assertTrue(loadDataWebViewInUiThread(DATA));
    }


    /**
     * Run WebStorage.getInstance on a non-ui thread before creating webview on ui thread
     */
    @SmallTest
    public void testWebStorageBeforeCreateWebView() throws InterruptedException {
        WebStorage.getInstance();
        mActivity.createWebViewOnUiThread(TIMEOUT);
        assertTrue(loadDataWebViewInUiThread(DATA));
    }


    /**
     * Create webview on ui-thread, then run WebStorage.getInstance on non-ui thread
     */
    @SmallTest
    public void testWebStorageAfterCreateWebView() throws InterruptedException {
        assertTrue(mActivity.createWebViewOnUiThread(TIMEOUT));
        WebStorage.getInstance();
        assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /**
     * LoadData for webview created in non-ui thread
     */
    private boolean loadDataWebViewNonUiThread(final String data) throws InterruptedException {
        return mActivity.loadDataInNonUiThread(data, "text/html", null, TIMEOUT);
    }

    /**
     * LoadUrl for webview created in non-ui thread
     */
    private boolean loadUrlWebViewNonUiThread(final String url) throws InterruptedException {
        return mActivity.loadUrlInNonUiThread(url, TIMEOUT);
    }

    /**
     * LoadData for webview created in ui thread
     */
    private boolean loadDataWebViewInUiThread(final String data) throws InterruptedException {
        return mActivity.loadDataInUiThread(data, "text/html", null, TIMEOUT);
    }
}
