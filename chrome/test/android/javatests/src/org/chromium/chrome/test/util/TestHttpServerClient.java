// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import junit.framework.Assert;

import org.chromium.chrome.browser.util.StreamUtil;

import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;

/**
 * A test utility class to get URLs that point to the test HTTP server, and to verify that it is up
 * and giving expected responses to given URLs.
 */
public final class TestHttpServerClient {
    private static final int SERVER_PORT = 8000;

    private TestHttpServerClient() {
    }

    /**
     * Construct a suitable URL for loading a test data file from the hosts' HTTP server.
     *
     * @param path path relative to the document root.
     * @return an HTTP url.
     */
    public static String getUrl(String path) {
        return "http://localhost:" + SERVER_PORT + "/" + path;
    }

    /**
     * Construct a URL for loading a test data file with HTTP authentication fields.
     *
     * @param path path relative to the document root.
     * @return an HTTP url.
     */
    public static String getUrl(String path, String username, String password) {
        return "http://" + username + ":" + password + "@localhost:" + SERVER_PORT + "/" + path;
    }

    /**
     * Establishes a connection with the test server at default URL and verifies that it is running.
     */
    public static void checkServerIsUp() {
        checkServerIsUp(getUrl("chrome/test/data/android/ok.txt"), "OK Computer");
    }

    /**
     * Establishes a connection with the test server at a given URL and verifies that it is running
     * by making sure that the expected response is received.
     */
    public static void checkServerIsUp(String serverUrl, String expectedResponse) {
        InputStream is = null;
        try {
            URL testUrl = new URL(serverUrl);
            is = testUrl.openStream();
            byte[] buffer = new byte[128];
            int length = is.read(buffer);
            Assert.assertNotSame("Failed to access test HTTP Server at URL: " + serverUrl,
                    -1, expectedResponse.length());
            Assert.assertEquals("Failed to access test HTTP Server at URL: " + serverUrl,
                    expectedResponse, new String(buffer, 0, length).trim());
        } catch (MalformedURLException e) {
            Assert.fail(
                    "Failed to check test HTTP server at URL: " + serverUrl + ". Status: " + e);
        } catch (IOException e) {
            Assert.fail(
                    "Failed to check test HTTP server at URL: " + serverUrl + ". Status: " + e);
        } finally {
            StreamUtil.closeQuietly(is);
        }
    }
}
