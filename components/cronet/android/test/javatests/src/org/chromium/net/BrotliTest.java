// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.chromium.net.CronetTestRule.SERVER_CERT_PEM;
import static org.chromium.net.CronetTestRule.SERVER_KEY_PKCS8_PEM;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.CronetTestRule.RequiresMinApi;

/**
 * Simple test for Brotli support.
 */
@RequiresMinApi(5) // Brotli support added in API version 5: crrev.com/465216
public class BrotliTest extends CronetTestBase {
    private CronetEngine mCronetEngine;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        TestFilesInstaller.installIfNeeded(getContext());
        assertTrue(Http2TestServer.startHttp2TestServer(
                getContext(), SERVER_CERT_PEM, SERVER_KEY_PKCS8_PEM));
    }

    @Override
    protected void tearDown() throws Exception {
        assertTrue(Http2TestServer.shutdownHttp2TestServer());
        if (mCronetEngine != null) {
            mCronetEngine.shutdown();
        }
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
        mCronetEngine = builder.build();
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
        mCronetEngine = builder.build();
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
        mCronetEngine = builder.build();
        String url = Http2TestServer.getServeSimpleBrotliResponse();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        String expectedResponse = "The quick brown fox jumps over the lazy dog";
        assertEquals(expectedResponse, callback.mResponseAsString);
        assertEquals(callback.mResponseInfo.getAllHeaders().get("content-encoding").get(0), "br");
    }

    private TestUrlRequestCallback startAndWaitForComplete(String url) {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        return callback;
    }
}
