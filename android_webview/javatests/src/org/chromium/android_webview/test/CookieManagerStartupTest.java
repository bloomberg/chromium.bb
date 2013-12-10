// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.content.app.ContentMain;
import org.chromium.net.test.util.TestWebServer;

/**
 * Test for the CookieManager in the case where it's used before Chromium is started.
 */
public class CookieManagerStartupTest extends AwTestBase {

    private AwCookieManager mCookieManager;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mCookieManager = new AwCookieManager();
        assertNotNull(mCookieManager);

        ContentMain.initApplicationContext(getActivity().getApplicationContext());
    }

    @Override
    protected boolean needsBrowserProcessStarted() {
        return false;
    }

    private void startChromium() throws Exception {
        final Context context = getActivity();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                AwBrowserProcess.start(context);
            }
        });

        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mAwContents.getSettings().setJavaScriptEnabled(true);
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testStartup() throws Throwable {
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            String path = "/cookie_test.html";
            String url = webServer.setResponse(path, CommonResources.ABOUT_HTML, null);

            mCookieManager.setAcceptCookie(true);
            mCookieManager.removeAllCookie();
            assertTrue(mCookieManager.acceptCookie());
            assertFalse(mCookieManager.hasCookies());
            mCookieManager.setCookie(url, "count=41");

            startChromium();
            loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            executeJavaScriptAndWaitForResult(
                    mAwContents,
                    mContentsClient,
                    "var c=document.cookie.split('=');document.cookie=c[0]+'='+(1+(+c[1]));");

            assertEquals("count=42", mCookieManager.getCookie(url));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

}
