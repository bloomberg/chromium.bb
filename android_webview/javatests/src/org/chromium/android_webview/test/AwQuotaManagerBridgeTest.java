// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.test.suitebuilder.annotation.LargeTest;
import android.util.Pair;
import android.webkit.ValueCallback;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentSettings;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.List;

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

        ContentSettings settings = getContentSettingsOnUiThread(mAwContents);
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

    private AwQuotaManagerBridge getQuotaManagerBridge() throws Exception {
        return runTestOnUiThreadAndGetResult(new Callable<AwQuotaManagerBridge>() {
            @Override
            public AwQuotaManagerBridge call() throws Exception {
                return AwQuotaManagerBridge.getInstance();
            }
        });
    }

    private void deleteAllData() throws Exception {
        final AwQuotaManagerBridge bridge = getQuotaManagerBridge();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                bridge.deleteAllData();
            }
        });
    }

    private void deleteOrigin(final String origin) throws Exception {
        final AwQuotaManagerBridge bridge = getQuotaManagerBridge();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                bridge.deleteOrigin(origin);
            }
        });
    }

    private static class GetOriginsCallbackHelper extends CallbackHelper {
        private AwQuotaManagerBridge.Origins mOrigins;

        public void notifyCalled(AwQuotaManagerBridge.Origins origins) {
            mOrigins = origins;
            notifyCalled();
        }

        public AwQuotaManagerBridge.Origins getOrigins() {
            assert getCallCount() > 0;
            return mOrigins;
        }
    }

    private AwQuotaManagerBridge.Origins getOrigins() throws Exception {
        final GetOriginsCallbackHelper callbackHelper = new GetOriginsCallbackHelper();
        final AwQuotaManagerBridge bridge = getQuotaManagerBridge();

        int callCount = callbackHelper.getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                bridge.getOrigins(
                    new ValueCallback<AwQuotaManagerBridge.Origins>() {
                        @Override
                        public void onReceiveValue(AwQuotaManagerBridge.Origins origins) {
                            callbackHelper.notifyCalled(origins);
                        }
                    }
                );
            }
        });
        callbackHelper.waitForCallback(callCount);

        return callbackHelper.getOrigins();
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
        final AwQuotaManagerBridge bridge = getQuotaManagerBridge();

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
        final AwQuotaManagerBridge bridge = getQuotaManagerBridge();

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

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    crbug.com/180061
    */
    @DisabledTest
    public void testDeleteAllWithAppCache() throws Exception {
        long currentUsage = getUsageForOrigin(mOrigin);
        assertEquals(0, currentUsage);

        useAppCache();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return getUsageForOrigin(mOrigin) > 0;
                } catch (Exception e) {
                    return false;
                }
            }
        }));

        deleteAllData();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return getUsageForOrigin(mOrigin) == 0;
                } catch (Exception e) {
                    return false;
                }
            }
        }));
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    crbug.com/180061
    */
    @DisabledTest
    public void testDeleteOriginWithAppCache() throws Exception {
        long currentUsage = getUsageForOrigin(mOrigin);
        assertEquals(0, currentUsage);

        useAppCache();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return getUsageForOrigin(mOrigin) > 0;
                } catch (Exception e) {
                    return false;
                }
            }
        }));

        deleteOrigin(mOrigin);
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return getUsageForOrigin(mOrigin) == 0;
                } catch (Exception e) {
                    return false;
                }
            }
        }));
    }

    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    public void testGetResultsMatch() throws Exception {
        useAppCache();

        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return getOrigins().mOrigins.length > 0;
                } catch (Exception e) {
                    return false;
                }
            }
        });

        AwQuotaManagerBridge.Origins origins = getOrigins();
        assertEquals(origins.mOrigins.length, origins.mUsages.length);
        assertEquals(origins.mOrigins.length, origins.mQuotas.length);

        for (int i = 0; i < origins.mOrigins.length; ++i) {
            assertEquals(origins.mUsages[i], getUsageForOrigin(origins.mOrigins[i]));
            assertEquals(origins.mQuotas[i], getQuotaForOrigin(origins.mOrigins[i]));
        }
    }
}
