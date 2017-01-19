// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.ConditionVariable;
import android.support.test.filters.LargeTest;
import android.support.test.filters.SmallTest;

import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestBase.OnlyRunNativeCronet;
import org.chromium.net.MetricsTestUtil.TestRequestFinishedListener;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Date;
import java.util.concurrent.Executors;

/**
 * Tests making requests using QUIC.
 */
public class QuicTest extends CronetTestBase {
    private static final String TAG = QuicTest.class.getSimpleName();
    private static final String QUIC_PROTOCOL_STRING_PREFIX = "http/2+quic/";
    private CronetTestFramework mTestFramework;
    private ExperimentalCronetEngine.Builder mBuilder;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        // Load library first, since we need the Quic test server's URL.
        System.loadLibrary("cronet_tests");
        QuicTestServer.startQuicTestServer(getContext());

        mBuilder = new ExperimentalCronetEngine.Builder(getContext());
        mBuilder.enableNetworkQualityEstimator(true).enableQuic(true);
        mBuilder.addQuicHint(QuicTestServer.getServerHost(), QuicTestServer.getServerPort(),
                QuicTestServer.getServerPort());

        // TODO(mgersh): Enable connection migration once it works, see http://crbug.com/634910
        JSONObject quicParams = new JSONObject()
                                        .put("connection_options", "PACE,IW10,FOO,DEADBEEF")
                                        .put("host_whitelist", "test.example.com")
                                        .put("max_server_configs_stored_in_properties", 2)
                                        .put("delay_tcp_race", true)
                                        .put("idle_connection_timeout_seconds", 300)
                                        .put("close_sessions_on_ip_change", false)
                                        .put("migrate_sessions_on_network_change", false)
                                        .put("migrate_sessions_early", false)
                                        .put("race_cert_verification", true);
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions = new JSONObject()
                                                 .put("QUIC", quicParams)
                                                 .put("HostResolverRules", hostResolverParams);
        mBuilder.setExperimentalOptions(experimentalOptions.toString());
        mBuilder.setStoragePath(CronetTestFramework.getTestStorage(getContext()));
        mBuilder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP, 1000 * 1024);
        CronetTestUtil.setMockCertVerifierForTesting(
                mBuilder, QuicTestServer.createMockCertVerifier());
    }

    @Override
    protected void tearDown() throws Exception {
        QuicTestServer.shutdownQuicTestServer();
        super.tearDown();
    }

    @LargeTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testQuicLoadUrl() throws Exception {
        mTestFramework = startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, mBuilder);
        String quicURL = QuicTestServer.getServerURL() + "/simple.txt";
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        // Although the native stack races QUIC and SPDY for the first request,
        // since there is no http server running on the corresponding TCP port,
        // QUIC will always succeed with a 200 (see
        // net::HttpStreamFactoryImpl::Request::OnStreamFailed).
        UrlRequest.Builder requestBuilder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                quicURL, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        String expectedContent = "This is a simple text file served by QUIC.\n";
        assertEquals(expectedContent, callback.mResponseAsString);
        assertIsQuic(callback.mResponseInfo);
        // The total received bytes should be larger than the content length, to account for
        // headers.
        assertTrue(callback.mResponseInfo.getReceivedByteCount() > expectedContent.length());
        // This test takes a long time, since the update will only be scheduled
        // after kUpdatePrefsDelayMs in http_server_properties_manager.cc.
        while (true) {
            Log.i(TAG, "Still waiting for pref file update.....");
            Thread.sleep(10000);
            boolean contains = false;
            try {
                if (fileContainsString("local_prefs.json", "quic")) break;
            } catch (FileNotFoundException e) {
                // Ignored this exception since the file will only be created when updates are
                // flushed to the disk.
            }
        }
        assertTrue(fileContainsString("local_prefs.json",
                QuicTestServer.getServerHost() + ":" + QuicTestServer.getServerPort()));
        mTestFramework.mCronetEngine.shutdown();

        // Make another request using a new context but with no QUIC hints.
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        builder.setStoragePath(CronetTestFramework.getTestStorage(getContext()));
        builder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1000 * 1024);
        builder.enableQuic(true);
        JSONObject quicParams = new JSONObject().put("host_whitelist", "test.example.com");
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions = new JSONObject()
                                                 .put("QUIC", quicParams)
                                                 .put("HostResolverRules", hostResolverParams);
        builder.setExperimentalOptions(experimentalOptions.toString());
        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());
        mTestFramework = startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, builder);
        TestUrlRequestCallback callback2 = new TestUrlRequestCallback();
        requestBuilder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                quicURL, callback2, callback2.getExecutor());
        requestBuilder.build().start();
        callback2.blockForDone();
        assertEquals(200, callback2.mResponseInfo.getHttpStatusCode());
        assertEquals(expectedContent, callback2.mResponseAsString);
        assertIsQuic(callback.mResponseInfo);
        // The total received bytes should be larger than the content length, to account for
        // headers.
        assertTrue(callback2.mResponseInfo.getReceivedByteCount() > expectedContent.length());
    }

    // Returns whether a file contains a particular string.
    @SuppressFBWarnings("OBL_UNSATISFIED_OBLIGATION_EXCEPTION_EDGE")
    private boolean fileContainsString(String filename, String content) throws IOException {
        File file =
                new File(CronetTestFramework.getTestStorage(getContext()) + "/prefs/" + filename);
        FileInputStream fileInputStream = new FileInputStream(file);
        byte[] data = new byte[(int) file.length()];
        fileInputStream.read(data);
        fileInputStream.close();
        return new String(data, "UTF-8").contains(content);
    }

    @LargeTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    @SuppressWarnings("deprecation")
    public void testRealTimeNetworkQualityObservationsWithQuic() throws Exception {
        mTestFramework = startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, mBuilder);
        String quicURL = QuicTestServer.getServerURL() + "/simple.txt";
        ConditionVariable waitForThroughput = new ConditionVariable();

        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(Executors.newSingleThreadExecutor());
        TestNetworkQualityThroughputListener throughputListener =
                new TestNetworkQualityThroughputListener(
                        Executors.newSingleThreadExecutor(), waitForThroughput);

        mTestFramework.mCronetEngine.addRttListener(rttListener);
        mTestFramework.mCronetEngine.addThroughputListener(throughputListener);

        mTestFramework.mCronetEngine.configureNetworkQualityEstimatorForTesting(true, true, true);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        // Although the native stack races QUIC and SPDY for the first request,
        // since there is no http server running on the corresponding TCP port,
        // QUIC will always succeed with a 200 (see
        // net::HttpStreamFactoryImpl::Request::OnStreamFailed).
        UrlRequest.Builder requestBuilder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                quicURL, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        String expectedContent = "This is a simple text file served by QUIC.\n";
        assertEquals(expectedContent, callback.mResponseAsString);
        assertIsQuic(callback.mResponseInfo);

        // Throughput observation is posted to the network quality estimator on the network thread
        // after the UrlRequest is completed. The observations are then eventually posted to
        // throughput listeners on the executor provided to network quality.
        waitForThroughput.block();
        assertTrue(throughputListener.throughputObservationCount() > 0);

        // Check RTT observation count after throughput observation has been received. This ensures
        // that executor has finished posting the RTT observation to the RTT listeners.
        // NETWORK_QUALITY_OBSERVATION_SOURCE_URL_REQUEST
        assertTrue(rttListener.rttObservationCount(0) > 0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC
        assertTrue(rttListener.rttObservationCount(2) > 0);

        // Verify that effective connection type callback is received and
        // effective connection type is correctly set.
        assertTrue(mTestFramework.mCronetEngine.getEffectiveConnectionType()
                != EffectiveConnectionType.TYPE_UNKNOWN);

        // Verify that the HTTP RTT, transport RTT and downstream throughput
        // estimates are available.
        assertTrue(mTestFramework.mCronetEngine.getHttpRttMs() >= 0);
        assertTrue(mTestFramework.mCronetEngine.getTransportRttMs() >= 0);
        assertTrue(mTestFramework.mCronetEngine.getDownstreamThroughputKbps() >= 0);

        // Verify that the cached estimates were written to the prefs.
        while (true) {
            Log.i(TAG, "Still waiting for pref file update.....");
            Thread.sleep(10000);
            try {
                if (fileContainsString("local_prefs.json", "network_qualities")) {
                    break;
                }
            } catch (FileNotFoundException e) {
                // Ignored this exception since the file will only be created when updates are
                // flushed to the disk.
            }
        }
        assertTrue(fileContainsString("local_prefs.json", "network_qualities"));
        mTestFramework.mCronetEngine.shutdown();
    }

    @SmallTest
    @OnlyRunNativeCronet
    @Feature({"Cronet"})
    public void testMetricsWithQuic() throws Exception {
        mTestFramework = startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, mBuilder);
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);

        String quicURL = QuicTestServer.getServerURL() + "/simple.txt";
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        UrlRequest.Builder requestBuilder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                quicURL, callback, callback.getExecutor());
        Date startTime = new Date();
        requestBuilder.build().start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertIsQuic(callback.mResponseInfo);

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(requestInfo, quicURL, startTime, endTime);
        assertEquals(RequestFinishedInfo.SUCCEEDED, requestInfo.getFinishedReason());
        MetricsTestUtil.checkHasConnectTiming(requestInfo.getMetrics(), startTime, endTime, true);

        // Second request should use the same connection and not have ConnectTiming numbers
        callback = new TestUrlRequestCallback();
        requestFinishedListener.reset();
        requestBuilder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                quicURL, callback, callback.getExecutor());
        startTime = new Date();
        requestBuilder.build().start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        endTime = new Date();

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertIsQuic(callback.mResponseInfo);

        requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(requestInfo, quicURL, startTime, endTime);
        assertEquals(RequestFinishedInfo.SUCCEEDED, requestInfo.getFinishedReason());
        MetricsTestUtil.checkNoConnectTiming(requestInfo.getMetrics());

        mTestFramework.mCronetEngine.shutdown();
    }

    // Helper method to assert that the request is negotiated over QUIC.
    private void assertIsQuic(UrlResponseInfo responseInfo) {
        assertTrue(responseInfo.getNegotiatedProtocol().startsWith(QUIC_PROTOCOL_STRING_PREFIX));
    }
}
