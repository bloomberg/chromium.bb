// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.Feature;

/**
 * Simple test for Brotli support.
 */
public class BrotliTest extends CronetTestBase {
    private CronetTestFramework mTestFramework;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        // Load library first to create MockCertVerifier.
        System.loadLibrary("cronet_tests");
        assertTrue(Http2TestServer.startHttp2TestServer(
                getContext(), QuicTestServer.getServerCert(), QuicTestServer.getServerCertKey()));
    }

    @Override
    protected void tearDown() throws Exception {
        assertTrue(Http2TestServer.shutdownHttp2TestServer());
        mTestFramework.mCronetEngine.shutdown();
        super.tearDown();
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testBrotliAdvertised() throws Exception {
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        builder.enableBrotli(true);
        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());
        mTestFramework = startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, builder);
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertTrue(callback.mResponseAsString.contains("accept-encoding: gzip, deflate, br"));
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testBrotliNotAdvertised() throws Exception {
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());
        mTestFramework = startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, builder);
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertFalse(callback.mResponseAsString.contains("br"));
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testBrotliDecoded() throws Exception {
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        builder.enableBrotli(true);
        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());
        mTestFramework = startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, builder);
        String url = Http2TestServer.getServeSimpleBrotliResponse();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        String expectedResponse = "The quick brown fox jumps over the lazy dog";
        assertEquals(expectedResponse, callback.mResponseAsString);
        assertEquals(callback.mResponseInfo.getAllHeaders().get("content-encoding").get(0), "br");
    }

    private TestUrlRequestCallback startAndWaitForComplete(String url) {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                url, callback, callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        return callback;
    }
}
