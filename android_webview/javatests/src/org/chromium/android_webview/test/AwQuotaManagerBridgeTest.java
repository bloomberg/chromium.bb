// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.LargeTest;
import android.util.Pair;
import android.webkit.ValueCallback;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.AwQuotaManagerBridgeTestUtil;
import org.chromium.base.test.util.Feature;
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
        mWebServer = new TestWebServer(false);
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
                    }
                );
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
                    }
                );
            }
        });
        callbackHelper.waitForCallback(callCount);

        return callbackHelper.getValue();
    }

    private void useAppCache() throws Exception {
        final String CACHED_FILE_PATH = "/foo.js";
        final String CACHED_FILE_CONTENTS = "1 + 1;";
        mWebServer.setResponse(CACHED_FILE_PATH, CACHED_FILE_CONTENTS, null);

        final String MANIFEST_PATH = "/foo.manifest";
        final String MANIFEST_CONTENTS = "CACHE MANIFEST\nCACHE:\n" + CACHED_FILE_PATH;
        List<Pair<String, String>> manifestHeaders = new ArrayList<Pair<String, String>>();
        manifestHeaders.add(Pair.create("Content-Disposition", "text/cache-manifest"));
        mWebServer.setResponse(MANIFEST_PATH, MANIFEST_CONTENTS, manifestHeaders);

        final String PAGE_PATH = "/appcache.html";
        final String PAGE_CONTENTS = "<html manifest=\"" + MANIFEST_PATH + "\">" +
                "<head><script src=\"" + CACHED_FILE_PATH + "\"></script></head></html>";
        String url = mWebServer.setResponse(PAGE_PATH, PAGE_CONTENTS, null);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        executeJavaScriptAndWaitForResult(mAwContents, mContentsClient,
              "window.applicationCache.update();");
    }

    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    public void testDeleteAllWithAppCache() throws Exception {
        final long initialUsage = getUsageForOrigin(mOrigin);

        useAppCache();
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getUsageForOrigin(mOrigin) > initialUsage;
            }
        });

        deleteAllData();
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getUsageForOrigin(mOrigin) == 0;
            }
        });
    }

    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    public void testDeleteOriginWithAppCache() throws Exception {
        final long initialUsage = getUsageForOrigin(mOrigin);

        useAppCache();
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getUsageForOrigin(mOrigin) > initialUsage;
            }
        });

        deleteOrigin(mOrigin);
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getUsageForOrigin(mOrigin) == 0;
            }
        });
    }

    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    public void testGetResultsMatch() throws Exception {
        useAppCache();

        poll(new Callable<Boolean>() {
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
