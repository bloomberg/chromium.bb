// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.Log;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestBase.OnlyRunNativeCronet;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.HashMap;

/**
 * Tests making requests using QUIC.
 */
public class QuicTest extends CronetTestBase {
    private static final String TAG = "cr.QuicTest";
    private CronetTestFramework mTestFramework;
    private CronetEngine.Builder mBuilder;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        // Load library first, since we need the Quic test server's URL.
        System.loadLibrary("cronet_tests");
        QuicTestServer.startQuicTestServer(getContext());

        mBuilder = new CronetEngine.Builder(getContext());
        mBuilder.enableQUIC(true);
        mBuilder.addQuicHint(QuicTestServer.getServerHost(), QuicTestServer.getServerPort(),
                QuicTestServer.getServerPort());

        JSONObject quicParams = new JSONObject()
                                        .put("connection_options", "PACE,IW10,FOO,DEADBEEF")
                                        .put("host_whitelist", "test.example.com")
                                        .put("max_server_configs_stored_in_properties", 2)
                                        .put("delay_tcp_race", true)
                                        .put("max_number_of_lossy_connections", 10)
                                        .put("packet_loss_threshold", 0.5)
                                        .put("idle_connection_timeout_seconds", 300)
                                        .put("close_sessions_on_ip_change", true)
                                        .put("migrate_sessions_on_network_change", false);
        JSONObject experimentalOptions = new JSONObject().put("QUIC", quicParams);
        mBuilder.setExperimentalOptions(experimentalOptions.toString());

        mBuilder.setMockCertVerifierForTesting(QuicTestServer.createMockCertVerifier());
        mBuilder.setStoragePath(CronetTestFramework.getTestStorage(getContext()));
        mBuilder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP, 1000 * 1024);
    }

    @Override
    protected void tearDown() throws Exception {
        QuicTestServer.shutdownQuicTestServer();
        super.tearDown();
    }

    @SmallTest
    @Feature({"Cronet"})
    @SuppressWarnings("deprecation")
    @OnlyRunNativeCronet
    public void testQuicLoadUrl_LegacyAPI() throws Exception {
        String[] commandLineArgs = {
                CronetTestFramework.LIBRARY_INIT_KEY, CronetTestFramework.LibraryInitType.LEGACY};
        mTestFramework = new CronetTestFramework(null, commandLineArgs, getContext(), mBuilder);
        registerHostResolver(mTestFramework, true);
        String quicURL = QuicTestServer.getServerURL() + "/simple.txt";

        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();

        // Although the native stack races QUIC and SPDY for the first request,
        // since there is no http server running on the corresponding TCP port,
        // QUIC will always succeed with a 200 (see
        // net::HttpStreamFactoryImpl::Request::OnStreamFailed).
        HttpUrlRequest request = mTestFramework.mRequestFactory.createRequest(
                quicURL, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        request.start();
        listener.blockForComplete();
        assertEquals(200, listener.mHttpStatusCode);
        assertEquals(
                "This is a simple text file served by QUIC.\n",
                listener.mResponseAsString);
        assertEquals("quic/1+spdy/3", listener.mNegotiatedProtocol);
    }

    @LargeTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testQuicLoadUrl() throws Exception {
        mTestFramework = startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, mBuilder);
        registerHostResolver(mTestFramework);
        String quicURL = QuicTestServer.getServerURL() + "/simple.txt";
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        // Although the native stack races QUIC and SPDY for the first request,
        // since there is no http server running on the corresponding TCP port,
        // QUIC will always succeed with a 200 (see
        // net::HttpStreamFactoryImpl::Request::OnStreamFailed).
        UrlRequest.Builder requestBuilder = new UrlRequest.Builder(
                quicURL, callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        requestBuilder.build().start();
        callback.blockForDone();

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        String expectedContent = "This is a simple text file served by QUIC.\n";
        assertEquals(expectedContent, callback.mResponseAsString);
        assertEquals("quic/1+spdy/3", callback.mResponseInfo.getNegotiatedProtocol());
        // The total received bytes should be larger than the content length, to account for
        // headers.
        assertTrue(callback.mResponseInfo.getReceivedBytesCount() > expectedContent.length());
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
        CronetEngine.Builder builder = new CronetEngine.Builder(getContext());
        builder.setStoragePath(CronetTestFramework.getTestStorage(getContext()));
        builder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1000 * 1024);
        builder.enableQUIC(true);
        JSONObject quicParams = new JSONObject().put("host_whitelist", "test.example.com");
        JSONObject experimentalOptions = new JSONObject().put("QUIC", quicParams);
        builder.setExperimentalOptions(experimentalOptions.toString());
        builder.setMockCertVerifierForTesting(QuicTestServer.createMockCertVerifier());
        mTestFramework = startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, builder);
        registerHostResolver(mTestFramework);
        TestUrlRequestCallback callback2 = new TestUrlRequestCallback();
        requestBuilder = new UrlRequest.Builder(
                quicURL, callback2, callback2.getExecutor(), mTestFramework.mCronetEngine);
        requestBuilder.build().start();
        callback2.blockForDone();
        assertEquals(200, callback2.mResponseInfo.getHttpStatusCode());
        assertEquals(expectedContent, callback2.mResponseAsString);
        assertEquals("quic/1+spdy/3", callback2.mResponseInfo.getNegotiatedProtocol());
        // The total received bytes should be larger than the content length, to account for
        // headers.
        assertTrue(callback2.mResponseInfo.getReceivedBytesCount() > expectedContent.length());
    }

    // Returns whether a file contains a particular string.
    @SuppressFBWarnings("OBL_UNSATISFIED_OBLIGATION_EXCEPTION_EDGE")
    private boolean fileContainsString(String filename, String content) throws IOException {
        File file = new File(CronetTestFramework.getTestStorage(getContext()) + "/" + filename);
        FileInputStream fileInputStream = new FileInputStream(file);
        byte[] data = new byte[(int) file.length()];
        fileInputStream.read(data);
        fileInputStream.close();
        return new String(data, "UTF-8").contains(content);
    }
}
