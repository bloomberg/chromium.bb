// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import android.support.test.filters.SmallTest;

import org.json.JSONObject;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestBase;
import org.chromium.net.CronetTestFramework;
import org.chromium.net.CronetTestUtil;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.QuicTestServer;

import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Arrays;

/**
 * Tests HttpURLConnection upload using QUIC.
 */
@SuppressWarnings("deprecation")
public class QuicUploadTest extends CronetTestBase {
    private CronetTestFramework mTestFramework;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        // Load library first to create MockCertVerifier.
        System.loadLibrary("cronet_tests");
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());

        QuicTestServer.startQuicTestServer(getContext());

        builder.enableQuic(true);
        JSONObject quicParams =
                new JSONObject().put("host_whitelist", QuicTestServer.getServerHost());
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions = new JSONObject()
                                                 .put("QUIC", quicParams)
                                                 .put("HostResolverRules", hostResolverParams);
        builder.setExperimentalOptions(experimentalOptions.toString());

        builder.addQuicHint(QuicTestServer.getServerHost(), QuicTestServer.getServerPort(),
                QuicTestServer.getServerPort());

        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());

        mTestFramework = startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, builder);
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Regression testing for crbug.com/618872.
    public void testOneMassiveWrite() throws Exception {
        String path = "/simple.txt";
        URL url = new URL(QuicTestServer.getServerURL() + path);
        HttpURLConnection connection =
                (HttpURLConnection) mTestFramework.mCronetEngine.openConnection(url);
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        // Size is chosen so the last time mBuffer will be written 14831 bytes,
        // which is larger than the internal QUIC read buffer size of 14520.
        byte[] largeData = new byte[195055];
        Arrays.fill(largeData, "a".getBytes("UTF-8")[0]);
        connection.setFixedLengthStreamingMode(largeData.length);
        OutputStream out = connection.getOutputStream();
        // Write everything at one go, so the data is larger than the buffer
        // used in CronetFixedModeOutputStream.
        out.write(largeData);
        assertEquals(200, connection.getResponseCode());
        connection.disconnect();
    }
}
