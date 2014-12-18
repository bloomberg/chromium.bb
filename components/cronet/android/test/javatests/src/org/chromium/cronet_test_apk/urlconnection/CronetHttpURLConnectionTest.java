// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk.urlconnection;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.cronet_test_apk.CronetTestActivity;
import org.chromium.cronet_test_apk.CronetTestBase;
import org.chromium.cronet_test_apk.MockUrlRequestJobFactory;
import org.chromium.cronet_test_apk.UploadTestServer;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Basic tests of Cronet's HttpURLConnection implementation.
 * Tests annotated with {@code CompareDefaultWithCronet} will run once with the
 * default HttpURLConnection implementation and then with Cronet's
 * HttpURLConnection implementation. Tests annotated with
 * {@code OnlyRunCronetHttpURLConnection} only run Cronet's implementation.
 * See {@link CronetTestBase#runTest()} for details.
 */
public class CronetHttpURLConnectionTest extends CronetTestBase {
    private CronetTestActivity mActivity;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = launchCronetTestApp();
        assertTrue(UploadTestServer.startUploadTestServer(
                getInstrumentation().getTargetContext()));
    }

    @Override
    protected void tearDown() throws Exception {
        UploadTestServer.shutdownUploadTestServer();
        super.tearDown();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testBasicGet() throws Exception {
        URL url = new URL(UploadTestServer.getEchoMethodURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        assertEquals("GET", getResponseAsString(urlConnection));
        urlConnection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    // TODO(xunjieli): Use embedded test server's ServeFilesFromDirectory.
    // Mock UrlRequestJobs only work for chromium network stack.
    public void testNotFoundURLRequest() throws Exception {
        MockUrlRequestJobFactory mockFactory = new MockUrlRequestJobFactory(
                getInstrumentation().getTargetContext());
        URL url = new URL(MockUrlRequestJobFactory.NOTFOUND_URL);
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        assertEquals(404, urlConnection.getResponseCode());
        assertEquals("Not Found", urlConnection.getResponseMessage());
        assertEquals("<!DOCTYPE html>\n<html>\n<head>\n"
                + "<title>Not found</title>\n<p>Test page loaded.</p>\n"
                + "</head>\n</html>\n",
                getResponseAsString(urlConnection));
        urlConnection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testDisconnectBeforeConnectionIsMade() throws Exception {
        URL url = new URL(UploadTestServer.getEchoMethodURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        // Close connection before connection is made has no effect.
        // Default implementation passes this test.
        urlConnection.disconnect();
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        assertEquals("GET", getResponseAsString(urlConnection));
    }

    @SmallTest
    @Feature({"Cronet"})
    // TODO(xunjieli): Currently the wrapper does not throw an exception.
    // Need to change the behavior.
    // @CompareDefaultWithCronet
    public void testDisconnectAfterConnectionIsMade() throws Exception {
        URL url = new URL(UploadTestServer.getEchoMethodURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        // Close connection before connection is made has no effect.
        urlConnection.connect();
        urlConnection.disconnect();
        try {
            urlConnection.getResponseCode();
            fail();
        } catch (Exception e) {
            // Ignored.
        }
        try {
            InputStream in = urlConnection.getInputStream();
            fail();
        } catch (Exception e) {
            // Ignored.
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testAddRequestProperty() throws Exception {
        URL url = new URL(UploadTestServer.getEchoAllHeadersURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.addRequestProperty("foo-header", "foo");
        urlConnection.addRequestProperty("bar-header", "bar");
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        String headers = getResponseAsString(urlConnection);
        List<String> fooHeaderValues =
                getRequestHeaderValues(headers, "foo-header");
        List<String> barHeaderValues =
                getRequestHeaderValues(headers, "bar-header");
        assertEquals(1, fooHeaderValues.size());
        assertEquals("foo", fooHeaderValues.get(0));
        assertEquals(1, fooHeaderValues.size());
        assertEquals("bar", barHeaderValues.get(0));
        urlConnection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    // TODO(xunjieli): Change the annotation once crbug.com/432719 is fixed.
    public void testAddRequestPropertyWithSameKey() throws Exception {
        URL url = new URL(UploadTestServer.getEchoAllHeadersURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.addRequestProperty("header-name", "value1");
        urlConnection.addRequestProperty("header-name", "value2");
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        String headers = getResponseAsString(urlConnection);
        List<String> actualValues =
                getRequestHeaderValues(headers, "header-name");
        // TODO(xunjieli): Currently Cronet only sends the last header value
        // if there are multiple entries with the same header key, see
        // crbug/432719. Fix this test once the bug is fixed.
        assertEquals(1, actualValues.size());
        assertEquals("value2", actualValues.get(0));
        urlConnection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testSetRequestProperty() throws Exception {
        URL url = new URL(UploadTestServer.getEchoAllHeadersURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.setRequestProperty("header-name", "header-value");
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        String headers = getResponseAsString(urlConnection);
        List<String> actualValues =
                getRequestHeaderValues(headers, "header-name");
        assertEquals(1, actualValues.size());
        assertEquals("header-value", actualValues.get(0));
        urlConnection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testSetRequestPropertyTwice() throws Exception {
        URL url = new URL(UploadTestServer.getEchoAllHeadersURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.setRequestProperty("yummy", "foo");
        urlConnection.setRequestProperty("yummy", "bar");
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        String headers = getResponseAsString(urlConnection);
        List<String> actualValues =
                getRequestHeaderValues(headers, "yummy");
        assertEquals(1, actualValues.size());
        assertEquals("bar", actualValues.get(0));
        urlConnection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testAddAndSetRequestProperty() throws Exception {
        URL url = new URL(UploadTestServer.getEchoAllHeadersURL());
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.addRequestProperty("header-name", "value1");
        urlConnection.setRequestProperty("header-name", "value2");
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        String headers = getResponseAsString(urlConnection);
        List<String> actualValues =
                getRequestHeaderValues(headers, "header-name");
        assertEquals(1, actualValues.size());
        assertEquals("value2", actualValues.get(0));
        urlConnection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testInputStreamReadOneByte() throws Exception {
        String testInputString = "this is a really long header";
        URL url = new URL(UploadTestServer.getEchoHeaderURL("foo"));
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.addRequestProperty("foo", testInputString);
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        InputStream in = urlConnection.getInputStream();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        int b;
        while ((b = in.read()) != -1) {
            out.write(b);
        }
        urlConnection.disconnect();
        assertTrue(Arrays.equals(
                testInputString.getBytes(), out.toByteArray()));
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testInputStreamReadMoreBytesThanAvailable() throws Exception {
        String testInputString = "this is a really long header";
        byte[] testInputBytes = testInputString.getBytes();
        URL url = new URL(UploadTestServer.getEchoHeaderURL("foo"));
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.addRequestProperty("foo", testInputString);
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        InputStream in = urlConnection.getInputStream();
        byte[] actualOutput = new byte[testInputBytes.length + 256];
        int bytesRead = in.read(actualOutput, 0, actualOutput.length);
        byte[] readSomeMore = new byte[10];
        int bytesReadBeyondAvailable  = in.read(readSomeMore, 0, 10);
        urlConnection.disconnect();
        assertEquals(testInputBytes.length, bytesRead);
        assertEquals(-1, bytesReadBeyondAvailable);
        for (int i = 0; i < bytesRead; i++) {
            assertEquals(testInputBytes[i], actualOutput[i]);
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testInputStreamReadExactBytesAvailable() throws Exception {
        String testInputString = "this is a really long header";
        byte[] testInputBytes = testInputString.getBytes();
        URL url = new URL(UploadTestServer.getEchoHeaderURL("foo"));
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.addRequestProperty("foo", testInputString);
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        InputStream in = urlConnection.getInputStream();
        byte[] actualOutput = new byte[testInputBytes.length];
        int bytesRead = in.read(actualOutput, 0, actualOutput.length);
        urlConnection.disconnect();
        assertEquals(testInputBytes.length, bytesRead);
        assertTrue(Arrays.equals(testInputBytes, actualOutput));
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testInputStreamReadLessBytesThanAvailable() throws Exception {
        String testInputString = "this is a really long header";
        byte[] testInputBytes = testInputString.getBytes();
        URL url = new URL(UploadTestServer.getEchoHeaderURL("foo"));
        HttpURLConnection urlConnection =
                (HttpURLConnection) url.openConnection();
        urlConnection.addRequestProperty("foo", testInputString);
        assertEquals(200, urlConnection.getResponseCode());
        assertEquals("OK", urlConnection.getResponseMessage());
        InputStream in = urlConnection.getInputStream();
        byte[] firstPart = new byte[testInputBytes.length - 10];
        int firstBytesRead = in.read(firstPart, 0, testInputBytes.length - 10);
        byte[] secondPart = new byte[10];
        int secondBytesRead = in.read(secondPart, 0, 10);
        assertEquals(testInputBytes.length - 10, firstBytesRead);
        assertEquals(10, secondBytesRead);
        for (int i = 0; i < firstPart.length; i++) {
            assertEquals(testInputBytes[i], firstPart[i]);
        }
        for (int i = 0; i < secondPart.length; i++) {
            assertEquals(testInputBytes[firstPart.length + i], secondPart[i]);
        }
        urlConnection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testFollowRedirects() throws Exception {
        URL url = new URL(UploadTestServer.getFileURL("/redirect.html"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        connection.setInstanceFollowRedirects(true);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals(UploadTestServer.getFileURL("/success.txt"),
                connection.getURL().toString());
        assertEquals("this is a text file\n", getResponseAsString(connection));
        connection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testDisableRedirects() throws Exception {
        URL url = new URL(UploadTestServer.getFileURL("/redirect.html"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        connection.setInstanceFollowRedirects(false);
        assertEquals(302, connection.getResponseCode());
        assertEquals("Found", connection.getResponseMessage());
        assertEquals(UploadTestServer.getFileURL("/redirect.html"),
                connection.getURL().toString());
        connection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testDisableRedirectsGlobal() throws Exception {
        HttpURLConnection.setFollowRedirects(false);
        URL url = new URL(UploadTestServer.getFileURL("/redirect.html"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        assertEquals(302, connection.getResponseCode());
        assertEquals("Found", connection.getResponseMessage());
        assertEquals(UploadTestServer.getFileURL("/redirect.html"),
                connection.getURL().toString());
        connection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @CompareDefaultWithCronet
    public void testDisableRedirectsGlobalAfterConnectionIsCreated()
            throws Exception {
        HttpURLConnection.setFollowRedirects(true);
        URL url = new URL(UploadTestServer.getFileURL("/redirect.html"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        // Disabling redirects globally after creating the HttpURLConnection
        // object should have no effect on the request.
        HttpURLConnection.setFollowRedirects(false);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals(UploadTestServer.getFileURL("/success.txt"),
                connection.getURL().toString());
        assertEquals("this is a text file\n", getResponseAsString(connection));
        connection.disconnect();
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunCronetHttpURLConnection
    // Cronet does not support reading response body of a 302 response.
    public void testDisableRedirectsTryReadBody() throws Exception {
        URL url = new URL(UploadTestServer.getFileURL("/redirect.html"));
        HttpURLConnection connection =
                (HttpURLConnection) url.openConnection();
        connection.setInstanceFollowRedirects(false);
        try {
            connection.getInputStream();
            fail();
        } catch (IOException e) {
            // Expected.
        }
        // TODO(xunjieli): Test that error stream should null here.
        connection.disconnect();
    }

    /**
     * Helper method to extract response body as a string for testing.
     */
    private String getResponseAsString(HttpURLConnection connection)
            throws Exception {
        InputStream in = connection.getInputStream();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        int b;
        while ((b = in.read()) != -1) {
            out.write(b);
        }
        return out.toString();
    }

    /**
     * Helper method to extract a list of header values with the give header
     * name.
     */
    private List<String> getRequestHeaderValues(String allHeaders,
            String headerName) {
        Pattern pattern = Pattern.compile(headerName + ":\\s(.*)\\r\\n");
        Matcher matcher = pattern.matcher(allHeaders);
        List<String> headerValues = new ArrayList<String>();
        while (matcher.find()) {
            headerValues.add(matcher.group(1));
        }
        return headerValues;
    }
}
