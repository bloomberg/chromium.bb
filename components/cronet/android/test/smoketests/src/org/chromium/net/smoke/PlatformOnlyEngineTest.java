// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import android.support.test.filters.SmallTest;

import org.chromium.net.UrlRequest;

/**
 * Tests scenario when an app doesn't contain the native Cronet implementation.
 */
public class PlatformOnlyEngineTest extends CronetSmokeTestCase {
    private String mURL;
    private TestSupport.TestServer mServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        // Java-only implementation of the Cronet engine only supports Http/1 protocol.
        mServer = mTestSupport.createTestServer(getContext(), TestSupport.Protocol.HTTP1);
        assertTrue(mServer.start());
        mURL = mServer.getSuccessURL();
    }

    @Override
    protected void tearDown() throws Exception {
        mServer.shutdown();
        super.tearDown();
    }

    /**
     * Test a successful response when a request is sent by the Java Cronet Engine.
     */
    @SmallTest
    public void testSuccessfulResponse() {
        initCronetEngine();
        assertJavaEngine(mCronetEngine);
        SmokeTestRequestCallback callback = new SmokeTestRequestCallback();
        UrlRequest.Builder requestBuilder =
                mCronetEngine.newUrlRequestBuilder(mURL, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();
        assertSuccessfulNonEmptyResponse(callback, mURL);
    }
}
