// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import android.support.test.filters.SmallTest;

import org.json.JSONObject;

import static org.chromium.net.smoke.TestSupport.Protocol.QUIC;

import org.chromium.net.UrlRequest;

import java.net.URL;

/**
 * QUIC Tests.
 */
public class QuicTest extends NativeCronetTestCase {
    private TestSupport.TestServer mServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mServer = mTestSupport.createTestServer(getContext(), QUIC);
    }

    @Override
    protected void tearDown() throws Exception {
        mServer.shutdown();
        super.tearDown();
    }

    @SmallTest
    public void testQuic() throws Exception {
        assertTrue(mServer.start());
        final String urlString = mServer.getSuccessURL();
        final URL url = new URL(urlString);

        mCronetEngineBuilder.enableQuic(true);
        mCronetEngineBuilder.addQuicHint(url.getHost(), url.getPort(), url.getPort());
        mTestSupport.installMockCertVerifierForTesting(mCronetEngineBuilder);

        JSONObject quicParams = new JSONObject().put("delay_tcp_race", true);
        JSONObject experimentalOptions = new JSONObject().put("QUIC", quicParams);
        mTestSupport.addHostResolverRules(experimentalOptions);
        experimentalOptions.put("host_whitelist", url.getHost());

        mCronetEngineBuilder.setExperimentalOptions(experimentalOptions.toString());

        initCronetEngine();

        // QUIC is not guaranteed to win the race even with |delay_tcp_race| set, so try
        // multiple times.
        boolean quicNegotiated = false;

        for (int i = 0; i < 5; i++) {
            SmokeTestRequestCallback callback = new SmokeTestRequestCallback();
            UrlRequest.Builder requestBuilder =
                    mCronetEngine.newUrlRequestBuilder(urlString, callback, callback.getExecutor());
            requestBuilder.build().start();
            callback.blockForDone();
            assertSuccessfulNonEmptyResponse(callback, urlString);
            if (callback.getResponseInfo().getNegotiatedProtocol().startsWith("http/2+quic/")) {
                quicNegotiated = true;
                break;
            }
        }
        assertTrue(quicNegotiated);
    }
}
