// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

import java.io.File;
import java.util.HashMap;
import java.util.regex.Pattern;

/**
 * Tests for {@link HttpUrlRequestFactory}
 */
public class HttpUrlRequestFactoryTest extends CronetTestBase {
    // URL used for base tests.
    private static final String URL = "http://127.0.0.1:8000";

    @SmallTest
    @Feature({"Cronet"})
    public void testCreateFactory() throws Throwable {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        config.enableQUIC(true);
        config.addQuicHint("www.google.com", 443, 443);
        config.addQuicHint("www.youtube.com", 443, 443);
        config.setLibraryName("cronet_tests");
        String[] commandLineArgs = {CronetTestFramework.CONFIG_KEY, config.toString()};
        CronetTestFramework testFramework =
                startCronetTestFrameworkWithUrlAndCommandLineArgs(URL, commandLineArgs);
        HttpUrlRequestFactory factory = testFramework.mRequestFactory;
        assertNotNull("Factory should be created", factory);
        assertTrue("Factory should be Chromium/n.n.n.n@r but is "
                           + factory.getName(),
                   Pattern.matches("Chromium/\\d+\\.\\d+\\.\\d+\\.\\d+@\\w+",
                           factory.getName()));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testCreateLegacyFactory() {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        config.enableLegacyMode(true);

        HttpUrlRequestFactory factory = HttpUrlRequestFactory.createFactory(getContext(), config);
        assertNotNull("Factory should be created", factory);
        assertTrue("Factory should be HttpUrlConnection/n.n.n.n@r but is "
                           + factory.getName(),
                   Pattern.matches(
                           "HttpUrlConnection/\\d+\\.\\d+\\.\\d+\\.\\d+@\\w+",
                           factory.getName()));
        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HttpUrlRequest request = factory.createRequest(
                URL, 0, headers, listener);
        request.start();
        listener.blockForComplete();
        assertEquals(200, listener.mHttpStatusCode);
        assertEquals("OK", listener.mHttpStatusText);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testCreateLegacyFactoryUsingUrlRequestContextConfig() {
        UrlRequestContextConfig config = new UrlRequestContextConfig();
        config.enableLegacyMode(true);

        HttpUrlRequestFactory factory = HttpUrlRequestFactory.createFactory(getContext(), config);
        assertNotNull("Factory should be created", factory);
        assertTrue("Factory should be HttpUrlConnection/n.n.n.n@r but is "
                           + factory.getName(),
                   Pattern.matches(
                           "HttpUrlConnection/\\d+\\.\\d+\\.\\d+\\.\\d+@\\w+",
                           factory.getName()));
        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HttpUrlRequest request = factory.createRequest(
                URL, 0, headers, listener);
        request.start();
        listener.blockForComplete();
        assertEquals(200, listener.mHttpStatusCode);
        assertEquals("OK", listener.mHttpStatusText);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testQuicHintHost() {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        config.addQuicHint("www.google.com", 443, 443);
        try {
            config.addQuicHint("https://www.google.com", 443, 443);
        } catch (IllegalArgumentException e) {
            return;
        }
        fail("IllegalArgumentException must be thrown");
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testConfigUserAgent() throws Throwable {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";
        config.setUserAgent(userAgentValue);
        config.setLibraryName("cronet_tests");
        HttpUrlRequestFactory factory = HttpUrlRequestFactory.createFactory(getContext(), config);
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
        String url = NativeTestServer.getEchoHeaderURL(userAgentName);
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HashMap<String, String> headers = new HashMap<String, String>();
        HttpUrlRequest request = factory.createRequest(
                url, 0, headers, listener);
        request.start();
        listener.blockForComplete();
        assertEquals(userAgentValue, listener.mResponseAsString);
        NativeTestServer.shutdownNativeTestServer();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testConfigUserAgentLegacy() throws Throwable {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";
        config.setUserAgent(userAgentValue);
        config.enableLegacyMode(true);
        HttpUrlRequestFactory factory = HttpUrlRequestFactory.createFactory(getContext(), config);
        assertTrue("Factory should be HttpUrlConnection/n.n.n.n@r but is "
                           + factory.getName(),
                   Pattern.matches(
                           "HttpUrlConnection/\\d+\\.\\d+\\.\\d+\\.\\d+@\\w+",
                           factory.getName()));
        // Load test library for starting the native test server.
        System.loadLibrary("cronet_tests");

        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
        String url = NativeTestServer.getEchoHeaderURL(userAgentName);
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HashMap<String, String> headers = new HashMap<String, String>();
        HttpUrlRequest request = factory.createRequest(
                url, 0, headers, listener);
        request.start();
        listener.blockForComplete();
        assertEquals(userAgentValue, listener.mResponseAsString);
        NativeTestServer.shutdownNativeTestServer();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testEnableHttpCache() {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        config.enableHttpCache(HttpUrlRequestFactoryConfig.HTTP_CACHE_DISABLED, 0);
        config.enableHttpCache(HttpUrlRequestFactoryConfig.HTTP_CACHE_IN_MEMORY, 0);
        try {
            config.enableHttpCache(HttpUrlRequestFactoryConfig.HTTP_CACHE_DISK, 0);
            fail("IllegalArgumentException must be thrown");
        } catch (IllegalArgumentException e) {
            assertEquals("Storage path must be set", e.getMessage());
        }
        try {
            config.enableHttpCache(HttpUrlRequestFactoryConfig.HTTP_CACHE_DISK_NO_HTTP, 0);
            fail("IllegalArgumentException must be thrown");
        } catch (IllegalArgumentException e) {
            assertEquals("Storage path must be set", e.getMessage());
        }

        // Create a new directory to hold the disk cache data.
        File dir = getContext().getDir("disk_cache_dir", Context.MODE_PRIVATE);
        String path = dir.getPath();
        config.setStoragePath(path);
        config.enableHttpCache(HttpUrlRequestFactoryConfig.HTTP_CACHE_DISK, 100);
        config.enableHttpCache(HttpUrlRequestFactoryConfig.HTTP_CACHE_DISK_NO_HTTP, 100);
        try {
            config.enableHttpCache(HttpUrlRequestFactoryConfig.HTTP_CACHE_IN_MEMORY, 0);
            fail("IllegalArgumentException must be thrown");
        } catch (IllegalArgumentException e) {
            assertEquals("Storage path must be empty", e.getMessage());
        }
        assertTrue(dir.delete());
    }
}
