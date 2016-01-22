// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests Service Worker Client related APIs.
 */
public class AwServiceWorkerClientTest extends AwTestBase {

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private AwTestContainerView mTestContainerView;
    private TestAwServiceWorkerClient mServiceWorkerClient;

    private static final String INDEX_HTML =
            "<!DOCTYPE html>\n"
            + "<html>\n"
            + "  <body>\n"
            + "    <script>\n"
            + "      success = 0;\n"
            + "      navigator.serviceWorker.register('sw.js').then(function(reg) {;\n"
            + "         success = 1;\n"
            + "      }).catch(function(err) { \n"
            + "         console.error(err);\n"
            + "      });\n"
            + "    </script>\n"
            + "  </body>\n"
            + "</html>\n";

    private static final String SW_HTML = "fetch('fetch.html');";
    private static final String FETCH_HTML = ";)";

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mWebServer = TestWebServer.start();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mServiceWorkerClient = new TestAwServiceWorkerClient();
        getAwBrowserContext().getServiceWorkerController()
                .setServiceWorkerClient(mServiceWorkerClient);
        mAwContents = mTestContainerView.getAwContents();
        enableJavaScriptOnUiThread(mAwContents);
    }

    @Override
    protected void tearDown() throws Exception {
        if (mWebServer != null) mWebServer.shutdown();
        super.tearDown();
    }

    @SmallTest
    public void testInvokeInterceptCallback() throws Throwable {
        final String fullIndexUrl = mWebServer.setResponse("/index.html", INDEX_HTML, null);
        final String fullSwUrl = mWebServer.setResponse("/sw.js", SW_HTML, null);
        final String fullFetchUrl = mWebServer.setResponse("/fetch.html", FETCH_HTML, null);

        TestAwServiceWorkerClient.ShouldInterceptRequestHelper helper =
                mServiceWorkerClient.getShouldInterceptRequestHelper();
        int currentShouldInterceptRequestCount = helper.getCallCount();

        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        loadUrlSync(mAwContents, onPageFinishedHelper, fullIndexUrl);
        assertEquals(fullIndexUrl, onPageFinishedHelper.getUrl());

        // Check that the service worker has been registered successfully.
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getSuccessFromJS() == 1;
            }
        });

        helper.waitForCallback(currentShouldInterceptRequestCount, 2);

        // Check that the two service worker related callbacks were correctly intercepted.
        List<AwWebResourceRequest> requests = helper.getAwWebResourceRequests();
        assertEquals(2, requests.size());
        assertEquals(fullSwUrl, requests.get(0).url);
        assertEquals(fullFetchUrl, requests.get(1).url);
    }

    private int getSuccessFromJS() {
        int result = -1;
        try {
            result = Integer.parseInt(executeJavaScriptAndWaitForResult(
                    mAwContents, mContentsClient, "success"));
        } catch (Exception e) {
            fail("Unable to get success");
        }
        return result;
    }
}
