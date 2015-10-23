// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestBase;
import org.chromium.net.CronetTestFramework;
import org.chromium.net.NativeTestServer;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.ProtocolException;
import java.net.URL;

/**
 * Tests {@code getOutputStream} when {@code setChunkedStreamingMode} is enabled.
 * Tests annotated with {@code CompareDefaultWithCronet} will run once with the
 * default HttpURLConnection implementation and then with Cronet's
 * HttpURLConnection implementation. Tests annotated with
 * {@code OnlyRunCronetHttpURLConnection} only run Cronet's implementation.
 * See {@link CronetTestBase#runTest()} for details.
 */
public class CronetChunkedOutputStreamTest extends CronetTestBase {
    private static final String UPLOAD_DATA_STRING = "Nifty upload data!";
    private static final byte[] UPLOAD_DATA = UPLOAD_DATA_STRING.getBytes();
    private static final int REPEAT_COUNT = 100000;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        String[] commandLineArgs = {
                CronetTestFramework.LIBRARY_INIT_KEY, CronetTestFramework.LIBRARY_INIT_WRAPPER,
        };
        startCronetTestFrameworkWithUrlAndCommandLineArgs(null, commandLineArgs);
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
    }

    @Override
    protected void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        super.tearDown();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testGetOutputStreamAfterConnectionMade() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        connection.setChunkedStreamingMode(0);
        assertEquals(200, connection.getResponseCode());
        try {
            connection.getOutputStream();
            fail();
        } catch (ProtocolException e) {
            // Expected.
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testWriteAfterReadingResponse() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        connection.setChunkedStreamingMode(0);
        OutputStream out = connection.getOutputStream();
        assertEquals(200, connection.getResponseCode());
        try {
            out.write(UPLOAD_DATA);
            fail();
        } catch (IOException e) {
            // Expected.
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testPost() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        connection.setChunkedStreamingMode(0);
        OutputStream out = connection.getOutputStream();
        out.write(UPLOAD_DATA);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals(UPLOAD_DATA_STRING, getResponseAsString(connection));
        connection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testTransferEncodingHeaderSet() throws Exception {
        URL url = new URL(NativeTestServer.getEchoHeaderURL("Transfer-Encoding"));
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        connection.setChunkedStreamingMode(0);
        OutputStream out = connection.getOutputStream();
        out.write(UPLOAD_DATA);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals("chunked", getResponseAsString(connection));
        connection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testPostOneMassiveWrite() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        connection.setChunkedStreamingMode(0);
        OutputStream out = connection.getOutputStream();
        byte[] largeData = getLargeData();
        out.write(largeData);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        checkLargeData(getResponseAsString(connection));
        connection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testPostWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        connection.setChunkedStreamingMode(0);
        OutputStream out = connection.getOutputStream();
        for (int i = 0; i < UPLOAD_DATA.length; i++) {
            out.write(UPLOAD_DATA[i]);
        }
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals(UPLOAD_DATA_STRING, getResponseAsString(connection));
        connection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testPostOneMassiveWriteWriteOneByte() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        connection.setChunkedStreamingMode(0);
        OutputStream out = connection.getOutputStream();
        byte[] largeData = getLargeData();
        for (int i = 0; i < largeData.length; i++) {
            out.write(largeData[i]);
        }
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        checkLargeData(getResponseAsString(connection));
        connection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testPostWholeNumberOfChunks() throws Exception {
        URL url = new URL(NativeTestServer.getEchoBodyURL());
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        int totalSize = UPLOAD_DATA.length * REPEAT_COUNT;
        int chunkSize = 18000;
        // Ensure total data size is a multiple of chunk size, so no partial
        // chunks will be used.
        assertEquals(0, totalSize % chunkSize);
        connection.setChunkedStreamingMode(chunkSize);
        OutputStream out = connection.getOutputStream();
        byte[] largeData = getLargeData();
        out.write(largeData);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        checkLargeData(getResponseAsString(connection));
        connection.disconnect();
    }

    /**
     * Helper method to extract response body as a string for testing.
     */
    private static String getResponseAsString(HttpURLConnection connection) throws Exception {
        InputStream in = connection.getInputStream();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        int b;
        while ((b = in.read()) != -1) {
            out.write(b);
        }
        return out.toString();
    }

    /**
     * Produces a byte array that contains {@code REPEAT_COUNT} of
     * {@code UPLOAD_DATA_STRING}.
     */
    private static byte[] getLargeData() {
        byte[] largeData = new byte[REPEAT_COUNT * UPLOAD_DATA.length];
        for (int i = 0; i < REPEAT_COUNT; i++) {
            for (int j = 0; j < UPLOAD_DATA.length; j++) {
                largeData[i * UPLOAD_DATA.length + j] = UPLOAD_DATA[j];
            }
        }
        return largeData;
    }

    /**
     * Helper function to check whether {@code data} is a concatenation of
     * {@code REPEAT_COUNT} {@code UPLOAD_DATA_STRING} strings.
     */
    private static void checkLargeData(String data) {
        for (int i = 0; i < REPEAT_COUNT; i++) {
            assertEquals(UPLOAD_DATA_STRING, data.substring(UPLOAD_DATA_STRING.length() * i,
                                                     UPLOAD_DATA_STRING.length() * (i + 1)));
        }
    }
}
