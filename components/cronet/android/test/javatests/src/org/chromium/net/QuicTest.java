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

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        // Loads library first since native functions are used to retrieve
        // QuicTestServer info before config is constructed.
        System.loadLibrary("cronet_tests");
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testQuicLoadUrl() throws Exception {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        String quicURL = QuicTestServer.getServerURL() + "/simple.txt";
        config.enableQUIC(true);
        config.setLibraryName("cronet_tests");
        config.addQuicHint(QuicTestServer.getServerHost(),
                QuicTestServer.getServerPort(), QuicTestServer.getServerPort());
        config.setExperimentalQuicConnectionOptions("PACE,IW10,FOO,DEADBEEF");

        String[] commandLineArgs = {
                CronetTestActivity.CONFIG_KEY, config.toString() };
        CronetTestActivity activity =
                launchCronetTestAppWithUrlAndCommandLineArgs(null,
                                                             commandLineArgs);
        QuicTestServer.startQuicTestServer(activity.getApplicationContext());
        activity.startNetLog();

        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();

        // Quic always races the first request with SPDY. Since our test server
        // only supports Quic, request will either succeed with a 200 or fail
        // with a 500.
        for (int i = 0; i < 2; i++) {
            HttpUrlRequest request =
                    activity.mRequestFactory.createRequest(
                            quicURL,
                            HttpUrlRequest.REQUEST_PRIORITY_MEDIUM,
                            headers,
                            listener);
            request.start();
            listener.blockForComplete();
            assertTrue(listener.mHttpStatusCode == 200
                    || listener.mHttpStatusCode == 500);
            if (listener.mHttpStatusCode == 200) break;
        }
        assertEquals(200, listener.mHttpStatusCode);
        assertEquals(
                "This is a simple text file served by QUIC.\n",
                listener.mResponseAsString);
        assertEquals("quic/1+spdy/3", listener.mNegotiatedProtocol);
        activity.stopNetLog();
        QuicTestServer.shutdownQuicTestServer();
    }
}
