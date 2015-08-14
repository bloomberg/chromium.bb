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

    @LargeTest
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
        mActivity.mUrlRequestContext.shutdown();

        // Make another request using a new context but with no QUIC hints.
        UrlRequestContextConfig config = new UrlRequestContextConfig();
        config.setStoragePath(mActivity.getTestStorage());
        config.enableHttpCache(UrlRequestContextConfig.HTTP_CACHE_DISK, 1000 * 1024);
        config.enableQUIC(true);
        CronetUrlRequestContext newContext =
                new CronetUrlRequestContext(getInstrumentation().getTargetContext(), config);
        TestUrlRequestListener listener2 = new TestUrlRequestListener();
        UrlRequest request2 = newContext.createRequest(quicURL, listener2, listener2.getExecutor());
        request2.start();
        listener2.blockForDone();
        assertEquals(200, listener2.mResponseInfo.getHttpStatusCode());
        assertEquals("This is a simple text file served by QUIC.\n", listener2.mResponseAsString);
        assertEquals("quic/1+spdy/3", listener2.mResponseInfo.getNegotiatedProtocol());
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
