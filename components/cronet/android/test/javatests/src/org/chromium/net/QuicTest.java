// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.Log;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;

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
    private CronetTestActivity mActivity;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        // Load library first, since we need the Quic test server's URL.
        System.loadLibrary("cronet_tests");
        QuicTestServer.startQuicTestServer(getInstrumentation().getTargetContext());
        CronetEngine.Builder builder = new CronetEngine.Builder(mActivity);
        builder.enableQUIC(true);
        builder.addQuicHint(QuicTestServer.getServerHost(), QuicTestServer.getServerPort(),
                QuicTestServer.getServerPort());
        builder.setExperimentalQuicConnectionOptions("PACE,IW10,FOO,DEADBEEF");

        String[] commandLineArgs = {CronetTestActivity.CONFIG_KEY, builder.toString(),
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

    @LargeTest
    @Feature({"Cronet"})
    public void testQuicLoadUrl() throws Exception {
        String quicURL = QuicTestServer.getServerURL() + "/simple.txt";
        TestUrlRequestListener listener = new TestUrlRequestListener();

        // Although the native stack races QUIC and SPDY for the first request,
        // since there is no http server running on the corresponding TCP port,
        // QUIC will always succeed with a 200 (see
        // net::HttpStreamFactoryImpl::Request::OnStreamFailed).
        UrlRequest.Builder requestBuilder = new UrlRequest.Builder(
                quicURL, listener, listener.getExecutor(), mActivity.mCronetEngine);
        requestBuilder.build().start();
        listener.blockForDone();

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        String expectedContent = "This is a simple text file served by QUIC.\n";
        assertEquals(expectedContent, listener.mResponseAsString);
        assertEquals("quic/1+spdy/3", listener.mResponseInfo.getNegotiatedProtocol());
        // The total received bytes should be larger than the content length, to account for
        // headers.
        assertTrue(listener.mResponseInfo.getReceivedBytesCount() > expectedContent.length());

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
        mActivity.mCronetEngine.shutdown();

        // Make another request using a new context but with no QUIC hints.
        CronetEngine.Builder builder =
                new CronetEngine.Builder(getInstrumentation().getTargetContext());
        builder.setStoragePath(mActivity.getTestStorage());
        builder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1000 * 1024);
        builder.enableQUIC(true);
        CronetEngine newEngine = new CronetUrlRequestContext(builder);
        TestUrlRequestListener listener2 = new TestUrlRequestListener();
        requestBuilder =
                new UrlRequest.Builder(quicURL, listener2, listener2.getExecutor(), newEngine);
        requestBuilder.build().start();
        listener2.blockForDone();
        assertEquals(200, listener2.mResponseInfo.getHttpStatusCode());
        assertEquals(expectedContent, listener2.mResponseAsString);
        assertEquals("quic/1+spdy/3", listener2.mResponseInfo.getNegotiatedProtocol());
        // The total received bytes should be larger than the content length, to account for
        // headers.
        assertTrue(listener2.mResponseInfo.getReceivedBytesCount() > expectedContent.length());
    }

    // Returns whether a file contains a particular string.
    @SuppressFBWarnings("OBL_UNSATISFIED_OBLIGATION_EXCEPTION_EDGE")
    private boolean fileContainsString(String filename, String content) throws IOException {
        File file = new File(mActivity.getTestStorage() + "/" + filename);
        FileInputStream fileInputStream = new FileInputStream(file);
        byte[] data = new byte[(int) file.length()];
        fileInputStream.read(data);
        fileInputStream.close();
        return new String(data, "UTF-8").contains(content);
    }
}
