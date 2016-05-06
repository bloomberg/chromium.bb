// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.util.Pair;
import android.webkit.ValueCallback;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.AwQuotaManagerBridgeTestUtil;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests for the AwQuotaManagerBridge.
 */
public class AwQuotaManagerBridgeTest extends AwTestBase {
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private String mOrigin;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mTestView = createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestView.getAwContents();
        mWebServer = TestWebServer.start();
        mOrigin = mWebServer.getBaseUrl();

        AwSettings settings = getAwSettingsOnUiThread(mAwContents);
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAppCacheEnabled(true);
        settings.setAppCachePath("whatever");  // Enables AppCache.
    }

    @Override
    public void tearDown() throws Exception {
        deleteAllData();
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
        super.tearDown();
    }

    private void deleteAllData() throws Exception {
        final AwQuotaManagerBridge bridge =
                AwQuotaManagerBridgeTestUtil.getQuotaManagerBridge(this);
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                bridge.deleteAllData();
            }
        });
    }

    private void deleteOrigin(final String origin) throws Exception {
        final AwQuotaManagerBridge bridge =
                AwQuotaManagerBridgeTestUtil.getQuotaManagerBridge(this);
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                bridge.deleteOrigin(origin);
            }
        });
    }

    private static class LongValueCallbackHelper extends CallbackHelper {
        private long mValue;

        public void notifyCalled(long value) {
            mValue = value;
            notifyCalled();
        }

        public long getValue() {
            assert getCallCount() > 0;
            return mValue;
        }
    }

    private long getQuotaForOrigin(final String origin) throws Exception {
        final LongValueCallbackHelper callbackHelper = new LongValueCallbackHelper();
        final AwQuotaManagerBridge bridge =
                AwQuotaManagerBridgeTestUtil.getQuotaManagerBridge(this);

        int callCount = callbackHelper.getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                bridge.getQuotaForOrigin("foo.com",
                        new ValueCallback<Long>() {
                            @Override
                            public void onReceiveValue(Long quota) {
                                callbackHelper.notifyCalled(quota);
                            }
                        });
            }
        });
        callbackHelper.waitForCallback(callCount);

        return callbackHelper.getValue();
    }

    private long getUsageForOrigin(final String origin) throws Exception {
        final LongValueCallbackHelper callbackHelper = new LongValueCallbackHelper();
        final AwQuotaManagerBridge bridge =
                AwQuotaManagerBridgeTestUtil.getQuotaManagerBridge(this);

        int callCount = callbackHelper.getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                bridge.getUsageForOrigin(origin,
                        new ValueCallback<Long>() {
                            @Override
                            public void onReceiveValue(Long usage) {
                                callbackHelper.notifyCalled(usage);
                            }
                        });
            }
        });
        callbackHelper.waitForCallback(callCount);

        return callbackHelper.getValue();
    }

    private void useAppCache() throws Exception {
        final String cachedFilePath = "/foo.js";
        final String cachedFileContents = "1 + 1;";
        mWebServer.setResponse(cachedFilePath, cachedFileContents, null);

        final String manifestPath = "/foo.manifest";
        final String manifestContents = "CACHE MANIFEST\nCACHE:\n" + cachedFilePath;
        List<Pair<String, String>> manifestHeaders = new ArrayList<Pair<String, String>>();
        manifestHeaders.add(Pair.create("Content-Disposition", "text/cache-manifest"));
        mWebServer.setResponse(manifestPath, manifestContents, manifestHeaders);

        final String pagePath = "/appcache.html";
        final String pageContents = "<html manifest=\"" + manifestPath + "\">"
                + "<head><script src=\"" + cachedFilePath + "\"></script></head></html>";
        String url = mWebServer.setResponse(pagePath, pageContents, null);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        executeJavaScriptAndWaitForResult(mAwContents, mContentsClient,
                "window.applicationCache.update();");
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    Too flaky. See crbug.com/609977.
    */
    @DisabledTest
    public void testDeleteAllWithAppCache() throws Exception {
        final long initialUsage = getUsageForOrigin(mOrigin);

        useAppCache();
        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getUsageForOrigin(mOrigin) > initialUsage;
            }
        });

        deleteAllData();
        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getUsageForOrigin(mOrigin) == 0;
            }
        });
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    Too flaky. See crbug.com/609977.
    */
    @DisabledTest
    public void testDeleteOriginWithAppCache() throws Exception {
        final long initialUsage = getUsageForOrigin(mOrigin);

        useAppCache();
        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getUsageForOrigin(mOrigin) > initialUsage;
            }
        });

        deleteOrigin(mOrigin);
        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getUsageForOrigin(mOrigin) == 0;
            }
        });
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    Too flaky. See crbug.com/609977.
    */
    @DisabledTest
    public void testGetResultsMatch() throws Exception {
        useAppCache();

        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return AwQuotaManagerBridgeTestUtil.getOrigins(
                            AwQuotaManagerBridgeTest.this).mOrigins.length > 0;
            }
        });

        AwQuotaManagerBridge.Origins origins = AwQuotaManagerBridgeTestUtil.getOrigins(this);
        assertEquals(origins.mOrigins.length, origins.mUsages.length);
        assertEquals(origins.mOrigins.length, origins.mQuotas.length);

        for (int i = 0; i < origins.mOrigins.length; ++i) {
            assertEquals(origins.mUsages[i], getUsageForOrigin(origins.mOrigins[i]));
            assertEquals(origins.mQuotas[i], getQuotaForOrigin(origins.mOrigins[i]));
        }
    }
}
