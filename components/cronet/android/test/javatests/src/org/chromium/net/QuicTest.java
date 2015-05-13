// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

import java.util.HashMap;

/**
 * Tests making requests using QUIC.
 */
public class QuicTest extends CronetTestBase {

    private CronetTestActivity mActivity;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        // Load library first, since we need the Quic test server's URL.
        System.loadLibrary("cronet_tests");
        QuicTestServer.startQuicTestServer(getInstrumentation().getTargetContext());
        UrlRequestContextConfig config = new UrlRequestContextConfig();
        config.enableQUIC(true);
        config.addQuicHint(QuicTestServer.getServerHost(), QuicTestServer.getServerPort(),
                QuicTestServer.getServerPort());
        config.setExperimentalQuicConnectionOptions("PACE,IW10,FOO,DEADBEEF");

        String[] commandLineArgs = {CronetTestActivity.CONFIG_KEY, config.toString(),
                CronetTestActivity.CACHE_KEY, CronetTestActivity.CACHE_DISK_NO_HTTP};
        mActivity = launchCronetTestAppWithUrlAndCommandLineArgs(null, commandLineArgs);
    }

    @Override
    protected void tearDown() throws Exception {
        QuicTestServer.shutdownQuicTestServer();
        super.tearDown();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testQuicLoadUrl_LegacyAPI() throws Exception {
        String quicURL = QuicTestServer.getServerURL() + "/simple.txt";

        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();

        // Although the native stack races QUIC and SPDY for the first request,
        // since there is no http server running on the corresponding TCP port,
        // QUIC will always succeed with a 200 (see
        // net::HttpStreamFactoryImpl::Request::OnStreamFailed).
        HttpUrlRequest request = mActivity.mRequestFactory.createRequest(
                quicURL, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        request.start();
        listener.blockForComplete();
        assertEquals(200, listener.mHttpStatusCode);
        assertEquals(
                "This is a simple text file served by QUIC.\n",
                listener.mResponseAsString);
        assertEquals("quic/1+spdy/3", listener.mNegotiatedProtocol);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testQuicLoadUrl() throws Exception {
        String quicURL = QuicTestServer.getServerURL() + "/simple.txt";
        TestUrlRequestListener listener = new TestUrlRequestListener();

        // Although the native stack races QUIC and SPDY for the first request,
        // since there is no http server running on the corresponding TCP port,
        // QUIC will always succeed with a 200 (see
        // net::HttpStreamFactoryImpl::Request::OnStreamFailed).
        UrlRequest request = mActivity.mUrlRequestContext.createRequest(
                quicURL, listener, listener.getExecutor());
        request.start();
        listener.blockForDone();

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("This is a simple text file served by QUIC.\n", listener.mResponseAsString);
        assertEquals("quic/1+spdy/3", listener.mResponseInfo.getNegotiatedProtocol());
    }
}
