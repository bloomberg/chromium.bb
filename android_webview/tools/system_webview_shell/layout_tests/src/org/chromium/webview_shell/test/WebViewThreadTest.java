// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell.test;

import android.test.ActivityInstrumentationTestCase2;
import android.test.suitebuilder.annotation.SmallTest;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.WebStorage;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.chromium.webview_shell.WebViewThreadTestActivity;

/**
 * Tests running WebView on different threads.
 */
public class WebViewThreadTest extends ActivityInstrumentationTestCase2<WebViewThreadTestActivity> {
    private static final long TIMEOUT = scaleTimeout(2000);
    private static final String DATA = "<html><body>Testing<script>"
            + "console.log(\"testing\")</script></body></html>";
    private static final String URL_DATA = "javascript:console.log(\"testing\")";
    private WebViewThreadTestActivity mActivity;
    private InterruptedException mException;

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

    @SmallTest
    public void testLoadDataNonUiThread() throws InterruptedException {
        assertTrue(mActivity.createWebViewOnNonUiThread(TIMEOUT));
        assertTrue(loadDataWebViewNonUiThread(DATA));
    }

    @SmallTest
    public void testLoadUrlNonUiThread() throws InterruptedException {
        assertTrue(mActivity.createWebViewOnNonUiThread(TIMEOUT));
        assertTrue(loadUrlWebViewNonUiThread(URL_DATA));
    }

    @SmallTest
    public void testWebViewInitByDatabase() throws InterruptedException {
        initThenCreateWebViewOnUiThread(new Runnable() {
            @Override
            public void run() {
                mActivity.getWebViewDatabase();
            }
        });
    }

    @SmallTest
    public void testWebViewInitByCookieManager() throws InterruptedException {
        initThenCreateWebViewOnUiThread(new Runnable() {
            @Override
            public void run() {
                CookieManager.getInstance();
            }
        });
    }

    @SmallTest
    public void testWebViewInitByGeolocationPermissions() throws InterruptedException {
        initThenCreateWebViewOnUiThread(new Runnable() {
            @Override
            public void run() {
                GeolocationPermissions.getInstance();
            }
        });
    }

    @SmallTest
    public void testWebViewInitByWebStorage() throws InterruptedException {
        initThenCreateWebViewOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebStorage.getInstance();
            }
        });
    }

    private void initThenCreateWebViewOnUiThread(final Runnable runnable)
            throws InterruptedException {
        mException = null;
        Thread webviewCreateThread = new Thread(new Runnable() {
            @Override
            public void run() {
                runnable.run();
                try {
                    mActivity.createWebViewOnUiThread(TIMEOUT);
                } catch (InterruptedException e) {
                    mException = e;
                }
            }
        });
        webviewCreateThread.start();
        webviewCreateThread.join();
        if (mException != null) throw mException;
        assertTrue(loadDataWebViewInUiThread(DATA));
    }

    private boolean loadDataWebViewNonUiThread(final String data) throws InterruptedException {
        return mActivity.loadDataInNonUiThread(data, "text/html", null, TIMEOUT);
    }
    private boolean loadUrlWebViewNonUiThread(final String url) throws InterruptedException {
        return mActivity.loadUrlInNonUiThread(url, TIMEOUT);
    }
    private boolean loadDataWebViewInUiThread(final String data) throws InterruptedException {
        return mActivity.loadDataInUiThread(data, "text/html", null, TIMEOUT);
    }
}
