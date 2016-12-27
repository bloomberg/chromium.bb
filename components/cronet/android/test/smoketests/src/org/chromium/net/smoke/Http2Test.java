// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import android.support.test.filters.SmallTest;

import org.chromium.net.UrlRequest;

/**
 * HTTP2 Tests.
 */
public class Http2Test extends NativeCronetTestCase {
    private TestSupport.TestServer mServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mServer = mTestSupport.createTestServer(getContext(), TestSupport.Protocol.HTTP2);
    }

    @Override
    protected void tearDown() throws Exception {
        mServer.shutdown();
        super.tearDown();
    }

    // Test that HTTP/2 is enabled by default but QUIC is not.
    @SmallTest
    public void testHttp2() throws Exception {
        mTestSupport.installMockCertVerifierForTesting(mCronetEngineBuilder);
        initCronetEngine();
        assertTrue(mServer.start());
        SmokeTestRequestCallback callback = new SmokeTestRequestCallback();
        UrlRequest.Builder requestBuilder = mCronetEngine.newUrlRequestBuilder(
                mServer.getSuccessURL(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertSuccessfulNonEmptyResponse(callback, mServer.getSuccessURL());
        assertEquals("h2", callback.getResponseInfo().getNegotiatedProtocol());
    }
}
