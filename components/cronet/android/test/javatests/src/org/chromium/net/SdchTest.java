// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/**
 * Tests Sdch support.
 */
public class SdchTest extends CronetTestBase {
    private CronetTestActivity mActivity;

    private void setUp(boolean enableSdch) {
        UrlRequestContextConfig config = new UrlRequestContextConfig();
        config.enableSDCH(enableSdch);
        config.setLibraryName("cronet_tests");
        config.enableHttpCache(UrlRequestContextConfig.HttpCache.IN_MEMORY, 100 * 1024);
        String[] commandLineArgs = {CronetTestActivity.CONFIG_KEY, config.toString()};
        mActivity = launchCronetTestAppWithUrlAndCommandLineArgs(null, commandLineArgs);
        mActivity.startNetLog();

        // Registers custom DNS mapping for legacy ChromiumUrlRequestFactory.
        ChromiumUrlRequestFactory factory = (ChromiumUrlRequestFactory) mActivity.mRequestFactory;
        long legacyAdapter = factory.getRequestContext().getUrlRequestContextAdapterForTesting();
        assertTrue(legacyAdapter != 0);
        NativeTestServer.registerHostResolverProc(legacyAdapter, true);

        // Registers custom DNS mapping for CronetUrlRequestContext.
        CronetUrlRequestContext requestContext =
                (CronetUrlRequestContext) mActivity.mUrlRequestContext;
        long adapter = requestContext.getUrlRequestContextAdapter();
        assertTrue(adapter != 0);
        NativeTestServer.registerHostResolverProc(adapter, false);

        // Starts NativeTestServer.
        assertTrue(NativeTestServer.startNativeTestServer(getInstrumentation().getTargetContext()));
    }

