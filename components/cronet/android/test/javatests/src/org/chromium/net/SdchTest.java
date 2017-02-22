// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.support.test.filters.SmallTest;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.test.util.Feature;
import org.chromium.net.impl.CronetUrlRequestContext;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests Sdch support.
 */
public class SdchTest extends CronetTestBase {
    private CronetTestFramework mTestFramework;

    private enum Sdch {
        ENABLED,
        DISABLED,
    }

    @SuppressWarnings("deprecation")
    private void setUp(Sdch setting) throws JSONException {
        List<String> commandLineArgs = new ArrayList<String>();
        commandLineArgs.add(CronetTestFramework.CACHE_KEY);
        commandLineArgs.add(CronetTestFramework.CACHE_DISK);
        if (setting == Sdch.ENABLED) {
            commandLineArgs.add(CronetTestFramework.SDCH_KEY);
            commandLineArgs.add(CronetTestFramework.SDCH_ENABLE);
        }

        commandLineArgs.add(CronetTestFramework.LIBRARY_INIT_KEY);
        commandLineArgs.add(CronetTestFramework.LibraryInitType.CRONET);

        String[] args = new String[commandLineArgs.size()];
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions =
                new JSONObject().put("HostResolverRules", hostResolverParams);
        builder.setExperimentalOptions(experimentalOptions.toString());
        mTestFramework =
                new CronetTestFramework(null, commandLineArgs.toArray(args), getContext(), builder);
        // Start NativeTestServer.
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
    }

    @Override
    protected void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        super.tearDown();
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testSdchEnabled() throws Exception {
        setUp(Sdch.ENABLED);
        String targetUrl = NativeTestServer.getSdchURL() + "/sdch/test";
        long contextAdapter =
                getContextAdapter((CronetUrlRequestContext) mTestFramework.mCronetEngine);
        SdchObserver observer = new SdchObserver(targetUrl, contextAdapter);

        // Make a request to /sdch which advertises the dictionary.
        TestUrlRequestCallback callback1 = startAndWaitForComplete(mTestFramework.mCronetEngine,
                NativeTestServer.getSdchURL() + "/sdch/index?q=LeQxM80O");
        assertEquals(200, callback1.mResponseInfo.getHttpStatusCode());
        assertEquals("This is an index page.\n", callback1.mResponseAsString);
        assertEquals(Arrays.asList("/sdch/dict/LeQxM80O"),
                callback1.mResponseInfo.getAllHeaders().get("Get-Dictionary"));

        observer.waitForDictionaryAdded();

        // Make a request to fetch encoded response at /sdch/test.
        TestUrlRequestCallback callback2 =
                startAndWaitForComplete(mTestFramework.mCronetEngine, targetUrl);
        assertEquals(200, callback2.mResponseInfo.getHttpStatusCode());
        assertEquals("The quick brown fox jumps over the lazy dog.\n", callback2.mResponseAsString);

        mTestFramework.mCronetEngine.shutdown();

        // Shutting down the context will make JsonPrefStore to flush pending
        // writes to disk.
        String dictUrl = NativeTestServer.getSdchURL() + "/sdch/dict/LeQxM80O";
        assertTrue(fileContainsString("local_prefs.json", dictUrl));

        // Test persistence.
        mTestFramework = startCronetTestFrameworkWithUrlAndCronetEngineBuilder(
                null, mTestFramework.getCronetEngineBuilder());
        CronetUrlRequestContext newContext = (CronetUrlRequestContext) mTestFramework.mCronetEngine;
        long newContextAdapter = getContextAdapter(newContext);
        SdchObserver newObserver = new SdchObserver(targetUrl, newContextAdapter);
        newObserver.waitForDictionaryAdded();

        // Make a request to fetch encoded response at /sdch/test.
        TestUrlRequestCallback callback3 = startAndWaitForComplete(newContext, targetUrl);
        assertEquals(200, callback3.mResponseInfo.getHttpStatusCode());
        assertEquals("The quick brown fox jumps over the lazy dog.\n", callback3.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testSdchDisabled() throws Exception {
        setUp(Sdch.DISABLED);
        // Make a request to /sdch.
        // Since Sdch is not enabled, no dictionary should be advertised.
        TestUrlRequestCallback callback = startAndWaitForComplete(mTestFramework.mCronetEngine,
                NativeTestServer.getSdchURL() + "/sdch/index?q=LeQxM80O");
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("This is an index page.\n", callback.mResponseAsString);
        assertEquals(null, callback.mResponseInfo.getAllHeaders().get("Get-Dictionary"));
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testDictionaryNotFound() throws Exception {
        setUp(Sdch.ENABLED);
        // Make a request to /sdch/index which advertises a bad dictionary that
        // does not exist.
        TestUrlRequestCallback callback1 = startAndWaitForComplete(mTestFramework.mCronetEngine,
                NativeTestServer.getSdchURL() + "/sdch/index?q=NotFound");
        assertEquals(200, callback1.mResponseInfo.getHttpStatusCode());
        assertEquals("This is an index page.\n", callback1.mResponseAsString);
        assertEquals(Arrays.asList("/sdch/dict/NotFound"),
                callback1.mResponseInfo.getAllHeaders().get("Get-Dictionary"));

        // Make a request to fetch /sdch/test, and make sure Sdch encoding is not used.
        TestUrlRequestCallback callback2 = startAndWaitForComplete(
                mTestFramework.mCronetEngine, NativeTestServer.getSdchURL() + "/sdch/test");
        assertEquals(200, callback2.mResponseInfo.getHttpStatusCode());
        assertEquals("Sdch is not used.\n", callback2.mResponseAsString);
    }

    private long getContextAdapter(CronetUrlRequestContext requestContext) {
        return requestContext.getUrlRequestContextAdapter();
    }

    private TestUrlRequestCallback startAndWaitForComplete(CronetEngine cronetEngine, String url)
            throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        return callback;
    }

    // Returns whether a file contains a particular string.
    private boolean fileContainsString(String filename, String content) throws IOException {
        BufferedReader reader = new BufferedReader(new FileReader(
                CronetTestFramework.getTestStorage(getContext()) + "/prefs/" + filename));
        String line;
        while ((line = reader.readLine()) != null) {
            if (line.contains(content)) {
                reader.close();
                return true;
            }
        }
        reader.close();
        return false;
    }
}
