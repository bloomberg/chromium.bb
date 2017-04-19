// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.support.test.filters.MediumTest;

import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.test.util.Feature;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * Tests for experimental options.
 */
public class ExperimentalOptionsTest extends CronetTestBase {
    private static final String TAG = ExperimentalOptionsTest.class.getSimpleName();
    private CronetTestFramework mTestFramework;
    private ExperimentalCronetEngine.Builder mBuilder;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mBuilder = new ExperimentalCronetEngine.Builder(getContext());
        CronetTestUtil.setMockCertVerifierForTesting(
                mBuilder, QuicTestServer.createMockCertVerifier());
        assertTrue(Http2TestServer.startHttp2TestServer(
                getContext(), QuicTestServer.getServerCert(), QuicTestServer.getServerCertKey()));
    }

    @Override
    protected void tearDown() throws Exception {
        assertTrue(Http2TestServer.shutdownHttp2TestServer());
        if (mTestFramework != null && mTestFramework.mCronetEngine != null) {
            mTestFramework.mCronetEngine.shutdown();
        }
        super.tearDown();
    }

    @MediumTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Tests that NetLog writes effective experimental options to NetLog.
    public void testNetLog() throws Exception {
        File directory = new File(PathUtils.getDataDirectory());
        File logfile = File.createTempFile("cronet", "json", directory);
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions =
                new JSONObject().put("HostResolverRules", hostResolverParams);
        mBuilder.setExperimentalOptions(experimentalOptions.toString());

        mTestFramework = new CronetTestFramework(null, null, getContext(), mBuilder);
        mTestFramework.mCronetEngine.startNetLogToFile(logfile.getPath(), false);
        String url = Http2TestServer.getEchoMethodUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                url, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", callback.mResponseAsString);
        mTestFramework.mCronetEngine.stopNetLog();
        assertFileContainsString(logfile, "HostResolverRules");
        assertTrue(logfile.delete());
        assertFalse(logfile.exists());
    }

    @MediumTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testSetSSLKeyLogFile() throws Exception {
        String url = Http2TestServer.getEchoMethodUrl();
        File dir = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("ssl_key_log_file", "", dir);

        JSONObject experimentalOptions = new JSONObject().put("ssl_key_log_file", file.getPath());
        mBuilder.setExperimentalOptions(experimentalOptions.toString());
        mTestFramework = new CronetTestFramework(null, null, getContext(), mBuilder);

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                url, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", callback.mResponseAsString);

        assertFileContainsString(file, "CLIENT_RANDOM");
        assertTrue(file.delete());
        assertFalse(file.exists());
    }

    // Helper method to assert that file contains content. It retries 5 times
    // with a 100ms interval.
    private void assertFileContainsString(File file, String content) throws Exception {
        boolean contains = false;
        for (int i = 0; i < 5; i++) {
            contains = fileContainsString(file, content);
            if (contains) break;
            Log.i(TAG, "Retrying...");
            Thread.sleep(100);
        }
        assertTrue("file content doesn't match", contains);
    }

    // Returns whether a file contains a particular string.
    private boolean fileContainsString(File file, String content) throws IOException {
        FileInputStream fileInputStream = null;
        Log.i(TAG, "looking for [%s] in %s", content, file.getName());
        try {
            fileInputStream = new FileInputStream(file);
            byte[] data = new byte[(int) file.length()];
            fileInputStream.read(data);
            String actual = new String(data, "UTF-8");
            boolean contains = actual.contains(content);
            if (!contains) {
                Log.i(TAG, "file content [%s]", actual);
            }
            return contains;
        } catch (FileNotFoundException e) {
            // Ignored this exception since the file will only be created when updates are
            // flushed to the disk.
            Log.i(TAG, "file not found");
        } finally {
            if (fileInputStream != null) {
                fileInputStream.close();
            }
        }
        return false;
    }
}
