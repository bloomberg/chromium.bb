// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.net.CronetTestRule.getContext;
import static org.chromium.net.CronetTestRule.getTestStorage;

import android.support.test.filters.SmallTest;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.impl.CronetUrlRequestContext;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.Arrays;

/**
 * Tests Sdch support.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SdchTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private enum Sdch {
        ENABLED,
        DISABLED,
    }

    private CronetEngine.Builder createCronetEngineBuilder(Sdch setting) throws JSONException {
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        builder.enableSdch(setting == Sdch.ENABLED);
        mTestRule.enableDiskCache(builder);
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions =
                new JSONObject().put("HostResolverRules", hostResolverParams);
        builder.setExperimentalOptions(experimentalOptions.toString());
        // Start NativeTestServer.
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
        return builder;
    }

    @After
    public void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testSdchEnabled() throws Exception {
        CronetEngine.Builder cronetEngineBuilder = createCronetEngineBuilder(Sdch.ENABLED);
        CronetEngine cronetEngine = cronetEngineBuilder.build();
        String targetUrl = NativeTestServer.getSdchURL() + "/sdch/test";
        long contextAdapter = getContextAdapter((CronetUrlRequestContext) cronetEngine);
        SdchObserver observer = new SdchObserver(targetUrl, contextAdapter);

        // Make a request to /sdch which advertises the dictionary.
        TestUrlRequestCallback callback1 = startAndWaitForComplete(
                cronetEngine, NativeTestServer.getSdchURL() + "/sdch/index?q=LeQxM80O");
        assertEquals(200, callback1.mResponseInfo.getHttpStatusCode());
        assertEquals("This is an index page.\n", callback1.mResponseAsString);
        assertEquals(Arrays.asList("/sdch/dict/LeQxM80O"),
                callback1.mResponseInfo.getAllHeaders().get("Get-Dictionary"));

        observer.waitForDictionaryAdded();

        // Make a request to fetch encoded response at /sdch/test.
        TestUrlRequestCallback callback2 = startAndWaitForComplete(cronetEngine, targetUrl);
        assertEquals(200, callback2.mResponseInfo.getHttpStatusCode());
        assertEquals("The quick brown fox jumps over the lazy dog.\n", callback2.mResponseAsString);

        cronetEngine.shutdown();

        // Shutting down the context will make JsonPrefStore to flush pending
        // writes to disk.
        String dictUrl = NativeTestServer.getSdchURL() + "/sdch/dict/LeQxM80O";
        assertTrue(fileContainsString("local_prefs.json", dictUrl));

        // Test persistence.
        cronetEngine = cronetEngineBuilder.build();
        CronetUrlRequestContext newContext = (CronetUrlRequestContext) cronetEngine;
        long newContextAdapter = getContextAdapter(newContext);
        SdchObserver newObserver = new SdchObserver(targetUrl, newContextAdapter);
        newObserver.waitForDictionaryAdded();

        // Make a request to fetch encoded response at /sdch/test.
        TestUrlRequestCallback callback3 = startAndWaitForComplete(cronetEngine, targetUrl);
        assertEquals(200, callback3.mResponseInfo.getHttpStatusCode());
        assertEquals("The quick brown fox jumps over the lazy dog.\n", callback3.mResponseAsString);
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testSdchDisabled() throws Exception {
        CronetEngine cronetEngine = createCronetEngineBuilder(Sdch.DISABLED).build();
        // Make a request to /sdch.
        // Since Sdch is not enabled, no dictionary should be advertised.
        TestUrlRequestCallback callback = startAndWaitForComplete(
                cronetEngine, NativeTestServer.getSdchURL() + "/sdch/index?q=LeQxM80O");
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("This is an index page.\n", callback.mResponseAsString);
        assertEquals(null, callback.mResponseInfo.getAllHeaders().get("Get-Dictionary"));
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testDictionaryNotFound() throws Exception {
        CronetEngine cronetEngine = createCronetEngineBuilder(Sdch.ENABLED).build();
        // Make a request to /sdch/index which advertises a bad dictionary that
        // does not exist.
        TestUrlRequestCallback callback1 = startAndWaitForComplete(
                cronetEngine, NativeTestServer.getSdchURL() + "/sdch/index?q=NotFound");
        assertEquals(200, callback1.mResponseInfo.getHttpStatusCode());
        assertEquals("This is an index page.\n", callback1.mResponseAsString);
        assertEquals(Arrays.asList("/sdch/dict/NotFound"),
                callback1.mResponseInfo.getAllHeaders().get("Get-Dictionary"));

        // Make a request to fetch /sdch/test, and make sure Sdch encoding is not used.
        TestUrlRequestCallback callback2 =
                startAndWaitForComplete(cronetEngine, NativeTestServer.getSdchURL() + "/sdch/test");
        assertEquals(200, callback2.mResponseInfo.getHttpStatusCode());
        assertEquals("Sdch is not used.\n", callback2.mResponseAsString);
        cronetEngine.shutdown();
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
                getTestStorage(getContext()) + "/prefs/" + filename));
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