    @Override
    protected void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        mActivity.stopNetLog();
        super.tearDown();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSdchEnabled_LegacyAPI() throws Exception {
        setUp(true);
        // Make a request to /sdch/index which advertises the dictionary.
        TestHttpUrlRequestListener listener1 = startAndWaitForComplete_LegacyAPI(
                NativeTestServer.getSdchURL() + "/sdch/index?q=LeQxM80O");
        assertEquals(200, listener1.mHttpStatusCode);
        assertEquals("This is an index page.\n", listener1.mResponseAsString);
        assertEquals(Arrays.asList("/sdch/dict/LeQxM80O"),
                listener1.mResponseHeaders.get("Get-Dictionary"));

        waitForDictionaryAdded("LeQxM80O", true);

        // Make a request to fetch encoded response at /sdch/test.
        TestHttpUrlRequestListener listener2 =
                startAndWaitForComplete_LegacyAPI(NativeTestServer.getSdchURL() + "/sdch/test");
        assertEquals(200, listener2.mHttpStatusCode);
        assertEquals("The quick brown fox jumps over the lazy dog.\n", listener2.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSdchDisabled_LegacyAPI() throws Exception {
        setUp(false);
        // Make a request to /sdch/index.
        // Since Sdch is not enabled, no dictionary should be advertised.
        TestHttpUrlRequestListener listener = startAndWaitForComplete_LegacyAPI(
                NativeTestServer.getSdchURL() + "/sdch/index?q=LeQxM80O");
        assertEquals(200, listener.mHttpStatusCode);
        assertEquals("This is an index page.\n", listener.mResponseAsString);
        assertEquals(null, listener.mResponseHeaders.get("Get-Dictionary"));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testDictionaryNotFound_LegacyAPI() throws Exception {
        setUp(true);
        // Make a request to /sdch/index which advertises a bad dictionary that
        // does not exist.
        TestHttpUrlRequestListener listener1 = startAndWaitForComplete_LegacyAPI(
                NativeTestServer.getSdchURL() + "/sdch/index?q=NotFound");
        assertEquals(200, listener1.mHttpStatusCode);
        assertEquals("This is an index page.\n", listener1.mResponseAsString);
        assertEquals(Arrays.asList("/sdch/dict/NotFound"),
                listener1.mResponseHeaders.get("Get-Dictionary"));

        waitForDictionaryAdded("NotFound", true);

        // Make a request to fetch /sdch/test, and make sure Sdch encoding is not used.
        TestHttpUrlRequestListener listener2 =
                startAndWaitForComplete_LegacyAPI(NativeTestServer.getSdchURL() + "/sdch/test");
        assertEquals(200, listener2.mHttpStatusCode);
        assertEquals("Sdch is not used.\n", listener2.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSdchEnabled() throws Exception {
        setUp(true);
        // Make a request to /sdch which advertises the dictionary.
        TestUrlRequestListener listener1 =
                startAndWaitForComplete(NativeTestServer.getSdchURL() + "/sdch/index?q=LeQxM80O");
        assertEquals(200, listener1.mResponseInfo.getHttpStatusCode());
        assertEquals("This is an index page.\n", listener1.mResponseAsString);
        assertEquals(Arrays.asList("/sdch/dict/LeQxM80O"),
                listener1.mResponseInfo.getAllHeaders().get("Get-Dictionary"));

        waitForDictionaryAdded("LeQxM80O", false);

        // Make a request to fetch encoded response at /sdch/test.
        TestUrlRequestListener listener2 =
                startAndWaitForComplete(NativeTestServer.getSdchURL() + "/sdch/test");
        assertEquals(200, listener1.mResponseInfo.getHttpStatusCode());
        assertEquals("The quick brown fox jumps over the lazy dog.\n", listener2.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSdchDisabled() throws Exception {
        setUp(false);
        // Make a request to /sdch.
        // Since Sdch is not enabled, no dictionary should be advertised.
        TestUrlRequestListener listener =
                startAndWaitForComplete(NativeTestServer.getSdchURL() + "/sdch/index?q=LeQxM80O");
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("This is an index page.\n", listener.mResponseAsString);
        assertEquals(null, listener.mResponseInfo.getAllHeaders().get("Get-Dictionary"));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testDictionaryNotFound() throws Exception {
        setUp(true);
        // Make a request to /sdch/index which advertises a bad dictionary that
        // does not exist.
        TestUrlRequestListener listener1 =
                startAndWaitForComplete(NativeTestServer.getSdchURL() + "/sdch/index?q=NotFound");
        assertEquals(200, listener1.mResponseInfo.getHttpStatusCode());
        assertEquals("This is an index page.\n", listener1.mResponseAsString);
        assertEquals(Arrays.asList("/sdch/dict/NotFound"),
                listener1.mResponseInfo.getAllHeaders().get("Get-Dictionary"));

        waitForDictionaryAdded("NotFound", false);

        // Make a request to fetch /sdch/test, and make sure Sdch encoding is not used.
        TestUrlRequestListener listener2 =
                startAndWaitForComplete(NativeTestServer.getSdchURL() + "/sdch/test");
        assertEquals(200, listener2.mResponseInfo.getHttpStatusCode());
        assertEquals("Sdch is not used.\n", listener2.mResponseAsString);
    }

    /**
     * Helper method to wait for dictionary to be fetched and added to the list of available
     * dictionaries.
     */
    private void waitForDictionaryAdded(String dict, boolean isLegacyAPI) throws Exception {
        // Since Http cache is enabled, making a request to the dictionary explicitly will
        // be served from the cache.
        String url = NativeTestServer.getSdchURL() + "/sdch/dict/" + dict;
        if (isLegacyAPI) {
            TestHttpUrlRequestListener listener = startAndWaitForComplete_LegacyAPI(url);
            if (dict.equals("NotFound")) {
                assertEquals(404, listener.mHttpStatusCode);
            } else {
                assertEquals(200, listener.mHttpStatusCode);
                assertTrue(listener.mWasCached);
            }
        } else {
            TestUrlRequestListener listener = startAndWaitForComplete(url);
            if (dict.equals("NotFound")) {
                assertEquals(404, listener.mResponseInfo.getHttpStatusCode());
            } else {
                assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
                assertTrue(listener.mResponseInfo.wasCached());
            }
        }
        // Now wait for dictionary to be added to the manager, which occurs
        // asynchronously.
        // TODO(xunjieli): Rather than setting an arbitrary delay, consider
        // implementing a SdchObserver to watch for dictionary add events once
        // add event is implemented in SdchObserver.
        Thread.sleep(1000);
    }

    private TestHttpUrlRequestListener startAndWaitForComplete_LegacyAPI(String url)
            throws Exception {
        Map<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HttpUrlRequest request = mActivity.mRequestFactory.createRequest(
                url, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        request.start();
        listener.blockForComplete();
        return listener;
    }

    private TestUrlRequestListener startAndWaitForComplete(String url) throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest request =
                mActivity.mUrlRequestContext.createRequest(url, listener, listener.getExecutor());
        request.start();
        listener.blockForDone();
        return listener;
    }
}
